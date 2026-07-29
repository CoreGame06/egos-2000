/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: cooperative threads and synchronization
 */

#include <sys/queue.h>
#include "print.c"
#include "thread.h"
#include <stdbool.h>

/* Student's code goes here (Cooperative Threads). */
/* Define the TCB and helper functions (if needed) for cooperative threads. */

#define MAX_THREADS 32
struct thread TCB[MAX_THREADS];
int current_idx;

void threads_clear()
{
    // 清理进程
    for(int i=0;i<MAX_THREADS;i++)
    {
        if(TCB[i].status == THREAD_TERMINATED)
        {
            if(TCB[i].stack_base!=NULL)
            {
                free(TCB[i].stack_base);
                TCB[i].stack_base = NULL;

            }
            TCB[i].status = THREAD_UNUSED;
        } 
    }
}

void thread_init() {
    for( int i = 1;i<MAX_THREADS;i++)
    {
        TCB[i].id = i;
        TCB[i].status = THREAD_UNUSED;
    }
    TCB[0].id = 0;
    TCB[0].status = THREAD_RUNNING;
    TCB[0].args = NULL;
    TCB[0].entry = NULL;
    TCB[0].stack_base = NULL;
}

void ctx_entry() {
    void (*entry)(void* args) = TCB[current_idx].entry;
    void *args = TCB[current_idx].args;
    if(entry!=NULL)
        entry(args);
    thread_exit(); 
}

void thread_create(void (*entry)(void *arg), void *arg) {
    int idx=-1;
    for(int i=0;i<MAX_THREADS;i++)
    {
        if(TCB[i].status==THREAD_UNUSED)
        {
            idx = i;
            break;
        }
    }
    if(idx!=-1)
    {
        void* stack = malloc(STACK_SIZE);
        if(stack == NULL)
        {
            printf("Failed to allocate stack memory for thread\n\r");
            return;
        }
        void* init_sp = (char*)stack + STACK_SIZE;
        TCB[idx].args = arg;
        TCB[idx].entry = entry;
        TCB[idx].sp = init_sp;
        TCB[idx].stack_base =stack;

        // 上下文切换
        int old_idx = current_idx;
        TCB[old_idx].status = THREAD_READY;
        TCB[idx].status = THREAD_RUNNING;
        current_idx = idx;
        ctx_start(&TCB[old_idx].sp,init_sp); // 调用这个的时候，会把主线程的返回地址压入栈中，不是子线程的栈
        // ctx_start 会调用 ctx_entry，把自己的返回地址压入子线程的栈中。当子线程结束后，一步一步返回时
        //返回到 ctx_start时就在自己的栈上找不到下一个返回地址了。所以需要 thread_exit来善后

        // 清理进程
        threads_clear();
    }
    else
    {
        printf("Failed to create thread");
        return;
    }
}

void thread_yield() {
    int idx = current_idx+1;
    for(;idx%MAX_THREADS!=current_idx;idx++)
    {
        if(TCB[idx % MAX_THREADS].status==THREAD_READY)
        {
            idx =idx%MAX_THREADS;
            break;
        }
    }
    if(idx%MAX_THREADS == current_idx)
        return;
    int old_idx = current_idx;
    if(TCB[old_idx].status == THREAD_RUNNING)
        TCB[old_idx].status = THREAD_READY;
    TCB[idx].status = THREAD_RUNNING;
    current_idx = idx;
    ctx_switch(&TCB[old_idx].sp,TCB[idx].sp);    
    threads_clear();
}

void thread_exit() {
    TCB[current_idx].status = THREAD_TERMINATED;
    bool is_end = true;
    for(int i=0;i<MAX_THREADS;i++)
    {
        if(TCB[i].status == THREAD_RUNNING || TCB[i].status==THREAD_READY)
        {
            is_end = false;
            break;
        }
    }
    if(is_end) //只剩最后一个进程调用 thread_exit会进入这种分支。
        _end();
    else
        thread_yield();
}

/* Student's code goes here (Cooperative Threads). */
/* Define helper functions (if needed) for conditional variables. */

/* Student's code ends here. */

void cv_init(struct cv *condition) {
    /* Student's code goes here (Cooperative Threads). */

    /* Student's code ends here. */
}

void cv_wait(struct cv *condition) {
    /* Student's code goes here (Cooperative Threads). */

    /* Student's code ends here. */
}

void cv_signal(struct cv *condition) {
    /* Student's code goes here (Cooperative Threads). */

    /* Student's code ends here. */
}

#define BUF_SIZE 3
void* buffer[BUF_SIZE];
int count = 0;
int head = 0, tail = 0;
struct cv nonempty, nonfull;

void produce(void* arg) {
    while (1) {
        while (count == BUF_SIZE) cv_wait(&nonfull);
        /* At this point, the buffer is not full. */

        /* Student's code goes here (Cooperative Threads). */
        /* Print out the producer ID using the arg parameter. */

        /* Student's code ends here. */
        buffer[tail] = arg;
        tail = (tail + 1) % BUF_SIZE;
        count += 1;
        cv_signal(&nonempty);
    }
}

void consume(void *arg) {
    while (1) {
        while (count == 0) cv_wait(&nonempty);
        /* At this point, the buffer is not empty. */

        /* Student's code goes here (Cooperative Threads). */
        /* Print out the consumer ID using the arg parameter. */

        /* Student's code ends here. */
        void* result = buffer[head];
        head = (head + 1) % BUF_SIZE;
        count -= 1;
        cv_signal(&nonfull);
    }
}

int main() {
    thread_init();
    cv_init(&nonfull);
    cv_init(&nonempty);

    int ID[500];
    for (int i = 0; i < 500; i++) ID[i] = i;

    for (int i = 0; i < 500; i++)
        thread_create(consume, ID + i);

    for (int i = 0; i < 500; i++)
        thread_create(produce, ID + i);

    printf("main thread exits\n\r");
    thread_exit();

    /* The control flow should NEVER get here. If the main thread is the last to
     * call thread_exit(), thread_exit() should terminate the program by calling
     * the _end() in thread.s.
     * If the main thread is not the last, thread_exit() will switch the context
     * to another thread. Later, when all threads have called thread_exit(), the
     * last exited thread should then call _end() when running thread_exit(). */
}
