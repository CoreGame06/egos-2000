/* 公共头文件，定义 Earth/Grass 接口、内存和设备地址，以及多核相关工具。 */

#pragma once

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long long ulonglong;


// Earth 接口负责内存、时钟和设备读写。
struct earth {
    uint (*mmu_alloc)(); // 返回物理页池下标，页地址 = APPS_PAGES_BASE + 下标 × 4096。
    void (*mmu_free)(int pid); // 回收指定用户进程占用的物理页。
    void (*mmu_flush_cache)(); // 刷新当前核心的 TLB。
    void (*timer_reset)(uint core_id); // 设置指定核心的下一次时钟中断。

    // 在 pid 的页表中把虚拟页号 vpage_no 映射到页池下标 ppage_id。
    void (*mmu_map)(int pid, uint vpage_no, uint ppage_id);
    uint (*mmu_translate)(int pid, uint vaddr); // 查询指定进程的虚拟地址对应的物理地址。
    void (*mmu_switch)(int pid); // 让当前核心使用指定进程的页表。

    // 串口按字符读写，输入为空时 tty_input_empty 返回 1。
    void (*tty_read)(char* c);
    void (*tty_write)(char c);
    uint (*tty_input_empty)();
    // 从整盘第 block_no 块起读写 nblocks 块，每块 512 字节。
    void (*disk_read)(uint block_no, uint nblocks, char* dst);
    void (*disk_write)(uint block_no, uint nblocks, char* src);

};

// Grass 接口负责进程管理和消息收发。
struct grass {
    int (*proc_alloc)(); // 找一个空闲的 PCB，返回新 PID。
    void (*proc_free)(int pid);
    void (*proc_set_ready)(int pid); // 标记该进程可以首次被调度。

    // 消息参数见 syscall/syscall.h。
    void (*sys_send)(int receiver, char* msg, uint size);
    void (*sys_recv)(int from, int* sender, char* buf, uint size);
    void (*proc_sleep)(int pid,uint sleep_time); // 根据睡眠时长登记唤醒时刻，sleep_time 以微秒计。
    void (*proc_coresinfo)(); // 显示各核心正在运行的 PID。
};

// 通过指针调用已登记的接口。普通函数调用本身不会切换 CPU 权限。
extern struct earth* earth;
extern struct grass* grass;

// 内存布局。进程入口、参数页和栈顶使用固定虚拟地址，由各进程的页表映射到自己的物理页。
#define RAM_END         0x80600000UL // 本项目管理的 RAM 上界，不包含该地址。
#define APPS_PAGES_BASE 0x80400000UL // 物理页池起点，到 RAM_END 共 2 MiB。
#define APPS_STACK_TOP  0x80400000UL // 应用虚拟栈顶，初始映射 2 页，向低地址增长。
#define SYSCALL_ARG     0x80301000UL // 系统调用参数页。
#define APPS_ARG        0x80300000UL // main 的参数页，存放 argc 和 argv。
#define APPS_ENTRY      0x80200000UL // 应用入口的虚拟地址。
#define EGOS_STACK_TOP  0x80200000UL // 内核栈顶。
#define SHELL_WORK_DIR  0x80102000UL // Shell 和 cd 共用的工作目录页。
#define GRASS_STRUCT    0x80101000UL // Grass 接口表地址。
#define EARTH_STRUCT    0x80100000UL // Earth 接口表地址。
#define RAM_START       0x80000000UL // RAM 起点，内核代码和数据从这里开始。

// QEMU virt 上使用的设备地址。网卡寄存器与显存目前使用同一地址，同时启用会冲突。
#define SDHCI_PCI_ECAM   0x30008000UL // 磁盘控制器的 PCI 配置区。
#define SDHCI_BASE       0x40000000UL // 磁盘控制器寄存器。
#define PLIC_BASE        0x0C000000UL // 外设中断控制器。
#define ETH_PCI_ECAM     0x30018000UL // 网卡的 PCI 配置区。
#define ETH_CTL_BASE     0x41000000UL // 网卡寄存器。
#define UART_BASE        0x10000000UL // 串口寄存器。
#define CLINT_BASE       0x02000000UL // 定时器等核心本地中断寄存器。
#define FLASH_ROM_BASE   0x22000000UL // Flash 镜像地址。
#define VIDEO_FRAME_BASE 0x41000000UL // 显存地址。

// offset 是字节偏移。REGW 访问 32 位，REGB 访问 8 位，volatile 让编译器保留设备读写。
#define ACCESS(x)          (*(__typeof__(*x) volatile*)(x))
#define REGW(base, offset) (ACCESS((uint*)(base + offset)))
#define REGB(base, offset) (ACCESS((uchar*)(base + offset)))

// 核数要与 QEMU 的 -smp 一致。acquire 用原子交换反复尝试，release 负责解锁。
#define NCORES     4
#define release(x) __sync_lock_release(&x);
#define acquire(x) while (__sync_lock_test_and_set(&x, 1) != 0);
// boot_lock 让各核心依次启动，kernel_lock 保护共享内核栈和调度数据。
extern int boot_lock, kernel_lock, booted_core_cnt;

// 项目自己的打印函数，实现在 libc/print.c。
#define printf my_printf
int INFO(const char* format, ...);
int FATAL(const char* format, ...);
int SUCCESS(const char* format, ...);
int CRITICAL(const char* format, ...);
int my_printf(const char* format, ...);

// 网卡中断号与 PLIC 寄存器地址。PLIC_CAUSE 可读出待处理中断号，处理后写回同一个编号。
#define ETH_IRQ 35
#define PLIC_PRIORITY(irq)     (PLIC_BASE + 0x0000 + 4 * (irq))
#define PLIC_ENABLE_BASE(core) (PLIC_BASE + 0x2000 + 0x80 * (core))
#define PLIC_CAUSE(core)       (PLIC_BASE + 0x200004 + 0x1000 * (core))
// 每个描述符对应一个接收缓冲区。网卡通过 DMA 写入数据，再填写长度和接收状态。
 struct rx_desc{
    uint addr_lo; // 缓冲区物理地址的低 32 位。
    uint addr_hi; // 缓冲区物理地址的高 32 位。
    ushort length; // 收到的字节数。
    ushort checksum;
    uchar status; // 最低位为 1 表示这一项已接收完成。
    uchar errors;
    ushort special;
} __attribute__((packed)); // 去掉字段间的填充，保持网卡要求的 16 字节格式。

// 启动时设置接收缓冲区，网卡中断处理时读取。
extern struct rx_desc rx_desc[128];
extern char rx_buffer[128][2048];
