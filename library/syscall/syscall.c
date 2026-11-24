/* 应用调用的系统调用函数。先填参数页，再执行 ecall 进入内核。 */

// 这里实现IPC，具体是通过内核复制消息来进行通信
// 假设进程 A 给进程 B 发消息：
// A 的参数页 → A 的 PCB → B 的 PCB → B 的参数

#include "egos.h"
#include "syscall.h"
#include <stdlib.h>

// 每个进程用同一个虚拟地址 SYSCALL_ARG，页表将它映射到自己的物理页。
// 同一进程的线程共用这页，同时发起系统调用可能覆盖彼此的参数。
static struct syscall* sc = (struct syscall*)SYSCALL_ARG;

void sys_send(int receiver, char* msg, uint size) {
    // 填入目标 PID 和消息内容，进入内核等待对方接收。
    // 如果还需要处理结果，例如读到的文件数据，发送后要再调用 sys_recv 等回复。
    sc->type     = SYS_SEND;
    sc->receiver = receiver;
    memcpy(sc->content, msg, size);
    asm("ecall");
}

void sys_recv(int from, int* sender, char* buf, uint size) {
    // from 指定接收哪个 PID 的消息，GPID_ALL 表示接收任意来源。
    sc->type   = SYS_RECV;
    sc->sender = from;
    asm("ecall");
    // 有匹配的消息后，内核把内容和实际发送者 PID 写回 sc，这里再取出结果。
    memcpy(buf, sc->content, size);
    if (sender) *sender = sc->sender;
}

// 每个进程只准备一块 4 KiB 的工作线程栈，多次创建会复用同一个栈地址。
// 所以目前一个进程除自己外只可以单开一个线程，否则多线程的栈会踩踏
static char thread_stack[4096];

int thread_create(void (*entry)(void*), void* arg)
{
    // 把入口函数、参数、栈顶交给内核，分别用来设置新线程的 PC、a0 和 sp。
    // 栈向低地址增长，所以初始栈顶设在数组末尾之后。
    sc->type = SYS_THREAD;

    void** args = (void**)sc->content;
    args[0] = (void*)entry;
    args[1] = arg;
    args[2] = (char*)thread_stack + 4096;

    asm("ecall");
    return 0;
}
