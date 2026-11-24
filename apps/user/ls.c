#include "app.h"

int main(int argc, char** argv) {
    // 列出当前目录，当前只读取目录文件的第 0 块。
    if (argc > 1) {
        INFO("ls: ls with args is not implemented");
        return -1;
    }

    char buf[BLOCK_SIZE];
    file_read(workdir_ino, 0, buf);

    // 目录内容按“名字 inode号”保存，把编号换成空格，只显示名字。
    for (uint i = 1; i < strlen(buf); i++)
        if (buf[i - 1] == ' ' && buf[i] >= '0' && buf[i] <= '9') buf[i] = ' ';

    printf("%s\n\r", buf);
    return 0;
}
