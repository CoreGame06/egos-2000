/* 默认文件系统 mydisk（FILESYS=0）。一个文件由 inode 记录大小和首块，FAT 把它的块串成链。
 * 这里用分区内块号，sys_file 的底层接口再加上分区起点，得到整盘块号。
 * mkfs 在宿主机用这套代码创建镜像，sys_file 在 egos 中用它读写文件。
 */
#ifdef MKFS
#include <stdio.h>
#include <sys/types.h>
#define FATAL(msg, ...) do { fprintf(stderr, "FATAL: " msg "\n", ##__VA_ARGS__); exit(1); } while(0)
#else
#include "egos.h"
#endif

#include "inode.h"
#include <stdlib.h>
#include <string.h>
#define DUMMY_DISK_OFFSET(ino, offset) ino * 128 + offset

/* 从分区块 0 起依次是超级块、inode 表、FAT、文件数据块。
 * 默认超级块占 1 块，inode 表占 2 块，FAT 占 32 块，数据区从块 35 开始。
 * 将表缓存在内存里，沿 FAT 查找时可以直接读内存。两张表修改后按块写回。 */
#define FAT_CACHE_MAX 32 // FAT 缓存最多 32 块。
#define INODE_CACHE_MAX 2 // inode 缓存最多 2 块。

static block_t fat_cache[FAT_CACHE_MAX];
static block_t inode_cache[INODE_CACHE_MAX];
static block_t sb_cache;
static uint cache_I,cache_F;
static int cache_ready = 0;

struct inode{
    int size; // 文件长度，单位为块。
    int head; // 文件的第一个数据块，空文件填 -1。文件名保存在目录文件中。
};

struct entry{
    int next; // FAT[k] 记录块 k 的下一块，-1 表示到尾。空闲块也用 FAT 串起来。
};

struct superblock{
    int I;              // inode 表占多少块。
    int F;              // FAT 占多少块。
    int free_list_head; // 空闲链的第一块，分配时取出，回收时放回链头。
};

// 首次访问时通过 below 读入超级块和两张表。整个进程共用一份缓存，访问下层的 inode 0。
static void cache_init(inode_intf below)
{
    if(cache_ready) return;
    below->read(below,0,0,&sb_cache);
    struct superblock* sb = (struct superblock*)sb_cache.bytes;
    if(sb->I > INODE_CACHE_MAX || sb->F>FAT_CACHE_MAX)
        FATAL("mydisk:cache too small for I=%d F=%d",sb->I,sb->F);
    cache_F = sb->F;
    cache_I = sb->I;

    for(uint b=0; b<cache_I;b++) below->read(below,0,1+b,&inode_cache[b]);
    for(uint b=0;b<cache_F;b++) below->read(below,0,1+cache_I+b,&fat_cache[b]);
    cache_ready = 1;
}

// 每块 64 个 inode。用 ino/64 找表块，ino%64 找块内下标，再把该项复制到 *in。
// 例如 inode 65 在分区块 2，块内下标为 1。
int get_inode(inode_intf below, uint below_ino, uint ino, struct inode* in) {
    cache_init(below);
    struct inode* inodes = (struct inode*)inode_cache[ino / 64].bytes;
    *in = inodes[ino % 64];
    return 0;
}

// 用 *in 更新缓存中的 inode，再把它所在的整块写回分区。
int set_node(inode_intf below, uint below_ino, uint ino, struct inode* in) {
    cache_init(below);
    struct inode* inodes = (struct inode*)inode_cache[ino / 64].bytes;
    inodes[ino % 64] = *in;
    return below->write(below, below_ino, 1 + ino / 64, &inode_cache[ino / 64]);
}

// 每块 128 个 FAT 项。用 k/128 和 k%128 找到 FAT[k]，返回它记录的下一块号。
int get_fat(inode_intf below,uint below_ino,struct superblock* sb, uint k)
{
    cache_init(below);
    return ((struct entry*)fat_cache[k/128].bytes)[k%128].next;
}

