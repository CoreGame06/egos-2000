# File System

> **文件系统**

In the previous project, you learned how to read and write disk blocks; now you will study how to build a file system. A file system uses the disk to provide the abstractions of files and directories (aka. folders), which you have likely been familiar with through your daily interactions with operating systems (e.g., Windows File Explorer or macOS Finder). There are numerous file systems, and many of them are based on an abstraction called an inode. We thus start by explaining this important file system abstraction.

在上一个项目中，你学习了如何读写磁盘块；现在你将学习如何构建一个文件系统。文件系统借助磁盘提供文件和目录（即文件夹）的抽象，你在日常使用操作系统（例如 Windows 资源管理器或 macOS Finder）时，可能已经对这些概念非常熟悉了。目前存在许多种不同的文件系统，其中绝大多数都基于一种称为 inode（索引节点）的抽象。因此，我们将首先介绍这种重要的文件系统抽象。

## Inode and inode store

> **Inode 与 inode 存储**

Consider an inode as a number of blocks and a file system as an array of inodes. Say a file system maintains 1024 inodes, and each inode stores information about a file or a directory. For example, inodes in a file system could be used as follows.

你可以将一个 inode 看作是一组磁盘块，并将文件系统看作是一个 inode 数组。假设某个文件系统维护 1024 个 inode，每个 inode 存储有关某个文件或目录的信息。例如，文件系统中的 inode 可以按如下方式使用：

* Inode #12 holds 1 disk block for a small text file.
* Inode #103 contains 31 disk blocks for the cat executable.
* Inode #781 holds 1 disk block for the /bin directory, recording metadata about all files under /bin. This directory contains executable files for commands such as cat.

* Inode #12 占有 1 个磁盘块，存放一个小型文本文件。
* Inode #103 包含 31 个磁盘块，存放 `cat` 可执行文件。
* Inode #781 占有 1 个磁盘块，用于 `/bin` 目录，记录 `/bin` 下所有文件的元数据。该目录包含诸如 `cat` 等命令的可执行文件。

The mkfs tool provides a simple, concrete example of how inodes are used in egos-2000.

`mkfs` 工具提供了一个简单而具体的示例，展示了如何在 egos-2000 中使用 inode。

### Use of inodes in mkfs

> **Inode 在 mkfs 中的应用**

When you type make install or make qemu in egos-2000, the command compiles and runs tools/`mfks.c`, which produces a disk image file. Below is part of its output.

当你在 egos-2000 中输入 `make install` 或 `make qemu` 时，该命令会编译并运行 `tools/mkfs.c`，从而生成一个磁盘镜像文件。以下是其部分输出：

```shell
> make install
...
[INFO] Load ino=0, 34 bytes
[INFO] Load ino=1, 48 bytes
[INFO] Load ino=2, 26 bytes
[INFO] Load ino=3, 15 bytes
[INFO] Load ino=4, 15 bytes
[INFO] Load ino=5, 349 bytes
[INFO] Load ino=7, cat.elf: 15600 bytes
[INFO] Load ino=8, tcp_demo.elf: 10696 bytes
[INFO] Load ino=9, loop.elf: 11188 bytes
[INFO] Load ino=10, cd.elf: 15644 bytes
[INFO] Load ino=11, video_demo.elf: 9388 bytes
[INFO] Load ino=12, crash1.elf: 16772 bytes
[INFO] Load ino=13, crash2.elf: 9432 bytes
[INFO] Load ino=14, ls.elf: 10408 bytes
[INFO] Load ino=15, udp_demo.elf: 9832 bytes
[INFO] Load ino=16, echo.elf: 10144 bytes
[INFO] Load ino=6, ./   6 ../   0 cat   7 tcp_demo   8 loop   9 cd  10 video_demo  11 crash1  12 crash2  13 ls  14 udp_demo  15 echo  16
[INFO] Finish making the image files
```

Let's take a closer look at these 17 inodes numbered from #0 to #16.

让我们仔细观察一下这 17 个编号为 #0 到 #16 的 inode。

### Inodes for directories

> **用于目录的 Inode**

Inodes #0, #1, #2, #3, #4, and #6 are used for directories. The table below summarizes how the code in tools/`mfks.c` uses these inodes.

