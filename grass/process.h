/*
PCB 进程控制块
这些记录放在 kernel.c 的 proc_set 数组里，process.c 和 kernel.c 负责操作它们。
*/

#pragma once

#include "egos.h"
#include "syscall.h"

// 普通新进程的状态变化：空闲记录 -> 正在准备 -> 等待首次运行 -> 正在运行。
// 运行中被切走后，如果没有阻塞，就变成 RUNNABLE，等待以后从原位置继续执行。
enum proc_status {
    PROC_UNUSED,          // 这个数组位置空闲，可以用来登记新进程。
    PROC_LOADING,         // 已占用这个位置，正在准备程序和内存，尚不能调度。
    PROC_READY,           // 准备完毕，等待首次运行；调度时还要设置程序入口和初始参数。
    PROC_RUNNING,         // 当前正在某个 CPU 核心上运行。
    PROC_RUNNABLE,        // 已有可用的执行现场，等待调度时恢复；运行过的进程被切走后通常用此状态。
    PROC_PENDING_SYSCALL  // 系统调用尚未完成，例如正在等消息，暂时不能恢复用户代码执行。
};

// 本项目的线程也使用这个结构：同一进程的多个线程共享 PID，但各占一个数组位置。
struct process {
    int pid;                    
    struct syscall syscall;     // 保存这次系统调用的请求/消息，便于进程暂停期间由内核继续处理。
    enum proc_status status;    

    // 进程被中断或切走时，保存“执行到哪里”和“各寄存器的值”
    // mepc 是下次执行的指令地址；saved_registers 是寄存器值的备份
    uint mepc, saved_registers[32];

    ulonglong start_time;    // 创建时刻，用于计算从创建到退出总共经过多久。
    ulonglong first_time;    // 第一次被调度执行的时刻；0 表示还没记录，用于计算首次等待时间。
    ulonglong last_run_time; // 最近一次开始运行的时刻，切走时用“当前时刻 - 它”计算本次运行时长。
    ulonglong cpu_time;      // 把每次运行时长累加；当前实现也计入部分内核处理时间。
    uint time_intr_time;     // 运行期间遇到的定时器中断次数，用来观察被时钟打断了多少次。
    
    // 调度优先级：运行消耗的时间会影响优先级，具体规则在 process.c 中。
    int level;                  // 0 最高、4 最低；调度器优先选择数字较小的可运行记录。
    ulonglong level_time_left;   // 当前级别还允许累计运行多久，用完后降低优先级（已最低则不再降低）。

    // 用于处理进程的睡眠，这一时刻到了算醒来，可以被调度
    ulonglong sleep_until;       // 允许再次调度的目标时刻，单位同 mtime；0 表示没有睡眠限制。
};

// 最多 16 个有效记录；系统服务、普通进程和线程都占用这些位置。
#define MAX_NPROCESS 16

// 读取当前硬件时间，实现在 earth/cpu_intr.c，返回 mtime 计数值。
ulonglong mtime_get();

// 申请一个空闲记录并返回新 PID
int proc_alloc();
// 回收指定 PID 的内存并释放对应记录；传入 GPID_ALL（-1）时回收全部用户进程。
void proc_free(int);

// 参数都是 PID：把对应记录设成等待首次运行、正在运行、可以继续、系统调用未完成。
void proc_set_ready(int);
void proc_set_running(int);
void proc_set_runnable(int);
void proc_set_pending(int);

// MLFQ 调度策略。
// 按条件提升 Shell 或全体记录的优先级，避免交互迟缓或低优先级进程长时间等不到运行。
void mlfq_reset_level();
// p 指向要更新的记录，runtime 是它本次已运行的时长（mtime 计数），用于扣减本级配额。
// 根据进程这次运行了多久，扣减它当前优先级的运行配额；配额用完，就降低优先级
void mlfq_update_level(struct process* p, ulonglong runtime);
// 让 pid 睡眠 usec 微秒：记录目标时刻，后续由调度器跳过尚未到时的记录。
void proc_sleep(int pid, uint usec);
// 打印各个 CPU 核心当前运行的 PID。
void proc_coresinfo();

// 记录每个核心当前使用 proc_set 中的哪一项，保存的是数组下标，不是 PID。
// 例如 core_to_proc_idx[2]=5，表示核心 2 当前使用 proc_set[5]。
// 值为 MAX_NPROCESS（16）表示该核心空闲，对应额外准备的占位记录，PID 为 0。
extern uint core_to_proc_idx[NCORES];
