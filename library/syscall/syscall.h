#pragma once

#include "servers.h"
#include <string.h>

// 内核根据这三个编号分发系统调用。睡眠、文件读取等由系统服务处理，请求通过 SYS_SEND 发送。
enum syscall_type {
    SYS_RECV = 1,
    SYS_SEND = 2,
    SYS_THREAD = 3,
};

#define SYSCALL_MSG_LEN 1024
// 用户代码把请求写到参数页，内核复制到 PCB 中处理。接收完成后，内核把结果写回参数页。
struct syscall {
    enum syscall_type type; // 发送、接收或创建线程。
    int sender;             // 接收前填期望的来源，完成后记录实际发送者 PID。
    int receiver;           // 发送时填写目标 PID。
    char content[SYSCALL_MSG_LEN]; // 消息内容或线程创建参数。内核转发消息时复制整个字段。
    enum { PENDING, DONE } status; // 由内核记录请求的处理状态。
};

// size 是复制的字节数，两者都要求 size 不超过缓冲区容量和 1024 字节。
// 向 receiver 指定的 PID 发送 msg。
void sys_send(int receiver, char* msg, uint size);
// 从 from 指定的 PID 接收消息到 buf。sender 可用来取实际发送者 PID，传 NULL 则忽略。
void sys_recv(int from, int* sender, char* buf, uint size);
// 创建一个从 entry(arg) 开始执行的线程，共用当前进程的 PID 和页表。
int thread_create(void (*entry)(void*),void * agr);