Inode #0, #1, #2, #3, #4 和 #6 用于目录。下表总结了 `tools/mkfs.c` 中的代码如何使用这些 inode。

| Inode | Corresponding directory | Bytes stored in the inode |
> | --- | --- | --- |

| #0 | / | ./ 0 ../ 0 home/ 1 bin/ 6 |
> | #1 | /home | ./ 1 ../ 0 yunhao/ 2 rvr/ 3 yacqub/ 4 |

| #2 | /home/yunhao | ./ 2 ../ 1 README 5 |
> | #3 | /home/rvr | ./ 3 ../ 1 |

| #4 | /home/yacqub | ./ 4 ../ 1 |
> | #6 | /bin | ./ 6 ../ 0 cat 7 `tcp_demo` 8 loop 9 cd 10 `video_demo` 11 crash1 12 crash2 13 ls 14 `udp_demo` 15 echo 16 |

| Inode | 对应的目录 | Inode 中存储的字节数（或内容） |
> | --- | --- | --- |

| #0 | / | ./ 0 ../ 0 home/ 1 bin/ 6 |
> | #1 | /home | ./ 1 ../ 0 yunhao/ 2 rvr/ 3 yacqub/ 4 |

| #2 | /home/yunhao | ./ 2 ../ 1 README 5 |
> | #3 | /home/rvr | ./ 3 ../ 1 |

| #4 | /home/yacqub | ./ 4 ../ 1 |
> | #6 | /bin | ./ 6 ../ 0 cat 7 `tcp_demo` 8 loop 9 cd 10 `video_demo` 11 crash1 12 crash2 13 ls 14 `udp_demo` 15 echo 16 |

First, every directory contains ./ and ../, which correspond to itself and its parent directory, respectively. For example, the parent directory of both /bin and /home is /, as illustrated by the ../ 0 part of inodes #1 and #6. In general, egos-2000 uses the format {name} {inode no} ... to represent a directory. If a {name} ends with /, this name would represent a sub-directory (e.g., rvr/ in inode #1). Otherwise, this name represents a file (e.g., cat in inode #6). We use this naming convention for directories because it is simple. In most file systems, inodes record additional information, such as the size or creation time of a file or directory.

首先，每个目录都包含 `./` 和 `../`，分别对应自身和其父目录。例如，`/bin` 和 `/home` 的父目录都是 `/`，如 inode #1 和 #6 中的 `../ 0` 部分所示。通常，egos-2000 使用 `{name} {inode no} ...` 的格式来表示目录内容。如果某个 `{name}` 以 `/` 结尾，则该名称表示一个子目录（例如 inode #1 中的 `rvr/`）；否则，该名称表示一个文件（例如 inode #6 中的 `cat`）。我们采用这种目录命名约定是因为它足够简单。在大多数实际的文件系统中，inode 还会记录额外的元数据信息，例如文件或目录的大小、创建时间等。

All the inodes in this table contain fewer than 512 bytes, so each occupies only one block.

上表中的所有 inode 包含的数据均少于 512 字节，因此每个 inode 仅占用一个磁盘块。

### Inodes for files

> **用于文件的 Inode**

Inodes #5 and #7..#16 are used for files. For example, as shown in the table, inode #5 is for /home/yunhao/README, which is a short text file. The content of this text file is defined by the 349 bytes of contents[5] in tools/`mkfs.c`. Since 349 is smaller than the 512-byte block size, inode #5 also contains only one disk block for this README file.

Inode #5 以及 #7..#16 用于文件。例如，如上表所示，inode #5 对应 `/home/yunhao/README`，这是一个简短的文本文件。该文本文件的内容由 `tools/mkfs.c` 中 `contents[5]` 的 349 个字节定义。由于 349 小于 512 字节的块大小，因此 inode #5 也仅为这个 README 文件占用一个磁盘块。

As shown in the mkfs output above, inodes #7..#16 are used for the binary executable files of user commands, and they span multiple disk blocks because each executable file is larger than 512 bytes. For example, the echo command corresponds to inode #16, which contains 10144 bytes (i.e., ceil(10144/512) = 20 disk blocks).

