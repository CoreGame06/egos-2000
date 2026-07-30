#pragma once

#include "egos.h"
#include "syscall.h"

enum proc_status {
    PROC_UNUSED,
    PROC_LOADING,
    PROC_READY,
    PROC_RUNNING,
    PROC_RUNNABLE,
    PROC_PENDING_SYSCALL
};

struct process {
    int pid;
    struct syscall syscall;
    enum proc_status status;
    uint mepc, saved_registers[32];
    /* Student's code goes here (Preemptive Scheduling | System Call). */

    /* Add new fields for lifecycle statistics, MLFQ, or process sleep. */
    ulonglong start_time;
    ulonglong first_time; // 第一次被调用的时刻
    ulonglong last_run_time; //用于计算累积时刻，当前时刻减去这一时刻
    ulonglong cpu_time; //当前进程总的运行时间
    uint time_intr_time; //中断次数
    
    // 设置优先级
    int level;
    ulonglong level_time_left;
    /* Student's code ends here. */
};
#define MAX_NPROCESS 16

ulonglong mtime_get();

int proc_alloc();
void proc_free(int);
void proc_set_ready(int);
void proc_set_running(int);
void proc_set_runnable(int);
void proc_set_pending(int);

void mlfq_reset_level();
void mlfq_update_level(struct process* p, ulonglong runtime);
void proc_sleep(int pid, uint usec);
void proc_coresinfo();

extern uint core_to_proc_idx[NCORES];
