/* Author: Robbert van Renesse, August 2015, updated November 2015
 *
 * 备用文件系统 treedisk（FILESYS=1）。每个文件用一棵树索引数据块，磁盘结构见 file1.h。
 * below 和 below_ino 指定承载文件系统的下层存储，ino 是本层文件号。
 * 阅读时先看 treedisk_read 怎样查块，再看 treedisk_write 怎样补齐路径和增加树高。
 */
#include <stdlib.h>
#include <string.h>

#ifdef MKFS
#include <stdio.h>
#include <sys/types.h>
#else
#include "egos.h"
#endif

#include "file1.h"

// 一次读写用到的超级块、inode 所在块和块内指针。
struct treedisk_snapshot {
    union treedisk_block superblock;
    union treedisk_block inodeblock;
    block_no inode_blockno;
    struct treedisk_inode* inode;
};

// 文件系统接口保存的下层存储信息。
struct treedisk_state {
    inode_intf below; // 下层块读写接口。
    uint below_ino;   // 文件系统存放在下层的哪个 inode。
    uint ninodes;
};

static uint log_rpb;       // 一层索引使用的位数，128 个分支对应 7 位。
static block_t null_block; // 用于初始化空 inode 的全零块。

static void panic(const char* s) {
#ifdef MKFS
    fprintf(stderr, "%s", s);
    exit(1);
#else
    FATAL(s);
#endif
}

// 右移。如果移位数已达到类型宽度，直接返回 0，避免未定义行为。
static block_no log_shift_r(block_no x, uint nbits) {
    if (nbits >= sizeof(block_no) * 8) {
        return 0;
    }
    return x >> nbits;
}

// 读入超级块和指定 inode 所在的块，再记录该 inode 在缓冲区里的位置。
static int treedisk_get_snapshot(struct treedisk_snapshot* snapshot,
                                 struct treedisk_state* ts, uint inode_no) {
    if ((*ts->below->read)(ts->below, ts->below_ino, 0,
                           (block_t*)&snapshot->superblock) < 0)
        return -1;

    // 超级块给出 inode 表有多少块，据此检查编号范围。
    if (inode_no >=
        snapshot->superblock.superblock.n_inodeblocks * INODES_PER_BLOCK) {
        printf("!!TDERR: inode number too large %u %u\n", inode_no,
               snapshot->superblock.superblock.n_inodeblocks);
        return -1;
    }

    // inode 表从块 1 开始。除法找到表块，取余找到块内的那一项。
    snapshot->inode_blockno = 1 + inode_no / INODES_PER_BLOCK;
    if ((*ts->below->read)(ts->below, ts->below_ino, snapshot->inode_blockno,
                           (block_t*)&snapshot->inodeblock) < 0)
        return -1;

    snapshot->inode =
        &snapshot->inodeblock.inodeblock.inodes[inode_no % INODES_PER_BLOCK];
    return 0;
}

// 从超级块指向的空闲表取一块。空闲块号按组存放，每组的第 0 项指向下一组。
static block_no treedisk_alloc_block(struct treedisk_state* ts,
                                     struct treedisk_snapshot* snapshot) {
    block_no b;

    if ((b = snapshot->superblock.superblock.free_list) == 0)
        panic("treedisk_alloc_block: inode store is full\n");

    // 读取当前组，从末尾找一个尚未取走的块号。
    union treedisk_block freelistblock;
    if ((*ts->below->read)(ts->below, ts->below_ino, b,
                           (block_t*)&freelistblock) < 0) {
        panic("treedisk_alloc_block");
    }
    uint i;
    for (i = REFS_PER_BLOCK; --i > 0;)
        if (freelistblock.freelistblock.refs[i] != 0) {
            break;
        }

    // 组里还有空闲块号就取走一项。取完后，组本身也可分配，超级块的指针移到下一组。
    block_no free_blockno;
    if (i == 0) {
        free_blockno = b;
        snapshot->superblock.superblock.free_list =
            freelistblock.freelistblock.refs[0];
        if ((*ts->below->write)(ts->below, ts->below_ino, 0,
                                (block_t*)&snapshot->superblock) < 0) {
            panic("treedisk_alloc_block: superblock");
        }
    } else {
        free_blockno = freelistblock.freelistblock.refs[i];
        freelistblock.freelistblock.refs[i] = 0;
        if ((*ts->below->write)(ts->below, ts->below_ino, b,
                                (block_t*)&freelistblock) < 0) {
            panic("treedisk_alloc_block: freelistblock");
        }
    }

    return free_blockno;
}

// 读取文件 inode 中记录的块数。
static int treedisk_getsize(inode_intf self, uint ino) {
    struct treedisk_state* ts = self->state;
    struct treedisk_snapshot snapshot;
    if (treedisk_get_snapshot(&snapshot, ts, ino) < 0) return -1;

    return snapshot.inode->nblocks;
}