如前面的 `mkfs` 输出所示，inode #7..#16 用于用户命令的二进制可执行文件。由于每个可执行文件都大于 512 字节，它们会跨越多个磁盘块。例如，`echo` 命令对应 inode #16，包含 10144 字节（即 $$\lceil 10144 / 512 \rceil$ = 20$ 个磁盘块）。

Lastly, NINODES is defined as 128 in library/file/`inode.h`, and mkfs uses NINODES to initialize the file system. This means the file system supports 128 inodes in total, but only 17 are used when egos-2000 starts to boot.

最后，`NINODES` 在 `library/file/inode.h` 中被定义为 128，`mkfs` 使用 `NINODES` 来初始化文件系统。这意味着该文件系统总共支持 128 个 inode，但在 egos-2000 启动时仅使用了其中的 17 个。

> [!TIP]
> Our goal is to give you a concrete and minimal example of how a file system could be organized as an array of inodes. Make sure that you see how different inodes can hold different numbers of disk blocks, and how an inode can be used as either a file or a directory. In the rest of P6, we will focus on the inode abstraction and ignore how a file system maps a file or directory to an inode number.

> [!TIP]
> > 我们的目标是为你提供一个具体而精简的示例，展示如何将文件系统组织为一个 inode 数组。请务必理清不同的 inode 如何包含不同数量的磁盘块，以及同一个 inode 机制如何既能表示文件又能表示目录。在 P6 的后续内容中，我们将专注于 inode 级别的抽象，暂不关注文件系统如何将文件或目录路径映射到 inode 编号。

### The inode store interface

> **inode 存储接口**

In addition to NINODES, library/file/`inode.h` defines the `inode_intf` type.

除了 `NINODES` 之外，`library/file/inode.h` 还定义了 `inode_intf` 类型。

```c
typedef struct inode_store* inode_intf; /* inode store interface */

struct inode_store {
    int (*getsize)(inode_intf self, uint ino);
    int (*setsize)(inode_intf self, uint ino, uint newsize);
    int (*read)(inode_intf self, uint ino, uint offset, block_t* block);
    int (*write)(inode_intf self, uint ino, uint offset, block_t* block);
    void* state;
};
```

In short, `inode_store` defines the file system interface in egos-2000, and there are two file systems (i.e., `file0.c` and `file1.c` in library/file) that implement this interface. Each file system implements 4 functions: read, write, getsize, and setsize, and maintains an in-memory state. A pointer needs 4 bytes, so a struct `inode_store` holds 20 bytes, and the first 16 bytes contain the addresses of the 4 interface functions (i.e., the address of their first instruction).

简而言之，`inode_store` 定义了 egos-2000 中的文件系统接口，有两个文件系统实现（即 `library/file` 中的 `file0.c` 和 `file1.c`）实现了该接口。每个文件系统实现 4 个函数指针：`read`、`write`、`getsize` 和 `setsize`，并维护一份内存中的状态（state）。在 32 位系统上一个指针占用 4 字节，因此 `struct inode_store` 结构体共占用 20 字节，前 16 字节保存这 4 个接口函数的入口地址（即其首条指令的内存地址）。

If you are familiar with object-oriented programming, you may consider `inode_store` an interface (aka template), and a file system a class with a local variable and four methods that implement this interface. This explains the self argument of the interface functions, so a file system can access its in-memory state using self->state. In C, self is 4 bytes, the address of the 20-byte struct `inode_store`.

如果你熟悉面向对象编程，可以将 `inode_store` 视为一个接口（或抽象类），而将具体的文件系统实现视为一个包含内部状态变量和四个实现接口方法的类。这就解释了为什么接口函数中都有一个 `self` 参数，使得文件系统能够通过 `self->state` 访问其内存状态。在 C 语言中，`self` 是一个 4 字节的指针，指向这个 20 字节的 `struct inode_store`。

For read and write, there are 3 more arguments: ino is the inode number, offset is the block number, and block is the memory address of 512 bytes holding the block data. For example, as we have seen, the echo executable maps to inode #16, which contains 20 blocks. We can thus read inode #16 with an offset in the range 0..19.

对于 `read` 和 `write`，还有另外 3 个参数：`ino` 为 inode 编号；`offset` 为逻辑块号（即块偏移量）；`block` 为指向 512 字节数据块的内存地址。例如，如前所述，`echo` 可执行文件映射到 inode #16，共包含 20 个块。因此我们可以读取 inode #16 中 `offset` 范围在 0..19 内的各个数据块。

