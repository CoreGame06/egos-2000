/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: a simple (if not naive) file system
 */

#ifdef MKFS
#include <stdio.h>
#include <sys/types.h>
#define FATAL(msg) do { fprintf(stderr, "FATAL: %s\n", msg); exit(1); } while(0)
#else
#include "egos.h"
#endif

#include "inode.h"
#include <stdlib.h>
#include <string.h>
#define DUMMY_DISK_OFFSET(ino, offset) ino * 128 + offset

/* Student's code goes here (File System). */
struct inode{
    int size;
    int head;
};

struct entry{
    int next;
};

struct superblock{
    int I;
    int F;
    int free_list_head;
};

int get_inode(inode_intf below,uint below_ino, uint ino,struct inode* in)
{
    int block_num = 1 + (ino/64);
    int offset = ino % 64 ;
    
    block_t block;
    below->read(below,below_ino,block_num,&block);

    struct inode* inodes = (struct inode*)block.bytes;
    *in = inodes[offset];
    return 0;
}

// 在外部修改好后调用此函数保存修改
int set_node(inode_intf below,uint below_ino, uint ino,struct inode* in)
{
    int block_num = 1 + (ino/64);
    int offset = ino % 64 ;
    
    block_t block;
    below->read(below,below_ino,block_num,&block);
    struct inode* inodes = (struct inode*)block.bytes;
    inodes[offset]=*in;

    below->write(below,below_ino,block_num,&block);

    return 0;
}

int get_fat(inode_intf below,uint below_ino,struct superblock* sb, uint k)
{
    uint block_num = 1 + sb->I + (k/128);
    uint offset = k % 128;

    block_t block;
    below->read(below,below_ino,block_num,&block);
    struct entry* entries = (struct entry*)block.bytes;
    return entries[offset].next;
}

int set_fat(inode_intf below,uint below_ino,struct superblock* sb, uint k,int next)
{
    uint block_num = 1 + sb->I + (k/128);
    uint offset = k % 128;

    block_t block;
    below->read(below,below_ino,block_num,&block);
    struct entry* entries = (struct entry*)block.bytes;   
    entries[offset].next = next;

    below->write(below,below_ino,block_num,&block);
    return 0;
}
/* Add a header file similar to file1.h, which defines data structures in your
 * file system. Include this header file and other header files you need, such
 * as string.h. Define helper functions shared by the file system functions. */

/* Student's code ends here. */
// 文件系统的中间层，通过更换这个来更换文件系统的核心实现
int mydisk_read(inode_intf self, uint ino, uint offset, block_t* block) {
    /* Student's code goes here (File System). */

    /* Replace the code below with your own file system read logic. */
    inode_intf below = self->state;
    
    block_t sb_block;
    below->read(below,0,0,&sb_block);
    struct superblock* sb = (struct superblock*)sb_block.bytes;

    struct inode in;
    get_inode(below,0,ino,&in);
    if(in.size<=offset)
        return -1;
    
    int curr = in.head;
    for(int i=0;i<offset;i++)
        curr=get_fat(below,0,sb,curr);   

    below->read(below,0,curr,block);

    return 0;
    /* Student's code ends here. */
}

int mydisk_write(inode_intf self, uint ino, uint offset, block_t* block) {
    /* Student's code goes here (File System). */

    /* Replace the code below with your own file system write logic. */
    inode_intf below = self->state;

    block_t sb_block;
    below->read(below,0,0,&sb_block);
    struct superblock* sb = (struct superblock*)sb_block.bytes;
    struct inode in;
    get_inode(below,0,ino,&in);

    // 扩容
    while(in.size<=offset)
    {
        uint new_block = sb->free_list_head;
        if(new_block==-1)
            FATAL("No disk space!");
        sb->free_list_head = get_fat(below,0,sb,new_block);
        set_fat(below,0,sb,new_block,-1);

        if(in.size==0)
            in.head = new_block;
        else
        {
            int curr = in.head;
            for(int i=0;i<in.size-1;i++)
                curr=get_fat(below,0,sb,curr);
            set_fat(below,0,sb,curr,new_block);   
        }
        ++in.size;
        below->write(below,0,0,&sb_block);
        set_node(below,0,ino,&in);
    }
    int curr = in.head;
    for(int i=0;i<offset;i++)
        curr=get_fat(below,0,sb,curr);
    below->write(below,0,curr,block);

    return 0;
    /* Student's code ends here. */
}

