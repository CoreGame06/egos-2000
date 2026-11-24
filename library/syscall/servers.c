/* 把退出、睡眠、文件读取和终端操作封装成应用可直接调用的函数。 */

#include "egos.h"
#include "syscall.h"
#include <stdlib.h>

static int sender;
static char buf[SYSCALL_MSG_LEN];

void exit(int status) {
    // 请求 sys_proc 回收整个进程，包括同 PID 的线程。当前消息只传退出请求类型。
    struct proc_request req;
    req.type = PROC_EXIT;
    sys_send(GPID_PROCESS, (void*)&req, sizeof(req));
    while (1); // 等待回收，避免 exit 返回后继续执行应用代码。
}

void sleep(uint usec) {
    // 请 sys_proc 登记唤醒时刻，调度器会跳过尚未到时的进程。
    // 这里只发送请求，依赖 sys_proc 在调用者再次运行前完成登记。
    struct proc_request req;
    req.type = PROC_SLEEP;
    req.sleep_time = usec;
    sys_send(GPID_PROCESS,(void*)&req,sizeof(req));

}

int dir_lookup(int dir_ino, char* name) {
    // 目录的第 0 块存放“名字 inode号”，例如 "README   5 "。目录名以 / 结尾。
    char buf[BLOCK_SIZE];
    file_read(dir_ino, 0, buf);

    uint blen = strlen(buf), namelen = strlen(name);
    if (blen <= namelen) return -1;

    // 按完整名字匹配，再把后面的数字读作 inode 号。
    for (uint i = 0; i < blen - namelen; i++)
        if (!strncmp(name, buf + i, namelen) && buf[i + namelen] == ' ' &&
            (i == 0 || buf[i - 1] == ' '))
            return atoi(buf + i + namelen);

    return -1;
}

int file_read(int file_ino, uint offset, char* block) {
    // 请求 sys_file 读取文件的第 offset 块，等待回复后把 512 字节复制到 block。
    // 同一进程的线程共用上面的回复缓冲区，应串行调用。
    struct file_request req;
    req.type   = FILE_READ;
    req.ino    = file_ino;
    req.offset = offset;

    sys_send(GPID_FILE, (void*)&req, sizeof(req));
    sys_recv(GPID_FILE, &sender, buf, SYSCALL_MSG_LEN);

    struct file_reply* reply = (void*)buf;
    memcpy(block, reply->block.bytes, BLOCK_SIZE);

    return reply->status == FILE_OK ? 0 : -1;
}

// KERNEL 选择内核和系统服务使用的串口读写版本，普通应用通过消息请求终端服务。
// 这个编译开关只决定读写走哪条路径，CPU 权限由运行模式决定。
#ifndef KERNEL

// 请求终端服务读一行，等回复后复制到调用者的 buf。
int term_read(char* buf, uint len) {
    struct term_request req;
    struct term_reply reply;
    req.type = TERM_INPUT;
    req.len  = len;
    sys_send(GPID_TERMINAL, (void*)&req, sizeof(req));
    sys_recv(GPID_TERMINAL, NULL, (void*)&reply, sizeof(reply));
    memcpy(buf, reply.buf, reply.len);
    return reply.len;
}

// 将要输出的 len 个字节交给终端服务，服务不会另外发送回复。
void term_write(char* str, uint len) {
    struct term_request req;
    req.type = TERM_OUTPUT;
    req.len  = len;
    memcpy(req.buf, str, len);
    sys_send(GPID_TERMINAL, (void*)&req, sizeof(req));
}

#else

// 直接从串口逐字节读取，回显输入，处理回车和退格。留一个字节放字符串结束符。
int term_read(char* buf, uint len) {
    char c;
    for (int i = 0; i < len - 1; i++) {
        earth->tty_read(&c);
        buf[i] = c;

        switch (c) {
        case 0x0d: // 回车结束本行，空行返回 0。
            buf[i] = 0;
            term_write("\n\r", 2);
            return i ? i + 1 : 0;
        case 0x7f: // 退格时擦掉上一个显示的字符，下次读入复用它的位置。
            c = 0;
            if (i) term_write("\b \b", 3);
            i = i ? i - 2 : i - 1;
        }
        if (c) term_write(&c, 1);
    }

    buf[len - 1] = 0;
    return len;
}

void term_write(char* str, uint len) {
    // 通过 Earth 的串口接口逐字节输出。
    for (uint i = 0; i < len; i++) earth->tty_write(str[i]);
}

#endif
