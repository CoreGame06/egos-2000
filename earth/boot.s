    .section .text.enter
    .global boot_loader, boot_lock, booted_core_cnt

boot_loader:
    /* P0/P8: 四个核心从同一入口开始，用原子交换竞争 boot_lock。
     * t0=锁地址，t1=拟写入的 1；amoswap 返回旧值，旧值为 0 才表示抢锁成功。
     * .aq 限制后续内存访问越过获得锁的操作；先持锁才可使用启动栈。
     * 首核直到 sys_proc 换到应用栈并释放启动锁后，才让其他核依次进入。
     */
    la t0, boot_lock          /* Load the address of boot_lock. */
    li t1, 1
    amoswap.w.aq t1, t1, (t0) /* Acquire boot_lock. */
    bnez t1, boot_loader
    la sp, boot_stack_top   /* 启动栈与中断处理用的内核栈分开。 */
    call boot

.bss
    boot_lock:       .word 0
    booted_core_cnt: .word 0

    .balign 16
    .space 16384
boot_stack_top:
