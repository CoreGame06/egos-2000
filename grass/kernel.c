/*
内核的事件处理和调度
中断、异常、系统调用先进入 kernel.s，保存寄存器后调用这里的 kernel_entry()。
主线：把当前线程的现场存入 PCB -> 处理这次事件 -> 必要时选择下一线程 -> 准备恢复它。
这里的 proc_set 每项是一个可调度记录，同一进程的多个线程可以占用不同项、共享 PID。

建议阅读顺序：kernel_entry -> excp_entry/intr_entry -> proc_yield -> 消息收发和线程创建。
*/

#include "process.h"
#include <string.h>

// 正在执行这段内核代码的核心编号；kernel.s 已加锁，所以同一时刻只有一个核心使用它。
uint core_in_kernel;
// 核心编号 -> PCB 数组下标，就是该核心运行的任务
uint core_to_proc_idx[NCORES] = {[0 ... NCORES-1] = MAX_NPROCESS};
// 前 16 项记录进程/线程，第 17 项（下标 16）保存核心空闲时的现场，避免覆盖用户记录。
// 核心没有线程可运行时，也可能被时钟中断打断，而进入内核后仍会保存执行现场
// 所以 第 17 项”是给空闲核心准备的占位记录“
struct process proc_set[MAX_NPROCESS + 1];

// 当前核心对应的 PCB、PID、状态和寄存器备份。这些是宏，每次使用时都会重新取值。
// 修改 curr_proc_idx 就是修改核心与 PCB 的对应关系，之后其它三个宏也随之指向新记录。
#define curr_proc_idx core_to_proc_idx[core_in_kernel]
#define curr_pid      proc_set[curr_proc_idx].pid
#define curr_status   proc_set[curr_proc_idx].status
#define curr_saved    proc_set[curr_proc_idx].saved_registers

static void intr_entry(uint);
static void excp_entry(uint);

// kernel.s 调用的 C 入口；正常返回后，再由汇编恢复寄存器并执行 mret。
void kernel_entry() {
    // 1. 获取当前核心编号，找到它刚才运行的 PCB。
    asm("csrr %0, mhartid" : "=r"(core_in_kernel));

    // 2. 把被打断线程的 PC 和寄存器值保存到它的 PCB
    asm("csrr %0, mepc" : "=r"(proc_set[curr_proc_idx].mepc));
    memcpy(curr_saved, (void*)(EGOS_STACK_TOP - 32 * 4), 32 * 4);

    // 3. mcause 记录进入内核的原因：最高位为 1 是中断，为 0 是异常（ecall 也属于异常）。
    // 处理这次事件，并可能选择下一个运行的线程。
    uint mcause;
    asm("csrr %0, mcause" : "=r"(mcause));
    // 中断时取低位原因号交给 intr_entry；异常则交给 excp_entry。
    (mcause & (1 << 31)) ? intr_entry(mcause & 0x3FF) : excp_entry(mcause);

    // 4. 从接下来要运行的线程的 PCB 中，取出 PC 和寄存器值，准备恢复执行。
    // PC 写入 CPU 的 mepc 寄存器：
    // 用寄存器值先复制到内核栈顶部预留的 128 字节保存区,再把这些值装进真正的 CPU 寄存器
    asm("csrw mepc, %0" ::"r"(proc_set[curr_proc_idx].mepc));
    memcpy((void*)(EGOS_STACK_TOP - 32 * 4), curr_saved, 32 * 4);
}

#define INTR_ID_TIMER   7  // 时钟中断。
#define EXCP_ID_ECALL_U 8  // 用户模式执行 ecall，主动请求内核服务。
#define EXCP_ID_ECALL_M 11 // 机器模式执行 ecall。
#define INTR_ID_EXTERNAL 11 // 外设中断

static void proc_yield();
static void proc_try_syscall(struct process* proc);