```c
// block_t defined in library/file/disk.h
#define BLOCK_SIZE 512
typedef struct block {
    char bytes[BLOCK_SIZE];
} block_t;

// This block variable holds 512 bytes in the memory.
block_t block;
// Given an `inode_intf fs`, read block #4 from the 20 blocks of inode #16.
fs->read(fs, 16, 4, &block);
```

### A simple file system

> **一个简单文件系统**

We now explain a simple file system in library/file/`file0.c` that implements the inode store interface `inode_intf`. We start with the `mydisk_init` function.

我们现在解释 `library/file/file0.c` 中的一个简单文件系统，它实现了 inode 存储接口 `inode_intf`。我们从 `mydisk_init` 函数开始：

```c
inode_intf mydisk_init(inode_intf below, uint below_ino) {
    inode_intf self = malloc(sizeof(struct inode_store));
    self->getsize   = mydisk_getsize;
    self->setsize   = mydisk_setsize;
    self->read      = mydisk_read;
    self->write     = mydisk_write;
    self->state     = below;
    return self;
}
```

The code allocates 20 bytes for the inode store interface, and sets the 4 interface functions as the `` functions in `file0.c`. Then, below is recorded as self->state since the file system needs it for reading and writing the disk. Take a look at apps/system/`sys_file.c`, where there is another inode store called disk, which simply reads or writes the disk with an offset, ignoring the inode number. A pointer to this disk variable is passed to `mydisk_init` as the below argument (i.e., `mydisk_init`(&disk, 0) in `sys_file.c`).

该代码为 inode 存储接口动态分配 20 个字节，并将 4 个接口函数指针分别绑定为 `file0.c` 中的 ```` 函数。随后将 `below` 记录到 `self->state` 中，因为上层文件系统底层需要通过它来读写磁盘。翻阅 `apps/system/sys_file.c` 可以看到，那里定义了另一个名为 `disk` 的底层 inode 存储，它忽略 inode 编号，仅根据块偏移量直接读写底层磁盘。指向该 `disk` 变量的指针作为 `below` 参数传递给 `mydisk_init`（即 `sys_file.c` 中的 `mydisk_init(&disk, 0)`）。

After assigning &disk to self->state in the `mydisk_init` function, the file system read and write functions use self->state to access the disk as follows.

在 `mydisk_init` 函数中将 `&disk` 赋值给 `self->state` 后，文件系统的读写函数使用 `self->state` 访问磁盘，如下所示：

```c
#define DUMMY_DISK_OFFSET(ino, offset) ino * 128 + offset

int mydisk_read(inode_intf self, uint ino, uint offset, block_t* block) {
    inode_intf below = self->state;
    return below->read(below, 0, DUMMY_DISK_OFFSET(ino, offset), block);
}

int mydisk_write(inode_intf self, uint ino, uint offset, block_t* block) {
    inode_intf below = self->state;
    return below->write(below, 0, DUMMY_DISK_OFFSET(ino, offset), block);
}
```

This simple file system assumes that every inode holds at most 128 blocks, and operations to (ino, offset) are directly translated to reading or writing disk block ino*128+offset. This is probably the simplest way to implement the inode store interface and support a file system that is barely enough to boot egos-2000.

这个简单文件系统假设每个 inode 最多包含 128 个块，对 `(ino, offset)` 的访问操作直接映射并转换为对磁盘块 `ino * 128 + offset` 的读写。这可以说是实现 inode 存储接口、并为 egos-2000 提供最基础启动支持的最简方式。

> [!TIP]
> The `mydisk_getsize` and `mydisk_setsize` functions simply print an error and halt, so they are not used during the boot of egos-2000. You will start from here and implement your own file system in the `` functions in `file0.c`. Make sure to set FILESYS = 0 in the `Makefile` so that mkfs and egos-2000 run your own file system code. In particular, after make install, you should see MKFS is using *mydisk* instead of MKFS is using *treedisk*.