// 更新 FAT[k] 的下一块号，再写回对应的 FAT 块。FAT 从分区块 1+I 开始。
int set_fat(inode_intf below,uint below_ino,struct superblock* sb, uint k,int next)
{
    cache_init(below);
    ((struct entry*)fat_cache[k/128].bytes)[k%128].next =next;
    return below->write(below,below_ino,1+cache_I+k/128,&fat_cache[k/128]);
}
// 读文件 ino 的第 offset 块，结果放到 block。offset 从 0 开始。
// 例如文件的块链是 35 → 40 → 52，offset=2 就从 head 出发沿 FAT 走两步，读分区块 52。
int mydisk_read(inode_intf self, uint ino, uint offset, block_t* block) {

    inode_intf below = self->state;
    cache_init(below);
    struct superblock* sb = (struct superblock*)sb_cache.bytes;

    struct inode in;
    get_inode(below,0,ino,&in);
    if(in.size<=offset) // 已超出文件范围。
        return -1;
    
    int curr = in.head;
    for(int i=0;i<offset;i++)
        curr=get_fat(below,0,sb,curr);   

    below->read(below,0,curr,block);

    return 0;
}

// 写入文件的第 offset 块。若超出文件末尾，先扩到 offset+1 块。
// 例如向空文件写 offset=2，会分配三块，只写入其中第三块。
int mydisk_write(inode_intf self, uint ino, uint offset, block_t* block) {

    inode_intf below = self->state;
    cache_init(below);
    struct superblock* sb = (struct superblock*)sb_cache.bytes;
    struct inode in;
    get_inode(below,0,ino,&in);

    // 先找出旧链尾。扩容时每接一块就更新 tail_block，整次扩容只需找一次旧链尾。
    int tail_block = -1;
    if (in.size > 0) {
        tail_block = in.head;
        for (int i = 0; i < in.size - 1; i++) {
            tail_block = get_fat(below, 0, sb, tail_block);
        }
    }
    // 记录是否增加了块。每次改 FAT 会立即写回，超级块和 inode 在扩容结束后写回。
    int expanded = 0;

    // 从空闲链头取块，接到文件尾。空文件直接把新块记为 head。
    while(in.size<=offset)
    {
        uint new_block = sb->free_list_head;
        if(new_block==-1)
            FATAL("No disk space!");
        sb->free_list_head = get_fat(below,0,sb,new_block);
        set_fat(below,0,sb,new_block,-1);

        if(in.size==0) {
            in.head = new_block;
            tail_block = new_block;
        } else {
            set_fat(below,0,sb,tail_block,new_block);   
            tail_block = new_block;
        }
        ++in.size;
        expanded = 1;
    }
    
    if (expanded) {
        below->write(below,0,0,&sb_cache);
        set_node(below,0,ino,&in);
    }

    // 找到目标分区块，把调用者提供的 512 字节写进去。
    int curr = in.head;
    for(int i=0;i<offset;i++)
        curr=get_fat(below,0,sb,curr);
    below->write(below,0,curr,block);

    return 0;
}

// 返回文件在 inode 中记录的块数。
int mydisk_getsize(inode_intf self, uint ino) {
    inode_intf below = self->state;
    struct inode in;
    get_inode(below,0,ino,&in);
    return in.size;
}

