/* Author: Robbert van Renesse, August 2015
 *
 * treedisk 的磁盘结构。超级块和 inode 表放在开头，其余块存放文件数据、树索引或空闲块记录。
 */
#pragma once
#include "inode.h"

// 每个间接块能存 128 个块号，查找时用文件块号每 7 位选一层分支。
// 指向数据块或间接块的引用为 0 时表示空洞，读取返回全零。
typedef unsigned int block_no;
#define REFS_PER_BLOCK   (BLOCK_SIZE / sizeof(block_no))
#define INODES_PER_BLOCK (BLOCK_SIZE / sizeof(struct treedisk_inode))

// 超级块存放在第 0 块。
struct treedisk_superblock {
    block_no n_inodeblocks; // inode 表占多少块。
    block_no free_list;     // 空闲表第一个块的块号，0 表示已无空闲块。
};

// 文件只有一块时，root 直接指向数据。文件更大时，root 指向树顶的间接块。
struct treedisk_inode {
    block_no root;    // 树的根块号。
    block_no nblocks; // 文件长度，单位为块。
};

// 一个块里放连续的多个 inode。
struct treedisk_inodeblock {
    struct treedisk_inode inodes[INODES_PER_BLOCK];
};

// refs[0] 指向下一组空闲块，其余位置存空闲块号。0 表示没有后续组或该位置为空。
struct treedisk_freelistblock {
    block_no refs[REFS_PER_BLOCK];
};

// 间接块存放子块编号。最下一层的子块才存放文件数据。
struct treedisk_indirblock {
    block_no refs[REFS_PER_BLOCK];
};

// 共用一块 512 字节缓冲区，按实际类型解释读到的数据。
union treedisk_block {
    block_t datablock;
    struct treedisk_superblock superblock;
    struct treedisk_inodeblock inodeblock;
    struct treedisk_freelistblock freelistblock;
    struct treedisk_indirblock indirblock;
};
