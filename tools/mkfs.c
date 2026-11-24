#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "inode.h"

/* P0/P6/P8: 本程序由宿主 CC 编译，在宿主 Linux 上执行，不是 egos 的用户命令。
 * 它把 build/release 中的 RISC-V ELF 放进镜像，并预先格式化文件系统。
 * exec 前部按 128 KiB 间隔放系统镜像，起始块号须与 library/file/disk.h 一致；
 * 最后的 Bohr.bmp 是显示演示资源，读取位置在 apps/user/video_demo.c。 */
char* egos_binaries[] = {"./egos.bin",
                         "../build/release/sys_proc.elf",
                         "../build/release/sys_terminal.elf",
                         "../build/release/sys_file.elf",
                         "../build/release/sys_shell.elf",
                         "./images/Bohr.bmp" /* for the video demo app */};
#define EGOS_BIN_NUM ((sizeof(egos_binaries) / sizeof(char*)))

/* P6: 目录就是存文本条目的普通 inode，格式为“名字 空格 inode号 空格”。
 * . 和 .. 也以目录项表示；目录名尾部加 /。目录 inode 0 为根，6 为 /bin。
 * contents[5] 为 README 内容，contents[6] 的 /bin 稍后追加用户 ELF 名字。 */
char bin_dir[256] = "./   6 ../   0 ";
char* contents[]  = {
    "./   0 ../   0 home/   1 bin/   6 ",
    "./   1 ../   0 egos/   2 rvr/   3 yacqub/   4 ",
    "./   2 ../   1 README   5 ",
    "./   3 ../   1 ",
    "./   4 ../   1 ",
    "With only 2000 lines of code, egos-2000 implements boot loader, SD card "
     "driver, tty driver, virtual memory with page tables, interrupt and "
     "exception handling, preemptive scheduler, system call, file system, "
     "shell, an Ethernet demo, several user commands, and the mkfs tool. "
     "Moreover, the EGOS book (https://egos.fun) contains 9 course projects.",
    bin_dir};
#define BIN_DIR_INODE ((sizeof(contents) / sizeof(char*)) - 1)

char inode[SIZE_2MB], tmp[512];
char exec[SIZE_2MB], fs[SIZE_2MB];

/* file_name 是宿主机路径，dst 为镜像内存缓冲区；返回读入的字节数。
 * 此处 open/read 是宿主系统调用，与 egos 的 file_read IPC 封装不同。 */
int load_file(char* file_name, char* dst) {
    struct stat st;
    stat(file_name, &st);
    int fd = open(file_name, O_RDONLY);
    for (uint nread = 0; nread < st.st_size;)
        nread += read(fd, dst + nread, st.st_size - nread);
    close(fd);

    return st.st_size;
}

int getsize(inode_intf bs, uint ino) { return FILE_SYS_DISK_SIZE / BLOCK_SIZE; }

int setsize(inode_intf bs, uint ino, uint newsize) { assert(0); }

/* P6: mkfs 用内存 fs[] 模拟分区块设备，复用和运行时相同的文件系统实现。
 * offset 是分区块号，512*offset 才是 fs[] 内的字节偏移；不用在宿主上启动 QEMU。 */
int ramread(inode_intf bs, uint ino, uint offset, block_t* block) {
    memcpy(block, fs + offset * BLOCK_SIZE, BLOCK_SIZE);
    return 0;
}

int ramwrite(inode_intf bs, uint ino, uint offset, block_t* block) {
    memcpy(fs + offset * BLOCK_SIZE, block, BLOCK_SIZE);
    return 0;
}

// argv[1..] 是 Makefile 传入的当前用户程序 ELF 路径，只打包这些程序，跳过遗留产物。
int main(int argc, char** argv) {
    /* Write the kernel and system server binaries into exec[]. */
    printf("[INFO] Load %ld kernel binary files\n", EGOS_BIN_NUM);
    for (uint i = 0; i < EGOS_BIN_NUM; i++) {
        int sz = load_file(egos_binaries[i], exec + i * EGOS_BIN_MAX_NBYTE);
        printf("[INFO] Load %s: %d bytes\n", egos_binaries[i], sz);
    }

    /* Initialize the file system using the fs[] buffer as a ramdisk. */
    printf("MKFS is using *%s*\n", FILESYS == 0 ? "mydisk" : "treedisk");
    struct inode_store ramdisk = (struct inode_store){.read    = ramread,
                                                      .write   = ramwrite,
                                                      .getsize = getsize,
                                                      .setsize = setsize};
    (FILESYS == 0) ? assert(mydisk_create(&ramdisk, 0, NINODES) >= 0)
                   : assert(treedisk_create(&ramdisk, 0, NINODES) >= 0);
    inode_intf filesys =
        (FILESYS == 0) ? mydisk_init(&ramdisk, 0) : treedisk_init(&ramdisk, 0);

    /* Write to inode 0..BIN_DIR_INODE-1 in the file system. */
    for (uint ino = 0; ino < BIN_DIR_INODE; ino++) {
        printf("[INFO] Load ino=%d, %ld bytes\n", ino, strlen(contents[ino]));
        strncpy(inode, contents[ino], BLOCK_SIZE);
        filesys->write(filesys, ino, 0, (void*)inode);
    }

    // 每个当前构建的用户 ELF 占用一个 inode，并把名字登记到 /bin。
    uint app_ino = BIN_DIR_INODE + 1;
    for (int i = 1; i < argc; i++) {
        const char* name = strrchr(argv[i], '/');
        name = name ? name + 1 : argv[i];
        size_t len = strlen(name);
        assert(len > 4 && !strcmp(name + len - 4, ".elf"));

        int file_size = load_file(argv[i], inode);
        printf("[INFO] Load ino=%d, %s: %d bytes\n", app_ino, name, file_size);
        for (uint b = 0; b * BLOCK_SIZE < file_size; b++)
            filesys->write(filesys, app_ino, b, (void*)(inode + b * BLOCK_SIZE));

        // 去掉 .elf 后缀，以命令名存入目录；不修改传入的路径字符串。
        sprintf(tmp, "%.*s%4d ", (int)(len - 4), name, app_ino++);
        assert(strlen(bin_dir) + strlen(tmp) < sizeof(bin_dir));
        strcat(bin_dir, tmp);
    }
    filesys->write(filesys, BIN_DIR_INODE, 0, (void*)bin_dir);
    printf("[INFO] Load ino=%ld, %s\n", BIN_DIR_INODE, bin_dir);

    /* P6/P8: disk.img=[2 MiB exec][2 MiB fs]，运行时作为模拟 SD 卡。
     * qemuROM.bin 为模拟 Flash 填充到 32 MiB。
     * qemuROM 和 disk 并非运行时自动同步的同一块存储；修改 SD 卡不等于修改 Flash。 */
    /* Generate the disk image file. */
    int fd  = open("disk.img", O_CREAT | O_WRONLY, 0666);
    int sz1 = write(fd, exec, SIZE_2MB);
    sz1 += write(fd, fs, SIZE_2MB);
    close(fd);

    fd      = open("qemuROM.bin", O_CREAT | O_WRONLY, 0666);
    int sz3 = write(fd, exec, SIZE_2MB);
    for (uint i = 0; i < 15; i++) sz3 += write(fd, fs, SIZE_2MB);
    /* Simply pad the image to 32MB, as required by QEMU. */
    close(fd);

    assert(sz1 == SIZE_2MB * 2 && sz3 == SIZE_2MB * 16);
    printf("[INFO] Finish making the image files\n");
    return 0;
}
