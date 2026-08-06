/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: the system call interface for applications
 */

#include "egos.h"
#include "syscall.h"
#include <stdlib.h>

static struct syscall* sc = (struct syscall*)SYSCALL_ARG;

void sys_send(int receiver, char* msg, uint size) {
    sc->type     = SYS_SEND;
    sc->receiver = receiver;
    memcpy(sc->content, msg, size);
    asm("ecall");
}

void sys_recv(int from, int* sender, char* buf, uint size) {
    sc->type   = SYS_RECV;
    sc->sender = from;
    asm("ecall");
    memcpy(buf, sc->content, size);
    if (sender) *sender = sc->sender;
}

static char thread_stack[4096];

int thread_create(void (*entry)(void*), void* arg)
{
    sc->type = SYS_THREAD;
    
    void** args = (void**)sc->content;
    args[0] = (void*)entry;
    args[1] = arg;
    args[2] = (char*)thread_stack + 4096;

    asm("ecall");
    return 0;
}
