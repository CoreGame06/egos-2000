/*
中断和异常的汇编入口
cpu_intr.c 已把 trap_entry 的地址登记给 CPU；中断、异常或 ecall 发生时，CPU 会跳到这里。
CPU 会自动记录执行位置和事件原因，但通用寄存器的值需要由下面的代码保存。
主线：加锁 -> 保存现场 -> 调用 kernel.c 处理事件和调度 -> 恢复所选线程的现场 -> mret。
*/

    .section .text
    .global trap_entry, kernel_lock

trap_entry:
    /* 1. 获取内核锁。*/
    csrw sscratch,t0
    csrw mscratch,t1
.Lacquire_kernel_lock:

    la t0,kernel_lock
    li t1,1
    amoswap.w.aq t1,t1,(t0)
    bnez t1, .Lacquire_kernel_lock

    csrr t1,mscratch
    csrr t0,sscratch

    /* 2. 把线程原来的栈指针存进 专用 mscratch，
        然后切换到内核栈处理中断 */
    csrw mscratch, sp
    li sp, 0x80200000

    /* 3. 在内核栈顶部留出 128 字节，用于保存中断的现场上下文 */
    addi sp, sp, -128 /* sp = EGOS_STACK_TOP - 128，向低地址腾出保存区。 */
    /* a0~a7：通常用于函数参数和返回值。 */
    sw a0,  0(sp)
    sw a1,  4(sp)
    sw a2,  8(sp)
    sw a3,  12(sp)
    sw a4,  16(sp)
    sw a5,  20(sp)
    sw a6,  24(sp)
    sw a7,  28(sp)
    /* t0~t6：临时寄存器。线程可能在任意指令处被打断，所以也必须保存。 */
    sw t0,  32(sp)
    sw t1,  36(sp)
    sw t2,  40(sp)
    sw t3,  44(sp)
    sw t4,  48(sp)
    sw t5,  52(sp)
    sw t6,  56(sp)
    /* s0~s11：用于跨函数调用保留数据的寄存器。 */
    sw s0,  60(sp)
    sw s1,  64(sp)
    sw s2,  68(sp)
    sw s3,  72(sp)
    sw s4,  76(sp)
    sw s5,  80(sp)
    sw s6,  84(sp)
    sw s7,  88(sp)
    sw s8,  92(sp)
    sw s9,  96(sp)
    sw s10, 100(sp)
    sw s11, 104(sp)
    /* ra：函数返回地址；gp/tp：全局数据、线程相关的基址寄存器。 */
    sw ra,  108(sp)
    sw gp,  112(sp)
    sw tp,  116(sp)
    /* 当前 sp 已经是内核栈地址，要从 mscratch 取回线程原来的 sp 再保存。
     * t0 的原值已经存到偏移 32 的位置，现在可以借用它。 */
    csrr t0, mscratch
    sw t0,  120(sp)   /* 对应 saved_registers[30]，即线程栈指针。 */

    /* 4. 进入 kernel.c：把现场存进 PCB，处理事件，选择接下来运行的线程。
     * 返回前，C 代码会把所选线程的寄存器值复制回这 128 字节，并设置 mepc。 */
    call kernel_entry

    /* 5. 从保存区恢复所选线程的寄存器。
     * 如果调度器从 A 切到了 B，下面装入的就是 B 的值；也可能继续执行 A。 */
    lw a0,  0(sp)
    lw a1,  4(sp)
    lw a2,  8(sp)
    lw a3,  12(sp)
    lw a4,  16(sp)
    lw a5,  20(sp)
    lw a6,  24(sp)
    lw a7,  28(sp)
    lw t0,  32(sp)
    lw t1,  36(sp)
    lw t2,  40(sp)
    lw t3,  44(sp)
    lw t4,  48(sp)
    lw t5,  52(sp)
    lw t6,  56(sp)
    lw s0,  60(sp)
    lw s1,  64(sp)
    lw s2,  68(sp)
    lw s3,  72(sp)
    lw s4,  76(sp)
    lw s5,  80(sp)
    lw s6,  84(sp)
    lw s7,  88(sp)
    lw s8,  92(sp)
    lw s9,  96(sp)
    lw s10, 100(sp)
    lw s11, 104(sp)
    lw ra,  108(sp)
    lw gp,  112(sp)
    lw tp,  116(sp)

    /* 6. 最后恢复所选线程的栈指针。之后不再使用这个内核栈保存区。 */
    lw sp,  120(sp)

    /* 7. 释放内核锁，让其它核心可以进入。*/
    csrw sscratch,t0
    la t0,kernel_lock
    sw zero,0(t0)       /* 当前用普通写 0 解锁，未额外加入释放内存屏障。 */
    csrr t0,sscratch

    /* 8. 跳到 mepc 指定的位置，继续执行下一个代码。 */
    mret

.bss
    /* 内核锁初始为 0（空闲），所有核心共享这一份。 */
    kernel_lock:     .word 0