// 处理异常：主动 ecall 就执行系统调用；普通应用出错就请求 sys_proc 回收它。
static void excp_entry(uint id) {
    // 分支一：系统调用。本项目把 8~11 归入这个分支
    if (id >= EXCP_ID_ECALL_U && id <= EXCP_ID_ECALL_M) {
        // 用户代码已把请求填在 SYSCALL_ARG 参数页，再通过 ecall 进入这里。

        // 每个进程都把请求信息写到自己虚拟地址空间中的 SYSCALL_ARG 位置。
        // 这里根据当前 PID 查页表，找到“这个进程的请求，实际放在内存的哪个位置（物理内存）”。
        uint syscall_paddr = earth->mmu_translate(curr_pid, SYSCALL_ARG);
        // 把找到的请求复制到当前线程的 PCB 中保存，供内核处理。
        memcpy(&proc_set[curr_proc_idx].syscall, (void*)syscall_paddr,
               sizeof(struct syscall));
        proc_set[curr_proc_idx].syscall.status = PENDING;

        // 只修改当前这条记录，表示请求还没处理完，暂时不能继续执行用户代码。
        proc_set[curr_proc_idx].status = PROC_PENDING_SYSCALL;

        proc_set[curr_proc_idx].mepc += 4;
        // 先尝试处理请求；如果现在不能完成，保持等待，之后调度扫描时会再次尝试。
        proc_try_syscall(&proc_set[curr_proc_idx]);
        // 重新选择谁运行：可能继续当前线程，也可能让另一个线程运行。
        proc_yield();
        return;
    }

    // 分支二：普通应用发生其它异常，例如写入不允许访问的地址。
    if(curr_pid>=GPID_USER_START)
    {
        INFO("process %d terminated with exception %d",curr_pid,id);
        // 构造一条发给 sys_proc 的“进程退出”请求，交给它回收内存、通知 Shell。
        // content 是消息内容区，把它按 proc_request 的结构来填写。
        struct proc_request *req= (struct proc_request*)proc_set[curr_proc_idx].syscall.content;
        req->type = PROC_EXIT;

        proc_set[curr_proc_idx].syscall.type = SYS_SEND;
        proc_set[curr_proc_idx].syscall.receiver= GPID_PROCESS;
        proc_set[curr_proc_idx].syscall.status= PENDING;

        // 把当前记录设为等待发送，复用下面的消息投递流程，再重新调度。
        proc_set[curr_proc_idx].status = PROC_PENDING_SYSCALL;
        proc_try_syscall(&proc_set[curr_proc_idx]);

        proc_yield();
        return;
    }

    // 系统服务或空闲内核路径出现未处理的异常，当前实现直接报告并停止。
    FATAL("excp_entry: kernel got exception %d", id);
}

// 处理中断：时钟到期就触发调度；外设中断还需要查明具体是哪个设备发出的。
static void intr_entry(uint id) {

    // 1. 时钟到期：记录当前线程被时钟打断了一次，再进入调度器。
    // 用户代码没有主动让出 CPU，也会被中断打断，因此实现了抢占式调度。
    if (id == INTR_ID_TIMER) 
    {
        ++proc_set[curr_proc_idx].time_intr_time;
        return proc_yield();
    }

    // 2. 外设中断。这里主要是网卡中断
    if (id==INTR_ID_EXTERNAL)
    {
        // PLIC 是汇集外设中断的控制器。读取当前核心的领取寄存器，取得设备中断号。
        // 例如得到 ETH_IRQ=35，表示网卡发来了中断。
        uint irq = REGW(PLIC_CAUSE(core_in_kernel),0);
        if(irq == ETH_IRQ)
        {
            REGW(ETH_CTL_BASE,0xC0); // 读取网卡中断原因，同时清除网卡侧的中断状态。

            // rx_head 指向下一项待检查的接收描述符；每项记录一个接收缓冲区的状态和长度。
            static int rx_head = 0;
            // 一次中断可能对应多个包，循环处理。
            while(rx_desc[rx_head].status & 1)
            {
                ushort len = rx_desc[rx_head].length;
                rx_buffer[rx_head][len<2047? len:2047] = '\0';

                // 拆包
                uchar* pkt = (uchar*)rx_buffer[rx_head];
                ushort ethertype = (pkt[12] << 8) | pkt[13];
                uchar proto = pkt[23];
                INFO("Got pkt: rxdesc[%d], len=%d, ethertype=0x%x, proto=%d",
                 rx_head, len, ethertype, (uint)proto);
                if (ethertype == 0x0800 && proto == 17) {
                // 0x0800 表示 IPv4，17 表示 UDP；14 字节以太网头 + 20 字节 IP 头 + 8 字节 UDP 头。
                SUCCESS("Get %d bytes from rxdesc[%d]: %s",
                        len, rx_head, &rx_buffer[rx_head][42]);
                }
                // 包处理完了，清状态、归还这一项给网卡，继续检查下一项；到第 128 项后绕回第 0 项。
                rx_desc[rx_head].status = 0;
                REGW(ETH_CTL_BASE,0x2818) = rx_head;
                rx_head  = (rx_head +1) % 128;
            }
        }
        // 通知中断控制器处理结束，然后重新调度。
        if(irq) REGW(PLIC_CAUSE(core_in_kernel),0) = irq;
        return proc_yield();
    }
}

