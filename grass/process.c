/*
进程记录的管理
创建时从proc_set找一个空闲的进程记录，填写 PID 和初始状态；
退出时回收进程内存，再把这个记录标为空闲
运行过程中还会调整优先级、记录睡眠的目标时刻。
这里主要修改进程的管理信息，挑选下一个运行谁、切换执行现场在 kernel.c/kernel.s 中完成。
*/

#include "process.h"

#define MLFQ_NLEVELS          5                // 共 5 级，level=0 最高，level=4 最低。
#define MLFQ_RESET_PERIOD     100000000         // 每隔 10 秒，把所有记录的优先级提升到最高。
#define MLFQ_LEVEL_RUNTIME(x) (x + 1) * 1000000 // 第 x 级累计运行配额：100/200/300/400/500ms。

// 数组定义在 kernel.c：前 16 项存放进程/线程，最后一项用于核心空闲时占位。
extern struct process proc_set[MAX_NPROCESS + 1];

// 按 PID 修改状态。数组位置会复用，所以要查找 PID，不能直接用 PID 当数组下标。
// 同一进程的多个线程可以共享 PID，这里会修改所有匹配且未释放的记录。
static void proc_set_status(int pid, enum proc_status status) {
    if (pid <= 0) return; // PID 0 是空闲占位；这个辅助函数不处理特殊值 -1。
    for (uint i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].status != PROC_UNUSED && proc_set[i].pid == pid) proc_set[i].status = status;
}

// 这四个函数共用上面的查找逻辑，只是设置的目标状态不同；它们本身不会切换 CPU。
void proc_set_ready(int pid) { proc_set_status(pid, PROC_READY); }
void proc_set_running(int pid) { proc_set_status(pid, PROC_RUNNING); }
void proc_set_runnable(int pid) { proc_set_status(pid, PROC_RUNNABLE); }
void proc_set_pending(int pid) { proc_set_status(pid, PROC_PENDING_SYSCALL); }

// 为一个新进程申请 PCB，返回新 PID；程序加载、页表和栈的准备由 elf_load 完成。
// 本函数内没有加锁；系统服务可直接调用它，不能认为所有 PCB 操作都已受内核锁保护。
int proc_alloc() {
    // static 让编号在多次调用之间保留；记录位置可以复用，PID 继续递增。
    static uint curr_pid = 0;
    for (uint i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].status == PROC_UNUSED) {
            // 1. 找到空闲记录，登记新 PID，并标记“正在准备”，暂时不能被调度。
            proc_set[i].pid    = ++curr_pid;
            proc_set[i].status = PROC_LOADING;

            // 2. 初始化运行统计。创建时刻现在就能记录，首次运行时刻要等调度器选中后再填。
            proc_set[i].cpu_time=0;
            proc_set[i].start_time=mtime_get();
            proc_set[i].last_run_time=0;
            proc_set[i].time_intr_time=0;
            proc_set[i].first_time=0;

            // 3. 新进程从最高优先级开始，先获得第 0 级的 100ms 累计配额。
            proc_set[i].level = 0;
            proc_set[i].level_time_left = MLFQ_LEVEL_RUNTIME(0);

            proc_set[i].sleep_until = 0; // 初始没有睡眠限制。
            return curr_pid; // 返回进程编号，不是这次找到的数组下标 i。
        }

    // 所有记录都在使用，当前实现直接报告失败，不等待其它进程退出。
    FATAL("proc_alloc: reach the limit of %d processes", MAX_NPROCESS);
}

// 回收一个指定 PID；如果传入 GPID_ALL（-1），则回收全部用户进程，保留系统服务。
// 主线：打印统计 -> 让 Earth 回收物理页（包括页表）-> 把 PCB 标为空闲，供以后复用。
void proc_free(int pid) {
    if (pid != GPID_ALL) { // 分支一：回收一个指定 PID。
        for(uint i=0;i<MAX_NPROCESS;i++)
        {
            if(proc_set[i].pid==pid)
            {
                // 周转时间：从创建到退出经过多久（包括等待、睡眠等时间）。
                ulonglong turnaround = mtime_get()-proc_set[i].start_time;
                // 响应时间：创建后等了多久才第一次运行。
                ulonglong response = proc_set[i].first_time-proc_set[i].start_time;
                // 除以 10000，把 mtime 计数换成毫秒；CPU 时间取调度时累加的 cpu_time。
                INFO("process %d terminated after %d timer interrupts, turnaround time: %dms, response time: %dms, CPU time: %dms",
                pid,proc_set[i].time_intr_time,(uint)(turnaround/10000),(uint)(response/10000),(uint)(proc_set[i].cpu_time/10000));
                break; // 这里只打印第一个匹配记录的统计，没有汇总同 PID 的多个线程。
            }
        }
        // 按 PID 回收内存；再释放所有同 PID 的有效记录。
        // 这里只修改 PCB 的状态，没有把整个结构清零；后续分配时会重新初始化相应字段。
        earth->mmu_free(pid);
        proc_set_status(pid, PROC_UNUSED);
    } else {
        // 分支二：遍历所有有效用户记录，逐个打印统计并回收。
        // PID 1~4 是常驻系统服务；GPID_USER_START=5，所以不会清理它们。
        for (uint i = 0; i < MAX_NPROCESS; i++)
            if (proc_set[i].pid >= GPID_USER_START &&
                proc_set[i].status != PROC_UNUSED) {
                ulonglong turnaround = mtime_get()-proc_set[i].start_time;
                ulonglong response = proc_set[i].first_time-proc_set[i].start_time;
                INFO("process %d terminated after %d timer interrupts, turnaround time: %dms, response time: %dms, CPU time: %dms",
                proc_set[i].pid,proc_set[i].time_intr_time,(uint)(turnaround/10000),(uint)(response/10000),(uint)(proc_set[i].cpu_time/10000));
                earth->mmu_free(proc_set[i].pid);
                proc_set[i].status = PROC_UNUSED;
            }
    }
}