// 调整文件块数，返回调整前的块数。增大时从空闲链取块，缩小时把多余的块还回去。
int mydisk_setsize(inode_intf self, uint ino, uint nblocks) {
    inode_intf below = self->state;
    cache_init(below);
    struct superblock* sb = (struct superblock*)sb_cache.bytes;

    struct inode in;
    get_inode(below,0,ino,&in);   

    int old_size = in.size;
    int tail_block = -1;
    if (in.size > 0) {
        tail_block = in.head;
        for (int i = 0; i < in.size - 1; i++) {
            tail_block = get_fat(below, 0, sb, tail_block);
        }
    }
    int expanded = 0;

    // 与 write 一样，把空闲块逐个接到文件尾。
    while(nblocks>in.size)
    {
        uint new_block = sb->free_list_head;
        if(new_block==-1)
            FATAL("No disk space!");
        sb->free_list_head = get_fat(below,0,sb,new_block);
        set_fat(below,0,sb,new_block,-1);

        if(in.size==0) {
            in.head = new_block;
            tail_block = new_block;
        } else {
            set_fat(below,0,sb,tail_block,new_block);   
            tail_block = new_block;
        }
        ++in.size;
        expanded = 1;
    }
    
    if (expanded) {
        below->write(below,0,0,&sb_cache);
        set_node(below,0,ino,&in);
    }

    // 缩到 0 时归还整条链，把 inode 设为空文件。目录条目仍然保留。
    if(nblocks==0 && old_size > 0)
    {
        int curr = in.head;
        while(curr!=-1)
        {
            // 先记住文件的下一块，再改写当前块的 FAT，把它接到空闲链头。
            int next_block = get_fat(below,0,sb,curr);
            set_fat(below,0,sb,curr,sb->free_list_head);
            sb->free_list_head = curr;
            curr = next_block;
        }
        in.size=0;
        in.head=-1;
        below->write(below,0,0,&sb_cache);
        set_node(below,0,ino,&in);
    }
    // 例如从 4 块缩到 2 块，先把第 2 块标成链尾，再归还原来的第 3、4 块。
    else if (nblocks > 0 && nblocks < old_size)
    {
        int curr = in.head;
        for(int i=0;i<nblocks-1;i++)
            curr = get_fat(below,0,sb,curr);
        int first_to_free = get_fat(below,0,sb,curr);
        set_fat(below,0,sb,curr,-1);
        curr = first_to_free;
        for(int i=nblocks;i<old_size;i++)
        {
            int next_block = get_fat(below,0,sb,curr);
            set_fat(below,0,sb,curr,sb->free_list_head);
            sb->free_list_head = curr;
            curr = next_block;
        }
        in.size = nblocks;
        below->write(below,0,0,&sb_cache);
        set_node(below,0,ino,&in);
    }

    return old_size;
}

// mkfs 用这里写出初始的超级块、inode 表和 FAT。ninodes 是要预留的 inode 数。
int mydisk_create(inode_intf below, uint below_ino, uint ninodes) {
    uint N = below->getsize(below,below_ino); // 分区总块数。
    // 每块装 64 个 inode 或 128 个 FAT 项。计算占用块数时向上取整。
    uint I = (ninodes+63)/64;
    uint F = (N+127)/128;
    block_t block;
    memset(&block,0,sizeof(block_t));

    // 第 0 块记录表的大小和第一个空闲块的位置。
    struct superblock* sb = (struct superblock*)block.bytes;
    sb->F = F;
    sb->I = I;
    sb->free_list_head = F+I+1;
    below->write(below,below_ino,0,&block);

    // 初始时每个 inode 都表示空文件。
    for(uint b=1;b<=I;b++)
    {
        block_t block;
        struct inode* inodes = (struct inode*)block.bytes;

        for(int i=0;i<64;i++)
        {
            inodes[i].size = 0;
            inodes[i].head = -1;
        }

        below->write(below,below_ino,b,&block);
    }
    // 把数据区串成空闲链。默认是 35 → 36 → … → 4095，最后一项填 -1。
    for(uint b =I+1;b<=I+F;b++)
    {
        block_t block;
        struct entry* entries = (struct entry*)block.bytes;

        for(int i=0;i<128;i++)
        {
            uint k = (b-(I+1)) * 128 + i;
            if(k>=1+I+F && k<N-1)
                entries[i].next=k+1;
            else
                entries[i].next =-1;
        }
        below->write(below,below_ino,b,&block);
    }
    return 0;
}

// 登记读写接口，state 保存下层接口。磁盘上的表等到首次访问时再读。
// 本实现固定访问下层的 inode 0。
inode_intf mydisk_init(inode_intf below, uint below_ino) {

    inode_intf self = malloc(sizeof(struct inode_store));
    self->getsize   = mydisk_getsize;
    self->setsize   = mydisk_setsize;
    self->read      = mydisk_read;
    self->write     = mydisk_write;
    self->state     = below;
    return self;
}
