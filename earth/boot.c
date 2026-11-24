/*
启动初始化
准备硬件和内核运行环境，
把执行权交给Grass
*/

#include "process.h"

void tty_init();
void disk_init();
void mmu_init();
void intr_init(uint core_id);
void grass_entry(uint core_id);

struct grass* grass = (void*)GRASS_STRUCT;
struct earth* earth = (void*)EARTH_STRUCT;

struct rx_desc rx_desc[128] __attribute__((aligned(16))); // 128个描述符，记录缓冲区的位置和接收状态
char rx_buffer[128][2048] __attribute__((aligned(16)));  // 对应的128个缓冲区 receiving from Ethernet.

// 各核心先在 boot.s 抢锁再进入此函数。
void boot() {

    // 读取当前核心编号，运行平台固定为 QEMU。
    uint core_id;
    asm("csrr %0, mhartid" : "=r"(core_id));

    if (booted_core_cnt++ == 0) {
        // 抢到锁的基础来初始化
        tty_init();  //串口接口初始化
        CRITICAL("--- Booting on QEMU with core #%d ---", core_id);

        disk_init(); // 磁盘初始化
        SUCCESS("Finished initializing the tty and disk devices");

        mmu_init(); //内存管理与地址翻译初始化
        intr_init(core_id); // 中断处理初始化
        SUCCESS("Finished initializing the MMU, timer and interrupts");

        // 初始化显示设备
        {
            #define VGA_PCI_ECAM 0x30010000UL
            #define VGA_MMIO_BASE 0x40001000UL
            REGW(VGA_PCI_ECAM, 0x4) = 0;
            REGW(VGA_PCI_ECAM, 0x10) = VIDEO_FRAME_BASE;
            REGW(VGA_PCI_ECAM, 0x18) = VGA_MMIO_BASE;
            REGW(VGA_PCI_ECAM, 0x4) = (1 << 1) | (1 << 2);
            volatile ushort* vbe = (ushort*)(VGA_MMIO_BASE + 0x500);
            vbe[4] = 0x00;
            vbe[0] = 0xB0C5;
            vbe[1] = 800;
            vbe[2] = 600;
            vbe[3] = 32;
            vbe[5] = 0;
            vbe[6] = 800;
            vbe[7] = 600;
            vbe[8] = 0;
            vbe[9] = 0;
            vbe[4] = 0x41;
            volatile uchar* vga_io = (uchar*)(VGA_MMIO_BASE + 0x400);
            (void)vga_io[0x1A];
            vga_io[0x0] = 0x20;
            INFO("VGA ID readback: 0x%x", vbe[0]);
            memset((void*)VIDEO_FRAME_BASE, 0xFF, 800 * 600 * 4);
        }

        // 配置网卡
        REGW(ETH_PCI_ECAM, 0x4)  = 0;
        REGW(ETH_PCI_ECAM, 0x10) = ETH_CTL_BASE;
        REGW(ETH_PCI_ECAM, 0x4)  = (1 << 1) | (1 << 2); /* 开启 Memory Space 和 Bus Master */
        // 配置网卡在内存中的接收地址
        for(int i=0;i<128;i++)
        {
            rx_desc[i].addr_lo = (uint)rx_buffer[i];
            rx_desc[i].addr_hi = 0;
            rx_desc[i].status = 0;
        }
        // 配置网卡
        REGW(ETH_CTL_BASE, 0x2800) = (uint)rx_desc;
        REGW(ETH_CTL_BASE, 0x2804) = 0;
        REGW(ETH_CTL_BASE, 0x2808) = 128 * sizeof(struct rx_desc);
        REGW(ETH_CTL_BASE, 0x2810) = 0;
        REGW(ETH_CTL_BASE, 0x2818) = 127;
        REGW(ETH_CTL_BASE, 0xD0)   = (1 << 7);
        REGW(ETH_CTL_BASE, 0x100)  = (1 << 1) | (1 << 15) | (1 << 26);

        // Earth 完成主要底层初始化，把执行权交给 Grass
        // Grass 准备进程管理和 IPC 接口，加载并启动第一个系统服务进程 sys_proc
        grass_entry(core_id);
    } else {
        // 其它核心初始化
        SUCCESS("--- Core #%d starts running ---", core_id);

        // 配置 4GB内存权限和satp寄存器(用于页表翻译)
        asm("csrw pmpaddr0, %0" : : "r"(0x40000000));
        asm("csrw pmpcfg0, %0" : : "r"(0xF));
        earth->mmu_switch(1); // 1即GPID_PROCESS()

        // 初始化中断
        intr_init(core_id);
        //重置该核心定时器
        earth->timer_reset(core_id);
        //释放锁，由其它核心初始化
        release(boot_lock);
        // 放锁后不再用启动栈。打开中断，在这里等调度器接管。
        asm volatile("csrsi mstatus, 8\n1: wfi\nj 1b" ::: "memory");
        __builtin_unreachable();
    }
}
