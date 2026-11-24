#pragma once

// 系统服务的调用接口和消息格式。进程、终端和文件服务分别定义各自的请求结构。
void exit(int status);
// usec 是请求睡眠的时长，单位为微秒。
void sleep(uint usec);
// 读取一行到 buf，len 是缓冲区容量，返回读入长度。
int term_read(char* buf, uint len);
// 输出 str 中的 len 个字节。
void term_write(char* str, uint len);
// 在目录 dir_ino 中按名字查找 inode 号，找不到返回 -1。
int dir_lookup(int dir_ino, char* name);
// 读取文件的第 offset 块，每块 512 字节，结果写入 block。
int file_read(int file_ino, uint offset, char* block);

// 系统服务的 PID 由启动顺序决定。
enum grass_servers {
    GPID_ALL = -1, // 接收时接受任意来源，回收时选中全部用户进程。
    GPID_UNUSED,    /* 0 */
    GPID_PROCESS,   /* 1 */
    GPID_TERMINAL,  /* 2 */
    GPID_FILE,      /* 3 */
    GPID_SHELL,     /* 4 */
    GPID_USER_START /* 5 */
};

// 发给进程服务的请求，最多 16 个命令参数，每个预留 32 字节。
#define CMD_NARGS   16
#define CMD_ARG_LEN 32

struct proc_request {
    enum { PROC_SPAWN, PROC_EXIT, PROC_KILLALL,PROC_SLEEP} type;
    int argc; // 创建进程时使用，例如 cat README 的 argc=2。
    char argv[CMD_NARGS][CMD_ARG_LEN]; // 参数字符串直接放进消息，接收方可直接读取。
    uint sleep_time; // 睡眠请求的时长，单位为微秒。
};

struct proc_reply {
    enum { CMD_OK, CMD_ERROR } type;
};

// 终端服务的请求和回复。
#define TERM_BUF_SIZE 512
struct term_request {
    enum { TERM_INPUT, TERM_OUTPUT } type;
    uint len; // 输入缓冲区容量或要输出的字节数。
    char buf[TERM_BUF_SIZE]; // 输出请求自带的文字。
};

struct term_reply {
    uint len;
    char buf[TERM_BUF_SIZE];
};

// 文件服务的请求和回复，每次操作一个文件块。
#include "disk.h"
struct file_request {
    enum {
        FILE_UNUSED,
        FILE_READ,
        FILE_WRITE,
    } type;
    uint ino; // 文件的 inode 号。
    uint offset; // 从文件开头算起的块号。
    block_t block; // 写请求携带的数据，读到的数据放在回复中。
};

struct file_reply {
    enum file_status { FILE_OK, FILE_ERROR } status;
    block_t block;
};
