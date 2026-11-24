#include "app.h"

// cat FILE：查找当前目录下的文件，读取并显示第一个 512 字节块。
int main(int argc, char** argv) {
    if (argc != 2) {
        INFO("usage: cat [FILE]");
        return -1;
    }

    // 根据文件名查找 inode 号。
    int file_ino = dir_lookup(workdir_ino, argv[1]);
    if (file_ino < 0) {
        INFO("cat: file %s not found", argv[1]);
        return -1;
    }

    // 通过文件服务读取第 0 块，再输出内容。
    char buf[BLOCK_SIZE];
    file_read(file_ino, 0, buf);
    printf("%s", buf);
    if (buf[strlen(buf) - 1] != '\n') printf("\n\r");

    return 0;
}