// 当前实现由 write 扩大文件，setsize 尚未实现。
static int treedisk_setsize(inode_intf self, uint ino, block_no nblocks) {
    panic("treedisk_setsize not implemented");
    return -1;
}

// 读取文件 ino 的第 offset 块。offset 从 0 开始，结果写入 block。
static int treedisk_read(inode_intf self, uint ino, block_no offset,
                         block_t* block) {
    struct treedisk_state* ts = self->state;

    struct treedisk_snapshot snapshot;
    if (treedisk_get_snapshot(&snapshot, ts, ino) < 0) return -1;

    // 文件末尾之外返回错误。文件内部尚未分配的块按空洞处理。
    if (offset >= snapshot.inode->nblocks) {
        printf("!!TDERR: offset too large %u %u\n", offset,
               snapshot.inode->nblocks);
        return -1;
    }

    // 用最后一个逻辑块号 nblocks-1 计算间接层数。1 块时直接读根，2～128 块时需要一层索引。
    uint nlevels = 0;
    if (snapshot.inode->nblocks > 0)
        while (log_shift_r(snapshot.inode->nblocks - 1, nlevels * log_rpb) !=
               0) {
            nlevels++;
        }

    // 从 inode.root 开始往下找，直到读到数据块。
    block_no b = snapshot.inode->root;
    for (;;) {
        // 任何一层的块号为 0 都表示这条路径尚未分配，返回全零。
        if (b == 0) {
            memset(block, 0, BLOCK_SIZE);
            return 0;
        }

        // 先把当前块读到 block。间接层数减完时，这次读到的就是文件数据。
        int result = (*ts->below->read)(ts->below, ts->below_ino, b, block);
        if (result < 0) return result;
        if (nlevels == 0) return 0;

        // 每 7 位选一个分支，从高位往低位。若有两层，offset=130 依次走下标 1、2。
        nlevels--;
        struct treedisk_indirblock* tib = (struct treedisk_indirblock*)block;
        uint index = log_shift_r(offset, nlevels * log_rpb) % REFS_PER_BLOCK;
        b          = tib->refs[index];
    }
    return 0;
}

// 写入文件的第 offset 块。超出原长度时扩大文件，并为实际写到的位置分配块。
static int treedisk_write(inode_intf self, uint ino, block_no offset,
                          block_t* block) {
    struct treedisk_state* ts = self->state;
    uint dirty_inode          = 0;

    struct treedisk_snapshot snapshot_buffer;
    struct treedisk_snapshot* snapshot = &snapshot_buffer;
    if (treedisk_get_snapshot(snapshot, ts, ino) < 0) return -1;

    // 根据旧文件大小算出当前有几层间接块。
    uint nlevels = 0;
    if (snapshot->inode->nblocks > 0)
        while (log_shift_r(snapshot->inode->nblocks - 1, nlevels * log_rpb) !=
               0) {
            nlevels++;
        }

    // 写到末尾之后时，把文件扩大到 offset+1 块，并计算新的层数。
    uint nlevels_after;
    if (offset >= snapshot->inode->nblocks) {
        snapshot->inode->nblocks = offset + 1;
        dirty_inode              = 1;
        nlevels_after            = 0;
        while (log_shift_r(offset, nlevels_after * log_rpb) != 0) {
            nlevels_after++;
        }
    } else {
        nlevels_after = nlevels;
    }

    // 高度不够就在旧根上方加新根，并把旧根放在新根的第 0 项，保留原来较低块号的路径。
    if (snapshot->inode->nblocks == 0) {
        nlevels = nlevels_after;
    } else if (nlevels_after > nlevels) {
        while (nlevels_after > nlevels) {
            block_no indir = treedisk_alloc_block(ts, snapshot);

            struct treedisk_indirblock tib;
            memset(&tib, 0, BLOCK_SIZE);
            tib.refs[0]           = snapshot->inode->root;
            snapshot->inode->root = indir;
            dirty_inode           = 1;
            if ((*ts->below->write)(ts->below, ts->below_ino, indir,
                                    (block_t*)&tib) < 0) {
                panic("treedisk_write: indirect block");
            }

            nlevels++;
        }
    }

    // 文件大小或根块改变时，把 inode 所在的整块写回。
    if (dirty_inode)
        if ((*ts->below->write)(ts->below, ts->below_ino,
                                snapshot->inode_blockno,
                                (block_t*)&snapshot->inodeblock) < 0) {
            panic("treedisk_write: inode block");
        }

    // 沿目标路径往下查找。parent_no 是上一级存放子块号的位置，分配后要把上一级写回。
    block_no b;
    block_no* parent_no   = &snapshot->inode->root;
    block_no parent_off   = snapshot->inode_blockno;
    block_t* parent_block = (block_t*)&snapshot->inodeblock;
    for (;;) {
        // 遇到空引用时才分配块。新建的间接块先清零，其它分支仍是空洞。
        struct treedisk_indirblock tib;
        if ((b = *parent_no) == 0) {
            b = *parent_no = treedisk_alloc_block(ts, snapshot);
            if ((*ts->below->write)(ts->below, ts->below_ino, parent_off,
                                    parent_block) < 0)
                panic("treedisk_write: parent");
            if (nlevels == 0) break;
            memset(&tib, 0, BLOCK_SIZE);
        } else {
            if (nlevels == 0) break;
            if ((*ts->below->read)(ts->below, ts->below_ino, b,
                                   (block_t*)&tib) < 0)
                panic("treedisk_write");
        }

        // 用 offset 的这一组位选出子块，下一轮从这项继续。
        nlevels--;
        uint index   = log_shift_r(offset, nlevels * log_rpb) % REFS_PER_BLOCK;
        parent_no    = &tib.refs[index];
        parent_block = (block_t*)&tib;
        parent_off   = b;
    }

    // 找到叶子后，写入调用者提供的整块数据。
    if ((*ts->below->write)(ts->below, ts->below_ino, b, block) < 0)
        panic("treedisk_write: data block");
    return 0;
}