// 调度函数：更新刚才的运行统计 -> 扫描可运行记录 -> 选优先级最高者 -> 准备恢复执行。
static void proc_yield() {
    // 1. 当前记录若仍在正常运行，先改为“可以继续”，使它也能参与本轮选择。
    if (curr_status == PROC_RUNNING) proc_set_runnable(curr_pid);

    // 2. 计算本次从开始运行到现在经过多久，累加 CPU 时间，并按消耗调整优先级配额。
    // mlfq_reset_level 还会按输入情况、整体提升周期恢复优先级
    ulonglong now = mtime_get();
    ulonglong runtime = now - proc_set[curr_proc_idx].last_run_time;
    proc_set[curr_proc_idx].cpu_time += runtime;
    mlfq_update_level(&proc_set[curr_proc_idx],runtime);
    mlfq_reset_level();

    // 3. 找下一条记录。初始下标 16 表示“还没找到”，5 比最低优先级 4 还低。
    int next_idx = MAX_NPROCESS;
    int min_level = 5;
    for (uint i = 1; i <= MAX_NPROCESS; i++) {
        // 从当前下标的后一项开始，取模后循环绕回，确保 16 项都检查一次。
        // 优先级相同时保留先找到的记录，因此同级之间可以轮流获得运行机会。
        struct process* p = &proc_set[(curr_proc_idx + i) % MAX_NPROCESS];

        // 还在睡眠的记录直接跳过；到时清除睡眠限制，再继续检查其它条件。
        if(p->sleep_until>0) 
        {
            if(now<p->sleep_until)
                continue;
            else
                p->sleep_until = 0;
        }
        // 等待中的系统调用重新尝试一次，例如检查消息是否已经送到；完成后可能变为可运行。
        if (p->status == PROC_PENDING_SYSCALL) proc_try_syscall(p);

        // 只选 READY/RUNNABLE，跳过未准备好、仍在等待、正在其它核心运行的记录。
        // level 越小越优先
        if ((p->status == PROC_READY || p->status == PROC_RUNNABLE) && (p->level<min_level)) {
            next_idx = (curr_proc_idx + i) % MAX_NPROCESS;
            min_level = p->level;
        }
    }

    if (next_idx < MAX_NPROCESS) {
        // 4a. 找到可运行记录：首次选中就登记首次运行时刻，每次选中都登记本次开始时刻。
        if(proc_set[next_idx].first_time==0)
            proc_set[next_idx].first_time = mtime_get();
        proc_set[next_idx].last_run_time = mtime_get();

        asm("csrc mstatus, %0" :: "r"(3<<11));
    } else {
        // 4b. 没有可运行记录：可能其它核心正在执行它们，也可能它们在睡眠或等待消息。
        // 先检查是否还存在有效记录，全部消失才报告失败。
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
        
        // 目前没有任务可以运行
        // 将这个核心标为空闲
        curr_proc_idx = MAX_NPROCESS;
        // 记录开始空闲时刻
        proc_set[MAX_NPROCESS].last_run_time = mtime_get();
        // 设置下一次时钟中断，等待下一次中断唤醒该核心查看是否有任务
        earth->timer_reset(core_in_kernel);
        release(kernel_lock);
        // 放锁后不再用共享内核栈。wfi 返回时继续等中断。
        asm volatile("csrsi mstatus, 8\n1: wfi\nj 1b" ::: "memory");
        __builtin_unreachable();
    }

    // 5. 更新当前核心的记录下标。
    curr_proc_idx = next_idx;
    // 让当前核心使用所选进程的页表
    earth->mmu_switch(curr_pid);
    earth->mmu_flush_cache();

    if (curr_status == PROC_READY) {
        // 对于首次执行的函数继续配置
        // 新进程第一次运行，准备 main 的参数位置，并设置程序入口
        curr_saved[0]                = APPS_ARG;
        curr_saved[1]                = APPS_ARG + 4;
        proc_set[curr_proc_idx].mepc = APPS_ENTRY;
    }
    // 标记正在运行并设置下一次时钟中断，最后由 kernel_entry 和 kernel.s 完成现场恢复。
    proc_set_running(curr_pid);
    earth->timer_reset(core_in_kernel);
}

