/*
磁盘块读写
QEMU 把 tools/disk.img 当作一张 SD 卡，通过 SDHCI 控制器供操作系统访问。
这里负责“从第几个块开始，读写多少块”，每块 512 字节；文件名、目录由上层文件系统处理。

读：SD 卡 -> 控制器通过 DMA 写入中转缓冲区 -> CPU 用 memcpy 拷到调用者的 dst。
写：调用者的 src -> CPU 用 memcpy 拷到中转缓冲区 -> 控制器通过 DMA 读出并写入 SD 卡。
DMA 就是由设备搬运数据；CPU 仍需设置地址、发送命令，并等待设备状态。
*/

#include "egos.h"
#include "disk.h"
#include <string.h>


#define SDHCI_DMA_ADDRESS      0x00 // DMA 要访问的内存地址。
#define SDHCI_BLK_CNT_AND_SIZE 0x04 // 本次传输的块数、每块字节数。
#define SDHCI_ARGUMENT         0x08 // 命令参数，例如读写起始位置。
#define SDHCI_CMD_AND_MODE     0x0C // 命令号，以及读/写、DMA 等传输设置。
#define SDHCI_RESPONSE0        0x10 // 读取 SD 卡返回的响应。
#define SDHCI_PRESENT_STATE    0x24 // 控制器当前是否忙碌。
#define SDHCI_CLKCON           0x2C // 时钟开关。
#define SDHCI_SOFTWARE_RESET   0x2F // 复位控制器。
#define SDHCI_INT_STAT         0x30 // 记录命令完成、传输完成等状态。
#define SDHCI_INT_STAT_ENABLE  0x34 // 允许记录哪些状态。
#define SDHCI_INT_SIG_ENABLE   0x38 // 允许哪些状态向 CPU 发出中断。
#define PAGE_SIZE 4096

// 向磁盘控制器下达一条命令，然后等它报告命令完成
// 和磁盘的读写函数配合，他们负责配置参数，这个负责下命令
static char sdhci_exec_cmd(uint idx, uint arg, uchar flag, uint mode) {
    while (REGW(SDHCI_BASE, SDHCI_PRESENT_STATE) & 0x3);
    REGW(SDHCI_BASE, SDHCI_INT_STAT) = 0xFFFFFFFF;

    REGW(SDHCI_BASE, SDHCI_ARGUMENT)     = arg;
    REGW(SDHCI_BASE, SDHCI_CMD_AND_MODE) = (((idx << 8) | flag) << 16) | mode;

    while (!(REGW(SDHCI_BASE, SDHCI_INT_STAT) & 0x1));
}

// 函数使用 DMA，把 SD 卡的数据读到内存里
// SD 卡 ──设备通过 DMA──→ aligned_buf ──CPU 执行 memcpy──→ dst
static void sdhci_read_multi(uint offset, uint cnt, char* dst) 
{

    static __attribute__((aligned(PAGE_SIZE))) char aligned_buf[PAGE_SIZE];
    REGW(SDHCI_BASE, SDHCI_DMA_ADDRESS)      = (uint)aligned_buf;
    REGW(SDHCI_BASE, SDHCI_BLK_CNT_AND_SIZE) = (cnt << 16) | BLOCK_SIZE;   
    #define DATA_PRESENT_FLAG (1 << 5)
    #define MULTI_READ_MODE   ((1 << 5) | (1 << 4) | (1 << 2) | (1 << 1) | (1 << 0))
    offset *= BLOCK_SIZE;
    sdhci_exec_cmd(18, offset, DATA_PRESENT_FLAG,MULTI_READ_MODE);
    memcpy(dst, aligned_buf, cnt*BLOCK_SIZE);
}

// 初始化控制器和 SD 卡，让它们能够接收后面的读写命令。
static int sdhci_init() {
#define PCI_ECAM_ALLOW_MMIO_AND_DMA ((1 << 1) | (1 << 2))

    REGW(SDHCI_PCI_ECAM, 0x4)  = PCI_ECAM_ALLOW_MMIO_AND_DMA;
    REGW(SDHCI_PCI_ECAM, 0x10) = SDHCI_BASE;

    REGB(SDHCI_BASE, SDHCI_SOFTWARE_RESET) = 0x1;
    while (REGB(SDHCI_BASE, SDHCI_SOFTWARE_RESET) & 0x1);
    REGB(SDHCI_BASE, SDHCI_CLKCON) = 0x5;

    REGW(SDHCI_BASE, SDHCI_INT_SIG_ENABLE)  = 0x0;
    REGW(SDHCI_BASE, SDHCI_INT_STAT_ENABLE) = 0x27F003B;

    sdhci_exec_cmd(55, 0, 0, 0);             
    sdhci_exec_cmd(41, 0xFFF0000, 0, 0);     
    sdhci_exec_cmd(2, 0, 0, 0);             
    sdhci_exec_cmd(3, 0, 2 /* get response */, 0); 
    sdhci_exec_cmd(7, REGW(SDHCI_BASE, SDHCI_RESPONSE0), 0, 0); 
}