> [!TIP]
> > `mydisk_getsize` 和 `mydisk_setsize` 函数目前仅打印错误信息并终止执行，因此在 egos-2000 的启动过程中并未被调用。你将以此为起点，在 `file0.c` 的 ```` 函数中实现你自己的文件系统。请务必在 `Makefile` 中设置 `FILESYS = 0`，以便 `mkfs` 和 egos-2000 使用你实现的文件系统代码。特别是在运行 `make install` 后，终端应提示 `MKFS is using *mydisk*` 而非 `MKFS is using *treedisk*`。

## A linked-list-based file system

> **基于链表的文件系统**

Your job in this project is to implement some linked list data structures for the inode store interface functions. When you modify such data structures, you typically need to read a disk block into memory, apply your modifications in the memory, and write the block back to the disk. The data structures involved here are not complicated, but students often forget to apply their modifications back to disk, leading to subtle bugs.

你在本项目中的任务是为 inode 存储接口函数实现相应基于链表的数据结构。在修改此类磁盘数据结构时，典型流程是：将对应的磁盘块读取到内存中，在内存缓冲区中完成修改，然后再将该磁盘块写回磁盘。这里涉及的数据结构本身并不复杂，但同学们常常会漏掉“写回磁盘”这一步，从而导致极为隐蔽的错误（bug）。

Regarding the data structure, you will implement a few linked lists similar to how the FAT file system is organized. We now explain how you could maintain one linked list for each inode, and one linked list for all the unused blocks on the disk.

在数据结构设计上，你将实现类似于 FAT（文件分配表）文件系统组织方式的链表。接下来我们将解释如何为每个 inode 维护一条数据块链表，以及如何为磁盘上所有未使用的空闲块维护一条空闲链表。

### Linked list

> **链表**

Suppose inode #28 has 3 blocks on the disk: block #100, #202, and #432. We can then use a simple linked list data structure to represent this inode as follows.

假设 inode #28 在磁盘上占用了 3 个块：块 #100、#202 和 #432。我们可以使用如下简单的链表数据结构来表示该 inode：

```c
struct inode {
    int size, head;
} inodes[128];

inodes[28].size = 3;
> inodes[28].head = 100;

struct entry {
    int next;
} FAT_entry[DISK_SIZE / BLOCK_SIZE];

FAT_entry[100]  = 202;
FAT_entry[202]  = 432;
FAT_entry[432]  = -1;
```

We call it `FAT_entry` because it is similar to how the FAT file system maintains a linked list. However, if you define inodes and `FAT_entry` in C like above, they will be in memory. Instead, we should write these two arrays to disk so that the file system information remains on disk after we power off the computer.

我们将该数组命名为 `FAT_entry`，因为它的工作机制类似于 FAT 文件系统中的表项链表。不过，如果像上面代码那样直接在 C 语言中申明 `inodes` 和 `FAT_entry` 全局变量，它们只会保存在内存中。我们必须将这两个结构化数组持久化写入磁盘，这样即使计算机断电关机，文件系统的信息也能完整保留在磁盘上。

### Initial disk layout

> **初始磁盘布局**

This picture shows how you could store linked list data structures on disk.

下图展示了如何在磁盘上存储链表数据结构。

We split the disk into 4 regions. The first block is called the superblock. The following I-blocks are called inode blocks and hold the inodes array. The next F-blocks are called FAT table blocks and hold the `FAT_entry` array. All the other blocks starting from I+F+1 are used as data blocks. Since the size of `struct inode` and `struct entry` are 8 and 4 bytes, respectively, a block can hold either 64 `struct inode` or 128 `struct entry`.

我们将磁盘划分为 4 个区域：
* 第 0 个块称为超级块（superblock）；
* 接下来的 I 个块称为 inode 块，用于存储 `inodes` 数组；
* 再接下来的 F 个块称为 FAT 表块，用于存储 `FAT_entry` 数组；
* 从第 I+F+1 个块开始的所有后续块均用作数据块。

由于 `struct inode` 和 `struct entry` 的结构体大小分别为 8 字节和 4 字节，因此一个 512 字节的磁盘块恰好可以容纳 64 个 `struct inode` 或 128 个 `struct entry`。

Therefore, for `FAT_entry`[100] = 202, we can read block I+1 into memory as an array of 128 entries, modify entry #100, and write it back to the disk. Similarly, for `FAT_entry`[202], we can read block I+2 into memory, modify entry #74 (i.e., 202%128==74), and then write it back to the disk. Modifying the inodes array with disk blocks 1..I is similar.

