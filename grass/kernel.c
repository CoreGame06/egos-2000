/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: kernel ≈ 2 handlers
 *   intr_entry() handles timer and device interrupts.
 *   excp_entry() handles system calls and faults (e.g., invalid memory access).
 */

#include "process.h"
#include <string.h>

uint core_in_kernel;
uint core_to_proc_idx[NCORES];
struct process proc_set[MAX_NPROCESS + 1];
/* proc_set[MAX_NPROCESS] is a placeholder for idle cores. */

#define curr_proc_idx core_to_proc_idx[core_in_kernel]
#define curr_pid      proc_set[curr_proc_idx].pid
#define curr_status   proc_set[curr_proc_idx].status
#define curr_saved    proc_set[curr_proc_idx].saved_registers

static void intr_entry(uint);
static void excp_entry(uint);

void kernel_entry() {
    /* With the kernel lock, only one core can enter this point at any time. */
    asm("csrr %0, mhartid" : "=r"(core_in_kernel));

    /* Save the process context. */
    asm("csrr %0, mepc" : "=r"(proc_set[curr_proc_idx].mepc));
    memcpy(curr_saved, (void*)(EGOS_STACK_TOP - 32 * 4), 32 * 4);

    uint mcause;
    asm("csrr %0, mcause" : "=r"(mcause));
    (mcause & (1 << 31)) ? intr_entry(mcause & 0x3FF) : excp_entry(mcause);

    /* Restore the process context. */
    asm("csrw mepc, %0" ::"r"(proc_set[curr_proc_idx].mepc));
    memcpy((void*)(EGOS_STACK_TOP - 32 * 4), curr_saved, 32 * 4);
}

#define INTR_ID_TIMER   7
#define EXCP_ID_ECALL_U 8
#define EXCP_ID_ECALL_M 11
static void proc_yield();
static void proc_try_syscall(struct process* proc);

static void excp_entry(uint id) {
    if (id >= EXCP_ID_ECALL_U && id <= EXCP_ID_ECALL_M) {
        /* Copy the system call arguments from user space to the kernel. */
        uint syscall_paddr = earth->mmu_translate(curr_pid, SYSCALL_ARG);
        memcpy(&proc_set[curr_proc_idx].syscall, (void*)syscall_paddr,
               sizeof(struct syscall));
        proc_set[curr_proc_idx].syscall.status = PENDING;

        proc_set_pending(curr_pid);
        proc_set[curr_proc_idx].mepc += 4;
        proc_try_syscall(&proc_set[curr_proc_idx]);
        proc_yield();
        return;
    }
    /* Student's code goes here (System Call & Protection | Virtual Memory). */

    /* Kill the current process if curr_pid is a user application. */
    if(curr_pid>=GPID_USER_START)
    {
        INFO("process %d terminated with exception %d",curr_pid,id);
        // 这里不可以直接free，通过sys_proc来退出
        // 发消息给 sys_proc
        struct proc_request *req= (struct proc_request*)proc_set[curr_proc_idx].syscall.content;
        req->type = PROC_EXIT;

        proc_set[curr_proc_idx].syscall.type = SYS_SEND;
        proc_set[curr_proc_idx].syscall.receiver= GPID_PROCESS;
        proc_set[curr_proc_idx].syscall.status= PENDING;

        //投递
        proc_set_pending(curr_pid);
        proc_try_syscall(&proc_set[curr_proc_idx]);

        proc_yield();
        return;
    }

    /* Student's code ends here. */
    FATAL("excp_entry: kernel got exception %d", id);
}

