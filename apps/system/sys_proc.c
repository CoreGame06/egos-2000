/* 进程管理服务（PID 1，用户态）：启动其它服务，处理进程创建、退出和睡眠请求。 */

#include "app.h"
#include "elf.h"
#include "disk.h"

// 当前加载的用户程序文件编号，以及最近创建的用户进程 PID。
static int app_ino, app_pid;
static void sys_spawn(uint base);
static int app_spawn(struct proc_request* req);

// grass_entry 传来的启动共享数据，对应 boot.s 中相邻的启动锁和核心计数。
struct multicore {
    int boot_lock;
    int booted_core_cnt;
};

// boot 用于放行其它核心，unused 不使用。
int main(int unused, struct multicore* boot) {
    SUCCESS("Enter kernel process GPID_PROCESS");

    // 释放启动锁，允许其它核心进入 boot。
    release(boot->boot_lock);
    // 忙等四个核心都进入 boot；计数在进入时就增加。
    while(boot->booted_core_cnt<NCORES);

    // sender 是发送者 PID；shell_waiting 表示 Shell 正在等前台命令退出。
    int sender, shell_waiting = 0;
    // 共用 buf 接收请求、准备回复。
    char buf[SYSCALL_MSG_LEN];

    // 按顺序启动终端、文件和 Shell；前两个服务发来就绪消息后，才继续下一步。
    // 加载并标记 READY 后，服务还要被调度执行、完成初始化，才能发就绪消息。
    sys_spawn(SYS_TERM_EXEC_START);
    grass->sys_recv(GPID_TERMINAL, NULL, buf, SYSCALL_MSG_LEN);
    INFO("sys_process receives: %s", buf);

    sys_spawn(SYS_FILE_EXEC_START);
    grass->sys_recv(GPID_FILE, NULL, buf, SYSCALL_MSG_LEN);
    INFO("sys_process receives: %s", buf);

    sys_spawn(SYS_SHELL_EXEC_START);

    // 接收请求，按类型处理。
    while (1) {
        // req/reply 共用 buf，分别按请求、回复的格式访问。
        struct proc_request* req = (void*)buf;
        struct proc_reply* reply = (void*)buf;
        // 接受任意进程的请求，并取得发送者 PID；没有消息时阻塞。
        grass->sys_recv(GPID_ALL, &sender, buf, SYSCALL_MSG_LEN);

        switch (req->type) {
        case PROC_SPAWN:
            // 创建用户进程，返回 CMD_OK 或 CMD_ERROR。
            reply->type = app_spawn(req);

            // 创建成功且末尾没有 &，说明 Shell 还需要等这个前台命令退出。
            shell_waiting = (req->argc > 0) &&
                (req->argv[req->argc - 1][0] != '&') && (reply->type == CMD_OK);
            if (!shell_waiting && reply->type == CMD_OK)
                INFO("process %d running in the background", app_pid);
            // 第一次回复创建结果；前台命令退出时再通知一次，后台命令不等第二次。
            grass->sys_send(GPID_SHELL, (void*)reply, sizeof(*reply));
            break;
        case PROC_EXIT:
            // 回收发送者的内存和 PCB。
            grass->proc_free(sender);

            // 前台命令退出，发第二条通知，让 Shell 继续读命令。
            if (shell_waiting && app_pid == sender) {
                shell_waiting = 0;
                grass->sys_send(GPID_SHELL, (void*)reply, sizeof(*reply));
            } else {
                INFO("background process %d terminated", sender);
            }
            break;
        case PROC_KILLALL:
            // 回收全部用户进程，保留系统服务。
            grass->proc_free(GPID_ALL);
            break;

        case PROC_SLEEP:
            // 设置发送者的睡眠目标时刻，sleep_time 单位为微秒。
            grass->proc_sleep(sender,req->sleep_time);
            break;
        default:
            FATAL("sys_process: invalid request %d", req->type);
        }
    }
}

// 供 elf_load 读取用户程序：通过 sys_file 读取文件 app_ino 的第 off 块（512 字节）。
static void app_read(uint off, char* dst) { file_read(app_ino, off, dst); }

// 创建普通进程。req 中包含命令名和参数，例如 echo hello 对应 argc=2、argv={"echo","hello"}。
static int app_spawn(struct proc_request* req) {
    // 在 /bin 中查找程序文件，找不到则返回创建失败。
    int bin_ino = dir_lookup(0, "bin/");
    if ((app_ino = dir_lookup(bin_ino, req->argv[0])) < 0) return CMD_ERROR;
    // 去掉后台执行标记 &，它不属于传给程序的参数。
    int argc = req->argv[req->argc - 1][0] == '&' ? req->argc - 1 : req->argc;

    // 分配 PID/PCB -> 加载程序并准备页表、参数和栈 -> 标记 READY，等待首次调度。
    // app_read 提供文件读取方法，此处还没有执行新程序的 main。
    app_pid = grass->proc_alloc();
    elf_load(app_pid, app_read, argc, (void**)req->argv);
    grass->proc_set_ready(app_pid);

    return CMD_OK;
}

// 要加载的系统服务在整盘上的起始块号。
static int sys_apps_base;
// 名称按 PID-1 索引，PID 由启动时的创建顺序保证。
char* sys_apps[] = {"sys_process", "sys_terminal", "sys_file", "sys_shell"};

// 系统服务按固定磁盘位置读取：起始块号 + 文件内块号。
// 启动 sys_file 本身时，它还不能接收文件请求，因此直接调用磁盘驱动。
static void sys_proc_read(uint block_no, char* dst) {
    earth->disk_read(sys_apps_base + block_no, 1, dst);
}

// 从整盘第 base 块加载系统服务；终端、文件、Shell 依次分配到 PID 2、3、4。
static void sys_spawn(uint base) {
    int pid = grass->proc_alloc();
    INFO("Load kernel process #%d: %s", pid, sys_apps[pid - 1]);

    // 指定起始块号，加载器通过 sys_proc_read 读取内容。
    sys_apps_base = base;
    elf_load(pid, sys_proc_read, 0, NULL);
    grass->proc_set_ready(pid); // 准备完毕，等待首次调度。
}