因此，若要更新 `FAT_entry[100] = 202`，我们可以将磁盘块 I+1 读取到内存中（作为一个包含 128 个表项的数组），修改其中的第 100 个表项，然后写回磁盘。同理，对于 `FAT_entry[202]`，我们可以将磁盘块 I+2 读取到内存中，修改其中第 74 个表项（即 $$202 \pmod{128}$ = 74$），再写回磁盘。通过磁盘块 1..I 修改 `inodes` 数组的操作也是同理。

Your first task is to implement the `mydisk_create` function as shown in the picture above. This function initializes the file system on the disk, a process also known as disk formatting. Specifically, you will do the following.

你的首要任务是实现 `mydisk_create` 函数（如上图磁盘布局所示）。该函数负责在磁盘上初始化文件系统，这一过程也即磁盘格式化（disk formatting）。具体而言，你需要完成以下步骤：

* Calculate I and F based on the number of inodes and the disk size.
* Set the size of all the inodes to 0, and set the head of all the inodes to -1.
* Create a linked list containing blocks I+F+1..N, which is called the initial free list.
* Define a data structure for the super block that records I, F, as well as the head of the initial free list, and write this data structure to the super block.
* You don't have to initialize the data blocks.

* 根据 inode 总数和磁盘容量计算所需占用块数 I 与 F；
* 将所有 inode 的 `size` 字段初始化为 0，`head` 字段初始化为 -1；
* 将磁盘块 I+F+1..N 串联成一个链表，作为初始的空闲块链表（free list）；
* 为超级块（superblock）定义数据结构，用于记录 I、F 以及空闲块链表的头节点索引，并将该结构写回超级块（第 0 块）；
* 无需对数据块区域进行专门初始化。

After the steps above, we have a file system with all inodes empty and all data blocks in the free list. You can also refer to the `treedisk_create` function in library/file/`file1.c`.

完成上述步骤后，我们就建立了一个初始文件系统：其中所有 inode 均为初始空状态，所有的物理数据块均包含在空闲块链表中。实现时，你也可以参考 `library/file/file1.c` 中的 `treedisk_create` 函数。

> [!TIP]
> Many file systems maintain a tree data structure instead of linked lists. The most widely used one is probably the B-tree, which has query complexity much lower than that of linked lists. Indeed, our solution code for this project runs slower than treedisk on QEMU, reflecting the complexity.

> [!TIP]
> > 许多现代文件系统会采用树形数据结构而非单链表。其中最广泛应用的是 B 树（或 B+ 树），其查询时间复杂度远优于链表。事实上，本项目中基于链表实现的参考解法在 QEMU 上运行速度慢于 `treedisk`（基于树结构的文件系统），这直观体现了数据结构复杂度对性能的影响。

### Using the linked lists

> **使用链表**

Your next task is to implement the `mydisk_read` and `mydisk_write` functions. At a high level, `mydisk_read` involves the following steps.

你的下一个任务是实现 `mydisk_read` 与 `mydisk_write` 函数。在总体逻辑上，`mydisk_read` 包含以下步骤：

* Read an inode block, and find the `struct inode` corresponding to the ino argument.
> * Check that the offset argument is smaller than the size field of the inode struct.

* Traverse from the start of the linked list (i.e., the head field) by offset times, and thus find the disk block number for the data block.
> * Read the data block into the block argument of `mydisk_read`.

* 读取对应的 inode 磁盘块，找到与 `ino` 参数相匹配的 `struct inode`；
> * 检查 `offset` 参数是否小于该 inode 的 `size` 字段；

* 从链表头节点（即 `head` 字段）出发，向前遍历 `offset` 步，从而定位到第 `offset` 个数据块对应的实际物理磁盘块号；
> * 将该物理磁盘块的数据读取到 `mydisk_read` 函数的 `block` 内存缓冲区中。

The steps for `mydisk_write` are similar, with one key difference. If the offset argument is larger than (or equal to) the size field in the `struct inode`, remove offset-size+1 blocks from the free list, and add these blocks to the inode. In other words, when writing to a large offset, you should expand the size of the inode by taking blocks from the free list. This is likely the trickiest part of this project, and make sure you have updated all 4 regions in the disk layout correctly.

