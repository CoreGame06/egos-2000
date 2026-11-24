#pragma once

/* 磁盘块大小和镜像布局，mkfs 和系统启动代码共用。 */

// 磁盘按 512 字节的块读写。
#define BLOCK_SIZE 512

typedef struct block {
    char bytes[BLOCK_SIZE];
} block_t;

// disk.img 前 2 MiB 放内核、系统服务和资源，后 2 MiB 放文件系统。
#define SIZE_2MB             (2 * 1024 * 1024)
#define EGOS_BIN_DISK_SIZE   SIZE_2MB
#define FILE_SYS_DISK_SIZE   SIZE_2MB
#define FILE_SYS_DISK_START  (EGOS_BIN_DISK_SIZE / BLOCK_SIZE) // 整盘第 4096 块。
#define EGOS_BIN_MAX_NBYTE   (128 * 1024) // 镜像中相邻预留位置的间隔。

// 各系统服务在整盘上的起始块号，供启动时直接从磁盘加载。
#define SYS_PROC_EXEC_START  (EGOS_BIN_MAX_NBYTE / BLOCK_SIZE) * 1
#define SYS_TERM_EXEC_START  (EGOS_BIN_MAX_NBYTE / BLOCK_SIZE) * 2
#define SYS_FILE_EXEC_START  (EGOS_BIN_MAX_NBYTE / BLOCK_SIZE) * 3
#define SYS_SHELL_EXEC_START (EGOS_BIN_MAX_NBYTE / BLOCK_SIZE) * 4
