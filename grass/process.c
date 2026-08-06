/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: helper functions for process management
 */

#include "process.h"

#define MLFQ_NLEVELS          5
#define MLFQ_RESET_PERIOD     100000000         /* 10 seconds */
#define MLFQ_LEVEL_RUNTIME(x) (x + 1) * 1000000 /* e.g., 100ms for level 0 */
extern struct process proc_set[MAX_NPROCESS + 1];

static void proc_set_status(int pid, enum proc_status status) {
    if (pid <= 0) return;
    for (uint i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].status != PROC_UNUSED && proc_set[i].pid == pid) proc_set[i].status = status;
}

void proc_set_ready(int pid) { proc_set_status(pid, PROC_READY); }
void proc_set_running(int pid) { proc_set_status(pid, PROC_RUNNING); }
void proc_set_runnable(int pid) { proc_set_status(pid, PROC_RUNNABLE); }
void proc_set_pending(int pid) { proc_set_status(pid, PROC_PENDING_SYSCALL); }

int proc_alloc() {
    static uint curr_pid = 0;
    for (uint i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].status == PROC_UNUSED) {
            proc_set[i].pid    = ++curr_pid;
            proc_set[i].status = PROC_LOADING;
            /* Student's code goes here (Multiple Projects). */

            /* [Preemptive Scheduling]
             * Initialize the fields for lifecycle statistics and MLFQ.
             * [System Call & Protection]
             * Initialize the fields for the process sleep system call. */
            proc_set[i].cpu_time=0;
            proc_set[i].start_time=mtime_get();
            proc_set[i].last_run_time=0;
            proc_set[i].time_intr_time=0;
            proc_set[i].first_time=0;

            proc_set[i].level = 0;
            proc_set[i].level_time_left = MLFQ_LEVEL_RUNTIME(0);

            proc_set[i].sleep_until = 0;
            /* Student's code ends here. */
            return curr_pid;
        }

    FATAL("proc_alloc: reach the limit of %d processes", MAX_NPROCESS);
}

void proc_free(int pid) {
    /* Student's code goes here (Preemptive Scheduling). */
    /* Print the lifecycle statistics of the terminated process or processes. */
    if (pid != GPID_ALL) { // pid==GPID_ALL==-1时杀死全部进程
        for(uint i=0;i<MAX_NPROCESS;i++)
        {
            if(proc_set[i].pid==pid)  // 这里pid创建时会一直++
            {
                ulonglong turnaround = mtime_get()-proc_set[i].start_time;
                ulonglong response = proc_set[i].first_time-proc_set[i].start_time;
                INFO("process %d terminated after %d timer interrupts, turnaround time: %dms, response time: %dms, CPU time: %dms",
                pid,proc_set[i].time_intr_time,(uint)(turnaround/10000),(uint)(response/10000),(uint)(proc_set[i].cpu_time/10000));
                break;
            }
        }
        earth->mmu_free(pid);
        proc_set_status(pid, PROC_UNUSED);
    } else {
        /* Free all user processes. */
        for (uint i = 0; i < MAX_NPROCESS; i++)
            if (proc_set[i].pid >= GPID_USER_START && //小于 GPID_USER_START的是系统核心进程
                proc_set[i].status != PROC_UNUSED) {
                ulonglong turnaround = mtime_get()-proc_set[i].start_time;
                ulonglong response = proc_set[i].first_time-proc_set[i].start_time;
                INFO("process %d terminated after %d timer interrupts, turnaround time: %dms, response time: %dms, CPU time: %dms",
                proc_set[i].pid,proc_set[i].time_intr_time,(uint)(turnaround/10000),(uint)(response/10000),(uint)(proc_set[i].cpu_time/10000));
                earth->mmu_free(proc_set[i].pid);
                proc_set[i].status = PROC_UNUSED;
            }
    }
    /* Student's code ends here. */
}

void mlfq_update_level(struct process* p, ulonglong runtime) {
    /* Student's code goes here (Preemptive Scheduling). */

    /* Update the MLFQ-related fields in struct process* p after this
     * process has run on the CPU for another ulonglong runtime time units. */

    if((p->level_time_left-=runtime)<=0)
    {
        if(p->level<MLFQ_NLEVELS-1)
            ++p->level;
        p->level_time_left = MLFQ_LEVEL_RUNTIME(p->level); //写在循环外面，level=4时也要重置时间的
    }
    /* Student's code ends here. */
}

void mlfq_reset_level() {
    /* Student's code goes here (Preemptive Scheduling). */
    if (!earth->tty_input_empty()) {
        /* Reset the level of GPID_SHELL if there is pending keyboard input. */
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

    static ulonglong MLFQ_last_reset_time = 0;
    /* Reset the level of all processes every MLFQ_RESET_PERIOD time units. */
    if(mtime_get()-MLFQ_last_reset_time>=MLFQ_RESET_PERIOD)
    {
        for(int i=0;i<MAX_NPROCESS;i++)
        {
            proc_set[i].level = 0;
            proc_set[i].level_time_left = MLFQ_LEVEL_RUNTIME(0);
        }
        MLFQ_last_reset_time = mtime_get();
    }
    /* Student's code ends here. */
}

void proc_sleep(int pid, uint usec) {
    /* Student's code goes here (System Call & Protection). */

    /* Update the sleep-related fields in the struct process for process pid. */
    for(int i=0;i<MAX_NPROCESS;i++)
    {
        if(proc_set[i].pid==pid)
        {
            proc_set[i].sleep_until = mtime_get() + (ulonglong)usec*10;
            break;
        }
    }

    /* Student's code ends here. */
}

void proc_coresinfo() {
    /* Student's code goes here (Multicore & Locks). */
    INFO("==============Core ID / Process ID==============");
    for(int i=0;i<NCORES;i++)
        INFO("Core #%d is running pid=%d",i,proc_set[core_to_proc_idx[i]].pid);
    /* Print out the pid of the process running on each CPU core. */

    /* Student's code ends here. */
}