`mydisk_write` 的实现步骤与读操作类似，但存在一个核心区别：若写入的 `offset` 大于或等于 `struct inode` 中的当前 `size` 字段，则需要从空闲链表中取出 `offset - size + 1` 个空闲块扩充给该 inode。换言之，当向超出当前文件大小的位置写入时，必须通过分配空闲块来动态扩展 inode 的容量。这往往是本项目中最具挑战性的部分，请务必确认你已正确更新了磁盘布局中涉及的所有 4 个区域的数据。

After implementing the two functions, you should be able to boot egos-2000 normally.

完成这两个函数的实现后，你就能够正常启动并运行 egos-2000 了。

### A complete inode store

> **完整的 inode 存储**

You can now implement `mydisk_getsize` and `mydisk_setsize`. getsize should be easy, since it is essentially the first step in `mydisk_read`. For setsize, start by implementing the case of nblocks being 0. For a non-zero nblocks, you can try to design helper functions shared by the setsize and write functions, which change the size of an inode.

接下来可以实现 `mydisk_getsize` 和 `mydisk_setsize`。`getsize` 相对简单，其逻辑本质上就是 `mydisk_read` 的第一步（读取 inode 获得 size）。对于 `setsize`，建议先处理 `nblocks` 为 0 的情况（释放该 inode 的所有块归还给空闲链表）。对于非零的 `nblocks`，可以考虑抽取 `setsize` 与 `write` 共用的辅助函数来动态调整 inode 所占用的磁盘块数量。

Since egos-2000 does not use these functions, we provide a separate testing framework.

由于 egos-2000 内核本身在启动时并未直接调用这些尺寸调节函数，我们专门提供了一个独立的测试框架供你验证。

## Testing your file system

> **测试你的文件系统**

We provide a testing framework for your file system code in this repo. Specifically, you can replace the `file0.c` in the repo with your code, and follow the instructions in the README. To write a test case, you write a trace file similar to the trace.txt in this repo.

我们在本仓库中为你的文件系统代码提供了一个测试框架。具体而言，你可以将编写好的代码替换仓库中的 `file0.c`，并按照 README 指引进行测试。若要自定义测试用例，可以编写类似于仓库中 `trace.txt` 的操作轨迹（trace）文件：

```txt
W:0:0:123456789
W:1:0:1234567
W:1:1:333333
R:0:0:123456789
R:1:0:1234567
R:1:1:333333
```

W:1:0:1234567 means to issue a write operation to your file system with inode #1, offset 0, and block data filled by the number 1234567. R:1:0:1234567 means to read from your file system with inode #1 and offset 0, and then check whether the block data returned by your file system still holds the number 1234567.

`W:1:0:1234567` 表示向文件系统发起写指令：对 inode #1 的 offset 0 写入填充了数字 1234567 的块数据。`R:1:0:1234567` 表示从文件系统发起读指令：读取 inode #1 的 offset 0，并校验返回的块数据是否准确包含数字 1234567。

You should write many more test traces that are more complex than trace.txt to comprehensively test your file system code. Remember to examine all the corner cases. Indeed, many students reported that AI is a great tool for generating comprehensive test cases, and you are encouraged to leverage AI.

你应该编写更多比 `trace.txt` 更复杂的测试 trace，对文件系统代码进行全面深入的测试。切记检查并涵盖所有的边界情况（corner cases）。不少同学反馈 AI 是生成全面测试用例的极佳助手，我们也强烈鼓励你借助 AI 来构建更完善的测试样例。

## Accomplishments

> **本章成果**

You have gained hands-on experience with the abstraction of inodes, files, and directories. You have also finished reading library/file/* and tools/`mkfs.c`. When explaining the `mydisk_init` function, we touched a little bit of apps/system/`sys_file.c`. You can finish reading this file, which serves all file system requests as a system process.

通过本实验，你已亲身实践了 inode、文件及目录的底层抽象机制，并完成了对 `library/file/*` 以及 `tools/mkfs.c` 源码的阅读。在解析 `mydisk_init` 函数时，我们曾简要提到了 `apps/system/sys_file.c`。建议你完整阅读该文件，了解它作为系统服务进程（system process）如何响应并处理所有的文件系统请求。