// 尝试发送消息：sender 指向发送方 PCB
// 本项目的发送要等接收方准备好；如果现在接收不了，就保持等待，留待以后再次尝试。
// 例如 A 给 B 发消息，只有 B 已经请求接收、并且允许接收 A 的消息，才会复制过去。
static void proc_try_send(struct process* sender) {
    for (uint i = 0; i < MAX_NPROCESS; i++) {
        struct process* dst = &proc_set[i];
        if (dst->pid == sender->syscall.receiver &&
            dst->status != PROC_UNUSED) {
            // 1. 找到目标 PID 的有效记录后，检查它是否正在等待接收消息。
            if (!(dst->syscall.type == SYS_RECV &&
                  dst->syscall.status == PENDING))
                return;
            // 2. 接收方允许任意来源（GPID_ALL=-1），或者明确指定了当前发送者，才继续。
            if (!(dst->syscall.sender == GPID_ALL ||
                  dst->syscall.sender == sender->pid))
                return;

            // 3. 标记消息已到达，并把“希望从谁接收”改为“实际是谁发来的”。
            // 这里的 DONE 只描述这次接收已匹配，进程状态还未改为可运行。
            dst->syscall.status = DONE;
            dst->syscall.sender = sender->pid;
            // 将消息从发送方的内核记录复制到接收方的内核记录
            memcpy(dst->syscall.content, sender->syscall.content,
                   SYSCALL_MSG_LEN);
            return;
        }
    }
    // 目标 PID 根本不存在
    FATAL("proc_try_send: unknown receiver pid=%d", sender->syscall.receiver);
}

// 尝试完成接收：receiver 指向接收方 PCB。消息到达后，写回它的参数页，并让收发双方可运行。
static void proc_try_recv(struct process* receiver) {
    if (receiver->syscall.status == PENDING) return;

    // 1. 找到接收方参数页的物理地址，把消息和实际发送者信息写回去。
    // 接收方从 ecall 返回后，sys_recv() 才能从参数页中取出消息，复制到自己的 buf。
    uint syscall_paddr = earth->mmu_translate(receiver->pid, SYSCALL_ARG);
    memcpy((void*)syscall_paddr, &receiver->syscall, sizeof(struct syscall));

    // 2. 接收方完成等待，可以重新参与调度
    receiver->status = PROC_RUNNABLE;
    // 3. 找到与这次接收对应、仍在等待发送的记录，也让它重新参与调度。
    for (int i = 0; i < MAX_NPROCESS; i++) {
        if (proc_set[i].pid == receiver->syscall.sender &&
            proc_set[i].syscall.type == SYS_SEND &&
            proc_set[i].syscall.receiver == receiver->pid &&
            proc_set[i].status == PROC_PENDING_SYSCALL) {
            proc_set[i].status = PROC_RUNNABLE;
            break;
        }
    }
}

// 给当前进程增加一个线程，proc 指向发起创建请求的 PCB。
// 新线程占用另一个 proc_set 位置，沿用相同 PID 和页表，因此共享代码、全局变量等内存。
// 只需单独准备它的执行入口、寄存器和栈位置，不需要再加载一份 ELF 或新建进程页表。
static void proc_try_thread_create(struct process* proc)
{
    // 1. 找一个空闲记录。这里直接沿用原进程 PID，没有调用会分配新 PID 的 proc_alloc()。
    int new_idx = -1;
    for(int i=0;i<MAX_NPROCESS;i++)
    {
        if(proc_set[i].status == PROC_UNUSED)
        {
            new_idx = i;
            break;
        }
    }
    if(new_idx == -1)
        FATAL("No free process slot for new thread");
    
    // 2. thread_create() 已在请求内容里依次填写三个指针：入口函数、传给它的参数、栈顶。
    void** args = (void**)proc->syscall.content;
    void* entry = args[0];
    void* arg  = args[1];
    void* stack_top = args[2];
    
    // 3. 登记共享 PID，设置新线程要执行的入口
    proc_set[new_idx].pid = proc->pid;
    proc_set[new_idx].status = PROC_RUNNABLE;
    proc_set[new_idx].mepc = (uint)entry;

    // 4. 寄存器初始值清零，再设置 a0=入口函数的首参数、sp=它自己的栈顶。
    memset(proc_set[new_idx].saved_registers,0,sizeof(proc_set[new_idx].saved_registers));
    proc_set[new_idx].saved_registers[0] = (uint)arg;        /* a0：入口首参数。 */
    proc_set[new_idx].saved_registers[30] = (uint)stack_top; /* sp：新线程私有栈顶。 */

    // 5. 创建请求完成
    proc->syscall.status = DONE;
    proc->status = PROC_RUNNABLE;
}

// 这里直接支持接收、发送、创建线程；文件读取、进程创建和 sleep 等需求通过消息交给系统服务。
static void proc_try_syscall(struct process* proc) {
    switch (proc->syscall.type) {
    case SYS_RECV:
        proc_try_recv(proc);
        break;
    case SYS_SEND:
        proc_try_send(proc);
        break;
    case SYS_THREAD:
        proc_try_thread_create(proc);
        break;
    default:
        FATAL("proc_try_syscall: unknown syscall type=%d", proc->syscall.type);
    }
}
