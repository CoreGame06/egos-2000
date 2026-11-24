/* 
 * 文件和磁盘分区共用的块读写接口。
 * 分区层把整个分区看成 inode 0。文件系统在其上用不同的 inode 号区分文件和目录。
 */

#pragma once
#include "disk.h"

#define NINODES 128
typedef struct inode_store* inode_intf;

// self 是当前这层的接口对象，ino 是这一层的 inode 号。
// offset 和 newsize 的单位是 512 字节块，读写成功返回 0，失败按接口约定返回 -1。
struct inode_store {
    int (*getsize)(inode_intf self, uint ino); // 返回块数。
    int (*setsize)(inode_intf self, uint ino, uint newsize); // 改成 newsize 块，返回原块数。
    int (*read)(inode_intf self, uint ino, uint offset, block_t* block); // 读到 block。
    int (*write)(inode_intf self, uint ino, uint offset, block_t* block); // 写入 block 的内容。
    void* state; // 实现保存自己的数据，例如下层接口。
};

// create 在下层存储上创建文件系统，init 返回访问已有文件系统的接口。
// below 是下层接口，below_ino 指明用下层的哪个 inode 存放文件系统。
inode_intf mydisk_init(inode_intf below, uint below_ino);
int mydisk_create(inode_intf below, uint below_ino, uint ninodes);

inode_intf treedisk_init(inode_intf below, uint below_ino);
int treedisk_create(inode_intf below, uint below_ino, uint ninodes);
