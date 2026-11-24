/*
这只中断和定时器
让每个核心知道：发生中断后去哪里处理、允许接收哪些中断。
提供读取当前时间、设置下一次定时器触发时间的接口。
定时器到期后，CPU 进入 Grass 的处理入口，再由 Grass 决定运行哪个进程。
*/

#include "egos.h"

// 硬件时钟 mtime 的地址，多个核心共用这个不断增加的计数。
#define MTIME_BASE    (CLINT_BASE + 0xBFF8)
// 每个核心各有一个 mtimecmp，记录自己的定时器应在什么时刻触发。
#define MTIMECMP_BASE (CLINT_BASE + 0x4000)
// 距离下一次时钟中断的间隔，单位为时钟计数；QEMU 下 100000 次计数约为 10 毫秒。
#define QUANTUM       100000UL

// 读取当前硬件时间
// 时间有 64 位，而当前 CPU 是 32 位，所以需要分开读取高、低两部分。
ulonglong mtime_get() {
    uint low, high;
    do {
        high = REGW(MTIME_BASE, 4);
        low  = REGW(MTIME_BASE, 0); 
    } while (REGW(MTIME_BASE, 4) != high); // 高位变了说明读取期间发生进位，需要重新读取。

    // 把高、低两部分拼成一个完整的 64 位时间值。
    return (((ulonglong)high) << 32) | low; 
}

// 设置某个核心的定时器：time 是目标时刻，core_id 是核心编号。
// 例如当前时间为 1000，设置 time=1300，表示再经过 300 次计数后触发。
// 当硬件时钟 mtime >= 该核心的 mtimecmp 时，就会产生定时器中断。
static void mtimecmp_set(ulonglong time, uint core_id) {
    // 每个目标时间占 8 字节，用 core_id * 8 找到对应核心的那一项。
    // 先把高位临时写成最大值，再写低位和真实高位，避免只更新一半时误触发。
    REGW(MTIMECMP_BASE, core_id * 8 + 4) = 0xFFFFFFFF;
    REGW(MTIMECMP_BASE, core_id * 8 + 0) = (uint)time;
    REGW(MTIMECMP_BASE, core_id * 8 + 4) = (uint)(time >> 32);
}

// 给 core_id 对应的核心重新设置下一次触发时间。
static void timer_reset(uint core_id) {
    mtimecmp_set(mtime_get() + QUANTUM, core_id);
}

// 实现在 grass/kernel.s：CPU 进入后保存执行现场，再调用 Grass 的 C 代码。
void trap_entry();

// 初始化当前核心 core_id 的中断配置
void intr_init(uint core_id) {
    // 把函数地址放进 Earth 接口，供 Grass 调用 earth->timer_reset(core_id)。
    earth->timer_reset = timer_reset;
    // 暂时把触发时刻设得很远，避免初始化期间立刻被定时器打断。
    // 后面准备开始运行或等待任务时，再调用 timer_reset 设置正常的触发间隔。
    mtimecmp_set(0x0FFFFFFFFFFFFFFFUL, core_id);

    // mtvec 保存处理入口地址：发生中断或异常后，CPU 从 trap_entry 开始执行。
    // 这里只登记入口，并没有立即调用 trap_entry。
    asm("csrw mtvec, %0" ::"r"(trap_entry));
    INFO("Use direct mode and put the address of the trap_entry into mtvec");

    // mip 记录待处理的中断； mie 控制允许哪些中断
    asm("csrw mip, %0" ::"r"(0));
    asm("csrs mie, %0" ::"r"(0x80));
    // bit7 让首次 mret 恢复中断开关。辅助核心在释放启动锁后再打开 bit3。
    asm volatile("csrs mstatus, %0" :: "r"(0x80));

    // 下面依次：允许外部中断、设置网卡中断优先级、允许送到当前核心。
    asm("csrs mie, %0"::"r"(1<<11));
    REGW(PLIC_PRIORITY(ETH_IRQ),0) = 1;
    REGW(PLIC_ENABLE_BASE(core_id) + (ETH_IRQ / 32) * 4, 0) |= (1 << (ETH_IRQ % 32));
}