static void intr_entry(uint id) {
    /* Student's code goes here (Preemptive Scheduling). */

    /* Update the process lifecycle statistics. */

    /* Student's code ends here. */

    if (id == INTR_ID_TIMER) 
    {
        ++proc_set[curr_proc_idx].time_intr_time;
        return proc_yield();
    }
    /* Student's code goes here (Ethernet & TCP/IP). */

    if(id == 11)
    {
        // 读取是谁触发了中断
        uint irq = REGW(PLIC_CAUSE(0),0);

        if(irq == ETH_IRQ)
        {
            /* 读取 E1000 的 ICR 寄存器 (0xC0) 清除网卡中断标志 */
            REGW(ETH_CTL_BASE, 0xC0);

            static int rx_head = 0;
            while(rx_desc[rx_head].status & 1)
            {
                ushort len = rx_desc[rx_head].length;
                rx_buffer[rx_head][len] = '\0'; // 添加字符串结束符 \0

                uchar* pkt = (uchar*)rx_buffer[rx_head];
                ushort ethertype = (pkt[12] << 8) | pkt[13];
                uchar proto = pkt[23];

                /* 打印所有收到的数据包信息以供调试 */
                INFO("Got pkt: rxdesc[%d], len=%d, ethertype=0x%x, proto=%d", rx_head, len, ethertype, (uint)proto);

                /* 只针对 IPv4 (0x0800) 且为 UDP (协议号 17) 的数据包打印 SUCCESS */
                if (ethertype == 0x0800 && proto == 17) {
                    char* msg = &rx_buffer[rx_head][42]; // 偏移量 42 为 UDP 消息体
                    SUCCESS("Get %d bytes from rxdesc[%d]: %s", len, rx_head, msg);
                }

                rx_desc[rx_head].status = 0;
                REGW(ETH_CTL_BASE, 0x2818) = rx_head;
                rx_head = (rx_head + 1) % 128;
            }
        }
        REGW(PLIC_CAUSE(0), 0) = irq; //写回irq，表示处理完了
        return proc_yield();
    }
    /* Handle an external interrupt from the Intel Gigabit Ethernet Controller.
     * Specifically, you need to (1) Claim the PLIC interrupt; (2) Check if the
     * interrupt is for receiving an Ethernet frame; (3) Read the received frame
     * from an RX buffer and print the content; (4) Complete the PLIC interrupt,
     * so PLIC can fire the next interrupt; (5) Call proc_yield() and return. */

    /* Student's code ends here. */
}