// 多个核心共用同一个控制器和中转缓冲区，整个读写过程都要加锁。
static int disk_lock = 0;

// 对外提供的读接口：从整盘 block_no 块开始读 nblocks 块，放到 dst。
// 例如 disk_read(300, 10, buf)：读第 300~309 块，分成 8 块和 2 块两批。
void disk_read(uint block_no, uint nblocks, char* dst) {
    acquire(disk_lock);
    while (nblocks > 0) {
        // 中转缓冲区只有一页，每批最多读 8 块，剩余不足 8 块就只读剩余部分。
        uint chunk = nblocks > 8 ? 8 : nblocks;
        sdhci_read_multi(block_no, chunk, dst);

        // 下一批从后面的磁盘块继续，写入 dst 后面的空间。
        block_no += chunk;
        nblocks -= chunk;
        dst += chunk * BLOCK_SIZE;
    }
    release(disk_lock);
}

// 写入一批块：offset 是整盘起始块号，cnt 为 1~8，src 指向要写入的数据。
static void sdhci_write_multi(uint offset,uint cnt,char * src)
{
    static __attribute__((aligned(PAGE_SIZE))) char aligned_buf[PAGE_SIZE];
    memcpy(aligned_buf,src, cnt*BLOCK_SIZE);
    REGW(SDHCI_BASE, SDHCI_DMA_ADDRESS)      = (uint)aligned_buf;
    REGW(SDHCI_BASE, SDHCI_BLK_CNT_AND_SIZE) = (cnt << 16) | BLOCK_SIZE;   
    #define DATA_PRESENT_FLAG (1 << 5)
    #define MULTI_WRITE_MODE   ((1 << 5) |  (1 << 2) | (1 << 1) | (1 << 0))
    offset *= BLOCK_SIZE;
    sdhci_exec_cmd(25, offset, DATA_PRESENT_FLAG, MULTI_WRITE_MODE);
}

// 对外提供的写接口：把 src 中的 nblocks*512 字节写到整盘 block_no 块开始的位置。
// 与 disk_read 一样，每批最多 8 块，逐批推进磁盘块号和内存地址。
void disk_write(uint block_no, uint nblocks, char* src) {
    acquire(disk_lock);
    while (nblocks > 0) {
        uint chunk = nblocks > 8 ? 8 : nblocks;
        sdhci_write_multi(block_no, chunk, src);

        block_no += chunk;
        nblocks -= chunk;
        src += chunk * BLOCK_SIZE;
    }
    release(disk_lock);
}

static void disk_test()
{
    // 启动时验证块读写：备份第 300~302 块 -> 写测试数据 -> 读回比对 -> 恢复原内容。
    // 测试正常结束才会恢复；如果中途失败或退出，备份不会自动写回。
    INFO("Running disk read/write test...");

    #define TEST_BLOCK_NO 300
    #define TEST_NBLOCKS 3
    char buf_orig[TEST_NBLOCKS * BLOCK_SIZE]; // 保存磁盘原内容，供最后恢复。
    char buf_test[TEST_NBLOCKS * BLOCK_SIZE]; // 准备写入的测试数据。
    char buf_read[TEST_NBLOCKS * BLOCK_SIZE]; // 保存读回的数据，用来比对。

    // 把测试数据的每个字节都设成 0xAB，便于检查读回结果是否一致。
    memset(buf_test,0xAB,sizeof(buf_test));

    // 备份
    disk_read(TEST_BLOCK_NO,TEST_NBLOCKS,buf_orig);
    // 写入
    disk_write(TEST_BLOCK_NO,TEST_NBLOCKS,buf_test);
    // 读取
    disk_read(TEST_BLOCK_NO,TEST_NBLOCKS,buf_read);
    // memcmp 返回 0 表示内容一致；否则报告失败并停止。
    if((memcmp(buf_test,buf_read,sizeof(buf_test))!=0))
        FATAL("Disk test failed: Read data does not match written data!");
    
    // 还原
    disk_write(TEST_BLOCK_NO,TEST_NBLOCKS,buf_orig);
    SUCCESS("Disk multi-block read/write test passed!");
}
// boot() 在启动阶段调用这里，使 Earth 的磁盘读写接口可用。
void disk_init() {
    // 1. 保存函数地址，供启动加载器、文件服务等通过 earth->disk_read/write 调用。
    earth->disk_read  = disk_read;
    earth->disk_write = disk_write;
    // 2. 初始化设备；3. 读写自测，检查当前驱动是否能正常工作。
    sdhci_init();
    disk_test();
}