// 根据本次运行时长扣减配额；配额用完就降低优先级，使其它可运行进程更容易得到 CPU。
// p 指向要更新的 PCB，runtime 是这次已经运行的时长，与配额都使用 mtime 计数单位。
void mlfq_update_level(struct process* p, ulonglong runtime) {

    // 先比较再扣减：配额是无符号数，避免无符号数的相减溢出
    if(p->level_time_left<=runtime)
    {
        // 配额已经用完，降低一级；如果已在最低的第 4 级，就保持第 4 级。
        if(p->level<MLFQ_NLEVELS-1) ++p->level;
        // 重新获得当前级别的完整配额，例如从第 0 级降到第 1 级后获得 200ms。
        // 当前实现每次最多降一级
        p->level_time_left = MLFQ_LEVEL_RUNTIME(p->level);
    }
    else
    {
        // 配额还没用完，只扣减本次消耗，保持原来的级别。例如剩 70ms，运行 20ms 后还剩 50ms。
        p->level_time_left -= runtime;
    }
}

// 按两种条件恢复高优先级：有输入时优先照顾 Shell；
// 每隔 10 秒整给所有进程恢复到最高优先级
void mlfq_reset_level() {
    // 1. tty_input_empty() 返回 0 表示有输入，加 ! 后为真。
    if (!earth->tty_input_empty()) {
        // 用户开始输入命令了，把 Shell 恢复到最高优先级并补满配额，提高交互响应速度。
        for(int i=0;i<MAX_NPROCESS;i++)
        {
            if(proc_set[i].pid == GPID_SHELL && proc_set[i].status != PROC_UNUSED)
            {
                proc_set[i].level =0;
                proc_set[i].level_time_left = MLFQ_LEVEL_RUNTIME(0);
                break;
            }
        }

    }

    // 2. 记住上次整体提升的时刻；static 让这个值在下一次调用时仍然保留。
    static ulonglong MLFQ_last_reset_time = 0;
    // 如果距离上次已经过去 10 秒，就整体提升，避免低优先级进程长时间等不到 CPU。
    if(mtime_get()-MLFQ_last_reset_time>=MLFQ_RESET_PERIOD)
    {
        for(int i=0;i<MAX_NPROCESS;i++)
        {
            proc_set[i].level = 0;
            proc_set[i].level_time_left = MLFQ_LEVEL_RUNTIME(0);
        }
        MLFQ_last_reset_time = mtime_get(); // 本轮完成后，从现在开始计算下一个 10 秒间隔。
    }
}

// 让 pid 对应的进程睡眠 usec 微秒：只登记“到什么时刻才允许再调度”。
void proc_sleep(int pid, uint usec) {

    for(int i=0;i<MAX_NPROCESS;i++)
    {
        if(proc_set[i].pid==pid)
        {
            proc_set[i].sleep_until = mtime_get() + (ulonglong)usec*10;
            break; 
        }
    }

}

// 显示各个核心正在运行哪个 PID，便于观察多核调度情况。
void proc_coresinfo() {
    INFO("==============Core ID / Process ID==============");
    // 先用核心编号取得 PCB 数组下标，再从该记录取得 PID；核心空闲时读到占位记录的 PID 0。
    for(int i=0;i<NCORES;i++)
        INFO("Core #%d is running pid=%d",i,proc_set[core_to_proc_idx[i]].pid);

}


/*
通过遍历 proc_set 查找 PID：
1. PID 随创建持续递增，而数组位置会在进程退出后复用，PID 与下标没有固定对应关系。
2. 同一进程的多个线程共享 PID，但各占一个数组位置，一个 PID 可能匹配多条记录。
    proc_set[5].pid = 5,proc_set[9].pid = 5 二者是一个进程下的线程
当前最多只有 16 条有效记录，遍历开销有限，暂不额外维护 PID 索引，保持实现简单


*/