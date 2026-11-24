/* 终端服务（PID 2，用户态）：接收输入/输出请求，调用串口读写接口。 */

#include "app.h"

int main() {
    // KERNEL 宏让这里的 term_read/write 直接操作串口，避免给自己发消息；不改变用户态权限。
    SUCCESS("Enter kernel process GPID_TERMINAL");

    // 通知 sys_proc 终端服务已准备好。
    char buf[SYSCALL_MSG_LEN];
    strcpy(buf, "Finish GPID_TERMINAL initialization");
    grass->sys_send(GPID_PROCESS, buf, 36);

    while (1) {
        // 共用 buf 接收请求和准备回复，sender 保存请求者 PID。
        int sender;
        struct term_request* req = (void*)buf;
        struct term_reply* reply = (void*)buf;
        // 等待任意进程的请求，没有消息时阻塞。
        grass->sys_recv(GPID_ALL, &sender, (void*)req, SYSCALL_MSG_LEN);

        // 请求的输入容量或输出长度不能超过缓冲区大小。
        if (req->len > TERM_BUF_SIZE)
            FATAL("sys_terminal: request len %d>TERM_BUF_SIZE", req->len);

        switch (req->type) {
        case TERM_INPUT:
            // 读取一行，回复内容和长度；req->len 是最多可写入的容量。
            // 读取期间轮询串口，当前请求结束后才处理下一条请求。
            reply->len = term_read(reply->buf, req->len);
            grass->sys_send(sender, (void*)reply, sizeof(*reply));
            break;
        case TERM_OUTPUT:
            // 输出请求中的文字，不另外回复消息。
            term_write(req->buf, req->len);
            break;
        default:
            FATAL("sys_terminal: invalid request %d", req->type);
        }
    }
}