static void proc_yield() {
    if (curr_status == PROC_RUNNING) proc_set_runnable(curr_pid);

    /* Student's code goes here (Multiple Projects). */

    /* [Preemptive Scheduling]
     * Measure and record lifecycle statistics for the *current* process.
     * Modify the loop below to find the next process to schedule with MLFQ.
     * [System Call & Protection]
     * Do not schedule a process that should still be sleeping at this time. */
    ulonglong now = mtime_get();
    ulonglong runtime = now - proc_set[curr_proc_idx].last_run_time;
    proc_set[curr_proc_idx].cpu_time += runtime;
    mlfq_update_level(&proc_set[curr_proc_idx],runtime);
    mlfq_reset_level();

    int next_idx = MAX_NPROCESS;
    int min_level = 5;
    for (uint i = 1; i <= MAX_NPROCESS; i++) {
        struct process* p = &proc_set[(curr_proc_idx + i) % MAX_NPROCESS];

        // 睡眠拦截
        if(p->sleep_until>0) 
        {
            if(mtime_get()<p->sleep_until)
                continue;
            else
                p->sleep_until = 0; //时间到，清零
        }
        if (p->status == PROC_PENDING_SYSCALL) proc_try_syscall(p);

        if ((p->status == PROC_READY || p->status == PROC_RUNNABLE) && (p->level<min_level)) {
            next_idx = (curr_proc_idx + i) % MAX_NPROCESS;
            min_level = p->level;
        }
    }

    if (next_idx < MAX_NPROCESS) {
        /* [Preemptive Scheduling]
         * Measure and record lifecycle statistics for the *next* process.
         * [System Call & Protection | Multicore & Locks]
         * Modify mstatus.MPP to enter machine or user mode after mret. */
        if(proc_set[next_idx].first_time==0)
            proc_set[next_idx].first_time = mtime_get();
        proc_set[next_idx].last_run_time = mtime_get();

        // 将mstatus的11和12位清零
        asm("csrc mstatus, %0" :: "r"(3<<11));
        // 如果是内核进程，且没有开启页表，则设置为特权模式 (M-Mode)
        if(proc_set[next_idx].pid < GPID_USER_START && earth->translation == SOFT_TLB)
            asm("csrs mstatus, %0" :: "r"(3<<11));
    } else {
        /* [Multicore & Locks]
         * Release the kernel lock.
         * [Multicore & Locks | System Call & Protection]
         * Set curr_proc_idx to MAX_NPROCESS; Reset the timer;
         * Enable interrupts by setting the mstatus.MIE bit to 1;
         * Wait for the next interrupt using the wfi instruction. */

        uint has_active_process =0;
        for(uint i=0;i<MAX_NPROCESS;i++)
        {
            if(proc_set[i].status != PROC_UNUSED)
            {
                has_active_process =1;
                break;
            }
        }
        if(!has_active_process)
            FATAL("proc_yield: no process to run on core %d", core_in_kernel);
        
        // 1. 将当前正在运行的进程设为空闲占位符 MAX_NPROCESS
        curr_proc_idx = MAX_NPROCESS;
        // 2. 告诉定时器硬件：重置闹钟，开启新的一轮倒计时
        earth->timer_reset(core_in_kernel);
        // 3. 打开全局中断开关 (修改 mstatus 寄存器中的 MIE 位为 1)
        // 这意味着只要硬件中断到来，CPU 就会马上响应
        asm("csrs mstatus, %0" ::"r"(1 << 3));
        // 4. 让 CPU 挂起休眠，不消耗指令周期，直到收到下一个中断为止
        asm("wfi");
    }
    /* Student's code ends here. */

    curr_proc_idx = next_idx;
    earth->mmu_switch(curr_pid);
    earth->mmu_flush_cache();
    if (curr_status == PROC_READY) {
        /* Set up the argc, argv, and initial program
         * counter for a newly created process. */
        curr_saved[0]                = APPS_ARG;
        curr_saved[1]                = APPS_ARG + 4;
        proc_set[curr_proc_idx].mepc = APPS_ENTRY;
    }
    proc_set_running(curr_pid);
    earth->timer_reset(core_in_kernel);
}

static void proc_try_send(struct process* sender) {
    for (uint i = 0; i < MAX_NPROCESS; i++) {
        struct process* dst = &proc_set[i];
        if (dst->pid == sender->syscall.receiver &&
            dst->status != PROC_UNUSED) {
            /* Return if process dst is not receiving messages or
             * is not taking messages from the sender process. */
            if (!(dst->syscall.type == SYS_RECV &&
                  dst->syscall.status == PENDING))
                return;
            if (!(dst->syscall.sender == GPID_ALL ||
                  dst->syscall.sender == sender->pid))
                return;

            dst->syscall.status = DONE;
            dst->syscall.sender = sender->pid;
            /* Copy the system call arguments within the kernel PCB. */
            memcpy(dst->syscall.content, sender->syscall.content,
                   SYSCALL_MSG_LEN);
            return;
        }
    }
    FATAL("proc_try_send: unknown receiver pid=%d", sender->syscall.receiver);
}

static void proc_try_recv(struct process* receiver) {
    if (receiver->syscall.status == PENDING) return;

    /* Copy the system call struct from the kernel back to user space. */
    uint syscall_paddr = earth->mmu_translate(receiver->pid, SYSCALL_ARG);
    memcpy((void*)syscall_paddr, &receiver->syscall, sizeof(struct syscall));

    /* Set the receiver and sender back to RUNNABLE. */
    proc_set_runnable(receiver->pid);
    proc_set_runnable(receiver->syscall.sender);
}

static void proc_try_syscall(struct process* proc) {
    switch (proc->syscall.type) {
    case SYS_RECV:
        proc_try_recv(proc);
        break;
    case SYS_SEND:
        proc_try_send(proc);
        break;
    default:
        FATAL("proc_try_syscall: unknown syscall type=%d", proc->syscall.type);
    }
}
