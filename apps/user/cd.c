#include "app.h"

// cd 是独立进程，通过修改共享的目录编号和路径，让 Shell 读到新工作目录。
int main(int argc, char** argv) {
    // 无参数时回到 /home/egos。
    if (argc == 1) {
        int home_ino = dir_lookup(0, "home/");
        int dir_ino = home_ino < 0 ? -1 : dir_lookup(home_ino, "egos/");
        if (dir_ino < 0) {
            INFO("cd: directory /home/egos not found");
            return -1;
        }
        workdir_ino = dir_ino;
        strcpy(workdir, "/home/egos");
        return 0;
    }

    // 在当前目录中查找目标目录，目录名以 / 结尾；找到后更新共享的 inode 号。
    if (argv[1][strlen(argv[1]) - 1] != '/') strcat(argv[1], "/");
    int dir_ino = dir_lookup(workdir_ino, argv[1]);
    if (dir_ino == -1) {
        INFO("cd: directory %s not found", argv[1]);
        return -1;
    }
    workdir_ino = dir_ino;

    // 更新共享路径：./ 不变，../ 去掉最后一级目录，其它名称追加到路径后面。
    if (strcmp("./", argv[1]) == 0) return 0;

    uint len = strlen(workdir);
    if (strcmp("../", argv[1]) == 0) {
        while (workdir[len] != '/') workdir[len--] = 0;
        if (len) workdir[len] = 0;
    } else {
        if (len > 1) strcat(workdir, "/");
        strncat(workdir, argv[1], strlen(argv[1]) - 1);
    }

    return 0;
}
