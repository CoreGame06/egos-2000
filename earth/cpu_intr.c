/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: wrapping the CPU interface for interrupts
 * Initialize the trap entry, enable interrupts, and reset the timer.
 */

#include "egos.h"

#define MTIME_BASE    (CLINT_BASE + 0xBFF8)
#define MTIMECMP_BASE (CLINT_BASE + 0x4000)
#define QUANTUM       (earth->platform == QEMU ? 100000UL : 50000000UL)

ulonglong mtime_get() {
    uint low, high;
    do {
        high = REGW(MTIME_BASE, 4);
        low  = REGW(MTIME_BASE, 0); 
    } while (REGW(MTIME_BASE, 4) != high); // 用 do while 防止进位

    return (((ulonglong)high) << 32) | low; 
}

static void mtimecmp_set(ulonglong time, uint core_id) {
    REGW(MTIMECMP_BASE, core_id * 8 + 4) = 0xFFFFFFFF;
    REGW(MTIMECMP_BASE, core_id * 8 + 0) = (uint)time;
    REGW(MTIMECMP_BASE, core_id * 8 + 4) = (uint)(time >> 32);
}

static void timer_reset(uint core_id) {
    mtimecmp_set(mtime_get() + QUANTUM, core_id);
}

void trap_entry(); /* See grass/kernel.s */
void intr_init(uint core_id) {
    /* Initialize the timer. */
    earth->timer_reset = timer_reset;
    mtimecmp_set(0x0FFFFFFFFFFFFFFFUL, core_id);

    /* Set up the interrupt/exception handling entry. */
    asm("csrw mtvec, %0" ::"r"(trap_entry));
    INFO("Use direct mode and put the address of the trap_entry into mtvec");

    /* Enable timer interrupt. */
    asm("csrw mip, %0" ::"r"(0));
    asm("csrs mie, %0" ::"r"(0x80));
    asm("csrs mstatus, %0" ::"r"(0x88));

    /* Student's code goes here (Ethernet & TCP/IP). */
    // 开启CPU的外部中断使能（mie控制中断的寄存器）
    asm("csrs mie, %0"::"r"(1<<11));
    //在PLIC中将网卡的IRQ设置为1
    REGW(PLIC_PRIORITY(ETH_IRQ),0) = 1;
    /* 3. 在 PLIC 中为 core_id 开启 ETH_IRQ 的中断监听 */
    REGW(PLIC_ENABLE_BASE(core_id) + (ETH_IRQ / 32) * 4, 0) |= (1 << (ETH_IRQ % 32));
    /* Enable external interrupt. Find the IRQ number corresponding to the
     * Ethernet controller device in PLIC, enable external interrupts from
     * this IRQ number, and then set the priority of this IRQ number to 1. */

    /* Student's code ends here. */
}