int mydisk_getsize(inode_intf self, uint ino) {
    /* Student's code goes here (File System). */
    inode_intf below = self->state;
    struct inode in;
    get_inode(below,0,ino,&in);
    return in.size;
    /* Student's code ends here. */
}

int mydisk_setsize(inode_intf self, uint ino, uint nblocks) {
    /* Student's code goes here (File System). */
    inode_intf below = self->state;

    block_t sb_block;
    below->read(below,0,0,&sb_block);
    struct superblock* sb = (struct superblock*)sb_block.bytes;

    struct inode in;
    get_inode(below,0,ino,&in);   

    int old_size = in.size;
    // 扩容
    while(nblocks>in.size)
    {
        uint new_block = sb->free_list_head;
        if(new_block==-1)
            FATAL("No disk space!");
        sb->free_list_head = get_fat(below,0,sb,new_block);
        set_fat(below,0,sb,new_block,-1);

        if(in.size==0)
            in.head = new_block;
        else
        {
            int curr = in.head;
            for(int i=0;i<in.size-1;i++)
                curr=get_fat(below,0,sb,curr);
            set_fat(below,0,sb,curr,new_block);   
        }
        ++in.size;
        below->write(below,0,0,&sb_block);
        set_node(below,0,ino,&in);
    }
    // nblocks = 0
    if(nblocks==0)
    {
        int curr = in.head;
        while(curr!=-1)
        {
            int next_block = get_fat(below,0,sb,curr);
            set_fat(below,0,sb,curr,sb->free_list_head);
            sb->free_list_head = curr;
            curr = next_block;
        }
        in.size=0;
        in.head=-1;
        below->write(below,0,0,&sb_block);
        set_node(below,0,ino,&in);
    }
    // 其余不为0，且小于原来大小的情况
    else
    {
        int curr = in.head;
        for(int i=0;i<nblocks-1;i++)
            curr = get_fat(below,0,sb,curr);
        int first_to_free = get_fat(below,0,sb,curr);
        set_fat(below,0,sb,curr,-1);
        curr = first_to_free;
        for(int i=nblocks;i<in.size;i++)
        {
            int next_block = get_fat(below,0,sb,curr);
            set_fat(below,0,sb,curr,sb->free_list_head);
            sb->free_list_head = curr;
            curr = next_block;
        }
        in.size = nblocks;
        below->write(below,0,0,&sb_block);
        set_node(below,0,ino,&in);
    }

    return old_size;
    /* Student's code ends here. */
}

int mydisk_create(inode_intf below, uint below_ino, uint ninodes) {
    /* Student's code goes here (File System). */
    uint N = below->getsize(below,below_ino); // 磁盘块
    //计算I
    uint I = (ninodes+63)/64; // 向上取整
    //计算F
    uint F = (N+127)/128;
    /* Create the initial on-disk data structures for your file system. */
    block_t block;
    memset(&block,0,sizeof(block_t));

    struct superblock* sb = (struct superblock*)block.bytes; //将char* 转换为superblock*
    sb->F = F;
    sb->I = I;
    sb->free_list_head = F+I+1;
    below->write(below,below_ino,0,&block);

    // 初始化 inode块
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
    // 初始化 FAT块，实现 FAT[35]=36
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
    /* Student's code ends here. */
    return 0;
}

inode_intf mydisk_init(inode_intf below, uint below_ino) {
    /* Student's code goes here (File System). */

    /* Feel free to add or modify anything here. */
    inode_intf self = malloc(sizeof(struct inode_store));
    self->getsize   = mydisk_getsize;
    self->setsize   = mydisk_setsize;
    self->read      = mydisk_read;
    self->write     = mydisk_write;
    self->state     = below;
    return self;
    /* Student's code ends here. */
}