// 保存下层存储位置，返回读写已有文件系统的接口。
inode_intf treedisk_init(inode_intf below, uint below_ino) {
    // 首次计算一层索引使用多少位。
    if (log_rpb == 0) {
        do {
            log_rpb++;
        } while (((REFS_PER_BLOCK - 1) >> log_rpb) != 0);
    }

    struct treedisk_state* ts = malloc(sizeof(struct treedisk_state));
    memset(ts, 0, sizeof(struct treedisk_state));
    ts->below     = below;
    ts->below_ino = below_ino;

    // 登记块操作函数。
    inode_intf self = malloc(sizeof(struct inode_store));
    memset(self, 0, sizeof(struct inode_store));
    self->state   = ts;
    self->getsize = treedisk_getsize;
    self->setsize = treedisk_setsize;
    self->read    = treedisk_read;
    self->write   = treedisk_write;
    return self;
}

// 把 next_free 到 nblocks-1 的块分组。每组取一块保存其余块号，并把新组接到链头。
block_no setup_freelist(inode_intf below, uint below_ino, block_no next_free,
                        block_no nblocks) {
    block_no freelist_data[REFS_PER_BLOCK];
    block_no freelist_block = 0;
    uint i;

    while (next_free < nblocks) {
        freelist_data[0] = freelist_block;
        freelist_block   = next_free++;
        for (i = 1; i < REFS_PER_BLOCK && next_free < nblocks; i++)
            freelist_data[i] = next_free++;

        for (; i < REFS_PER_BLOCK; i++) freelist_data[i] = 0;

        if ((*below->write)(below, below_ino, freelist_block,
                            (block_t*)freelist_data) < 0)
            panic("treedisk_setup_freelist");
    }
    return freelist_block;
}

// mkfs 创建初始文件系统。第 0 块放超级块，后面预留 inode 表，其余空间组成空闲表。
int treedisk_create(inode_intf below, uint below_ino, uint ninodes) {
    if (sizeof(union treedisk_block) != BLOCK_SIZE) {
        panic("treedisk_create: block has wrong size");
    }

    // inode 数向上取整成块数。
    uint n_inodeblocks = (ninodes + INODES_PER_BLOCK - 1) / INODES_PER_BLOCK;

    // 检查空间，至少要放下超级块、inode 表和一块空闲空间。
    uint nblocks = (*below->getsize)(below, below_ino);
    if (nblocks < n_inodeblocks + 2) {
        printf("treedisk_create: too few blocks\n");
        return -1;
    }

    // 用超级块的 inode 表块数判断镜像是否已经初始化。
    union treedisk_block superblock;
    if ((*below->read)(below, below_ino, 0, (block_t*)&superblock) < 0)
        return -1;

    if (superblock.superblock.n_inodeblocks == 0) {
        // 建立空闲表，并把链头块号记到超级块。
        union treedisk_block superblock;
        memset(&superblock, 0, BLOCK_SIZE);
        superblock.superblock.n_inodeblocks = n_inodeblocks;
        superblock.superblock.free_list =
            setup_freelist(below, below_ino, n_inodeblocks + 1, nblocks);
        if ((*below->write)(below, below_ino, 0, (block_t*)&superblock) < 0)
            return -1;

        // 将 inode 表清零，所有文件从 0 块开始。
        for (uint i = 1; i <= n_inodeblocks; i++)
            if ((*below->write)(below, below_ino, i, &null_block) < 0) {
                return -1;
            }

        printf("treedisk: Created a new filesystem with %d inodes\n", ninodes);
    } else {
        printf("treedisk: a filesystem already exists with %lu inodes",
               superblock.superblock.n_inodeblocks * INODES_PER_BLOCK);
    }

    return 0;
}
