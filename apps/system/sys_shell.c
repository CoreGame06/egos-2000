/* Shell（PID 4，用户态）：读命令 -> 拆参数 -> 请求创建进程 -> 等待结果。
 * 前台命令等待退出，后台命令（末尾带 &）只等待创建结果。 */

#include "app.h"

// 按空格拆参数，例如 echo hello -> argc=2、argv={"echo","hello"}。
// 最多 16 个参数，每个最多 31 个字符；只支持简单拆词。
int parse_request(char* buf, struct proc_request* req) {
    // idx 是当前参数的字符位置，nargs 是当前参数下标。
    uint idx = 0, nargs = 0;
    memset(req->argv, 0, CMD_NARGS * CMD_ARG_LEN);

    // 普通字符追加到当前参数；空格结束当前参数，连续空格跳过。
    for (uint i = 0; i < strlen(buf); i++)
        if (buf[i] != ' ') {
            req->argv[nargs][idx] = buf[i];
            if (++idx >= CMD_ARG_LEN) return -1; // 参数太长，解析失败。
        } else if (idx != 0) {
            // 补结束符，准备填写下一个参数。
            idx = req->argv[nargs][idx] = 0;
            if (++nargs >= CMD_NARGS) return -1;
        }
    // 最后一个参数若没有以空格结尾，也要计入总数。
    req->argc = idx ? nargs + 1 : nargs;
    return 0;
}

int main() {
    CRITICAL("Welcome to the egos-2000 shell!");

    // 首次先执行 cd 设置工作目录；之后处理循环末尾读到的命令。
    char buf[TERM_BUF_SIZE] = "cd";
    while (1) {
        struct proc_request req;
        struct proc_reply reply;

        if (strcmp(buf, "coresinfo") == 0) {
            // 查看各核心正在运行的 PID。
            grass->proc_coresinfo();

        } else if (strcmp(buf, "killall") == 0) {
            // 请求回收全部用户进程，保留系统服务。
            req.type = PROC_KILLALL;
            grass->sys_send(GPID_PROCESS, (void*)&req, sizeof(req));
        } else if (strcmp(buf, "pwd") == 0) {
            // 输出当前目录路径。
            printf("%s\n\r", workdir);
        }else {
            // 其它命令请求 sys_proc 创建进程，例如 echo、cat、cd。
            req.type = PROC_SPAWN;
            if (0 != parse_request(buf, &req)) {
                INFO("sys_shell: too many arguments or argument too long");
            } else if (req.argc > 0) {
                // 把拆好的命令名和参数发给 sys_proc。
                grass->sys_send(GPID_PROCESS, (void*)&req, sizeof(req));
                // 第一次等待：是否创建成功。
                grass->sys_recv(GPID_PROCESS, NULL, (void*)&reply,
                                sizeof(reply));

                if (reply.type != CMD_OK)
                    INFO("sys_shell: command %s not found", req.argv[0]);
                else if (req.argv[req.argc - 1][0] != '&')
                    // 前台命令第二次等待：进程退出。后台命令跳过，直接读下一条输入。
                    grass->sys_recv(GPID_PROCESS, NULL, (void*)&reply,
                                    sizeof(reply));
            }
        }

        // 显示当前目录并读取下一行，空行就重新显示提示符。
        // KERNEL 宏选择直接串口读写，不改变 Shell 的用户态权限。
        do {
            printf("\x1B[1;32m➜ \x1B[1;36m%s\x1B[1;0m ", workdir);
        } while (term_read(buf, TERM_BUF_SIZE) == 0);
    }
}
