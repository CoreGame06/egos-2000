/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: bootloader
 * Initialize the tty device, disk device, MMU, and CPU interrupts.
 */

#include "process.h"

void tty_init();
void disk_init();
void mmu_init();
void intr_init(uint core_id);
void grass_entry(uint core_id);

struct grass* grass = (void*)GRASS_STRUCT;
struct earth* earth = (void*)EARTH_STRUCT;

/* Student's code goes here (Ethernet & TCP/IP). */
struct rx_desc rx_desc[128] __attribute__((aligned(16)));
char rx_buffer[128][2048] __attribute__((aligned(16)));
/* Define an array of 128 RX descriptors and a buffer of 128*2048 bytes. Every
 * descriptor corresponds to a 2048-byte RX buffer for receiving from Ethernet.
 */

/* Student's code ends here. */

void boot() {
    uint core_id, vendor_id;
    asm("csrr %0, mhartid" : "=r"(core_id));
    asm("csrr %0, mvendorid" : "=r"(vendor_id));
    earth->platform = (vendor_id == 666) ? HARDWARE : QEMU;

    if (booted_core_cnt++ == 0) {
        /* The first booted core needs to do some more work. */
        tty_init();
        CRITICAL("--- Booting on %s with core #%d ---",
                 earth->platform == HARDWARE ? "Hardware" : "QEMU", core_id);

        disk_init();
        SUCCESS("Finished initializing the tty and disk devices");

        mmu_init();
        intr_init(core_id);
        SUCCESS("Finished initializing the MMU, timer and interrupts");

        /* Student's code goes here (I/O Device Driver). */

        /* Initialize QEMU's standard VGA device for apps/user/video_demo.c.
         * Start with https://www.qemu.org/docs/master/specs/standard-vga.html,
         * and you could ask AI/LLMs about the Bochs Dispi (Display Interface).
         * Your driver should setup the PCI ECAM for VGA, and then set the VGA
         * screen resolution to 800*600 pixels, each using 4 bytes for its RGB
         * information. Lastly, initialize all the pixels with white color. */
        if(earth->platform==QEMU)
        {
            #define VGA_PCI_ECAM 0x30010000UL  // 配置地址
            #define VGA_MMIO_BASE 0x40001000UL  // 控制地址

            // 1. 先禁用设备的 Memory Space，准备修改 BAR (标准 PCI 流程)
            REGW(VGA_PCI_ECAM, 0x4) = 0;
            
            /* 2. 配置 BAR0 (显存起始物理地址) 与 BAR2 (MMIO 寄存器物理地址) */
            REGW(VGA_PCI_ECAM, 0x10) = VIDEO_FRAME_BASE; // 0x41000000(把哪里的数据显示到显存上)
            REGW(VGA_PCI_ECAM, 0x18) = VGA_MMIO_BASE;    // 0x40001000(配置 MMIO)
            
            // 配置完 BAR 后，设置权限：开启 Memory Space (bit 1) 和 Bus Master (bit 2)
            REGW(VGA_PCI_ECAM, 0x4) = (1 << 1) | (1 << 2);

            /* 3. 使用 16 位指针操作位于 VGA_MMIO_BASE + 0x500 的 Bochs VBE 寄存器 */
            // 配置具体的显示规则
            volatile ushort* vbe = (ushort*)(VGA_MMIO_BASE + 0x500);
        
            vbe[4] = 0x00; // Index 4: 先禁用 VBE，准备修改参数
            vbe[0] = 0xB0C5; // Index 0: 设置 Bochs VBE ID，非常重要
            vbe[1] = 800;  // Index 1: 水平分辨率 800
            vbe[2] = 600;  // Index 2: 垂直分辨率 600
            vbe[3] = 32;   // Index 3: 32 位色彩深度 (BPP)
            vbe[5] = 0;    // Index 5: 银行(Bank)，LFB模式下必须为0
            vbe[6] = 800;  // Index 6: 虚拟宽度 (必须显式指定)
            vbe[7] = 600;  // Index 7: 虚拟高度 (必须显式指定)
            vbe[8] = 0;    // Index 8: X 偏移量
            vbe[9] = 0;    // Index 9: Y 偏移量
            vbe[4] = 0x41; // Index 4: 开启 VBE (0x01) 并开启线性显存 LFB (0x40)

            /* Unblank the screen using VGA IOports (0x400-0x41f are mapped to 0x3c0-0x3df) */
            volatile uchar* vga_io = (uchar*)(VGA_MMIO_BASE + 0x400);
            (void)vga_io[0x1A]; // Read 0x41A (maps to 0x3DA) to reset the ATC flip-flop
            vga_io[0x0] = 0x20;        // Write 0x20 to 0x400 (maps to 0x3C0) to unblank the screen

            INFO("VGA ID readback: 0x%x", vbe[0]);
            
            /* 4. 将显存清屏为白色 (800 * 600 * 4 字节，全填 0xFF) */
            memset((void*)VIDEO_FRAME_BASE, 0xFF, 800 * 600 * 4);   
        }
        /* Student's code ends here. */

        /* Student's code goes here (Ethernet & TCP/IP). */
        if (earth->platform == QEMU) {
            REGW(ETH_PCI_ECAM, 0x4)  = 0;
            REGW(ETH_PCI_ECAM, 0x10) = ETH_CTL_BASE;
            REGW(ETH_PCI_ECAM, 0x4)  = (1 << 1) | (1 << 2); /* 开启 Memory Space 和 Bus Master */
        }

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
        /* Use ETH_CTL_BASE as the BAR0 in Ethernet's PCIe configuration. Enable
         * the Ethernet controller's RXT0 (Receiver Timer Interrupt). Initialize
         * the MAC address register and the RX descriptor registers. Ensure that
         * every RX descriptor's address field is written with its corresponding
         * RX buffer's address. Read the descriptions for RCTL (Receive Control)
         * and decide which bits should be set to 1. */

        /* Student's code ends here. */

        grass_entry(core_id);
    } else {
        SUCCESS("--- Core #%d starts running ---", core_id);

        /* Student's code goes here (Multicore & Locks). */
        if(earth->translation == SOFT_TLB)
            FATAL("Multicore requires page table translation");

        // 配置 4GB内存权限和satp寄存器
        asm("csrw pmpaddr0, %0" : : "r"(0x40000000));
        asm("csrw pmpcfg0, %0" : : "r"(0xF));
        earth->mmu_switch(1); // 1即GPID_PROCESS()

        // 初始化中断
        intr_init(core_id);
        //重置定时器
        earth->timer_reset(core_id);
        //释放锁
        release(boot_lock);
        // 挂起等待
        asm("wfi");
        /* Refer to mmu_init() and intr_init(), and decide how to initialize
         * the CSRs for virtual memory and interrupts on this CPU core. */

        /* Reset the timer, release the boot lock, and run the wfi instruction.
         * After the next timer interrupt, this CPU core will enter the kernel,
         * and the kernel could schedule a process to run on this CPU core. */

        /* Student's code ends here. */
    }
}
