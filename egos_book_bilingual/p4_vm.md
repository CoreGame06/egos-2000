# Virtual Memory

> **虚拟内存**

The purpose of virtual memory is to ensure that a user process can access only its own memory. In particular, it should not be allowed to access the kernel's memory or another process's memory. You learned PMP in P3, so we start with an implementation of virtual memory that combines PMP with a caching mechanism called software TLB. It takes only 20 lines of code and requires no additional CPU support.

虚拟内存的目的是确保用户进程只能访问属于其自身的内存。具体而言，用户进程绝不能访问内核内存或其他进程的内存。你在 P3 中学习了 PMP，因此我们首先介绍一种将 PMP 与称为“软件 TLB”的缓存机制结合的虚拟内存实现。它只需要 20 行代码，且不需要额外的 CPU 硬件支持。

This simple implementation helps you gain a precise understanding of the interface and functionality of virtual memory. We then introduce page table translation, a complex but more widely used CPU support for virtual memory. Your job is to implement virtual memory using page table translation and provide the same functionalities as PMP + software TLB.

这种简单的实现有助于你准确理解虚拟内存的接口与功能。接下来我们将引入页表转换（page table translation），这是一种更复杂但也应用更广泛的 CPU 虚拟内存硬件支持机制。你的任务是使用页表转换来实现虚拟内存，并提供与 PMP + 软件 TLB 相同的完整功能。

TIP

We will use processes instead of threads throughout this project. Typically, threads can access each other's memory just like what you have implemented in P1. In contrast, a process typically cannot access another process's memory, and this isolation is enforced by virtual memory. This project will show this difference through page table translation.

提示

在本项目中，我们将全程使用“进程”而非“线程”。通常情况下，线程之间可以互相访问彼此的内存，就像你在 P1 中实现的那样。相比之下，进程通常无法访问另一个进程的内存，而这种隔离正是由虚拟内存来强制保证的。本项目将通过页表转换展示这一区别。

## Software TLB

> **软件 TLB**

Consider two processes trying to use the same memory region. For example, all processes in egos-2000 use `0x80400000` as the top of their stack (i.e., `APPS_STACK_TOP` in library/`egos.h`). Virtual memory creates the illusion that a process exclusively uses the memory below `0x80400000` for its stack.

考虑两个进程试图使用同一个内存区域的情况。例如，egos-2000 中的所有进程都使用 `0x80400000` 作为其栈顶（即 `library/egos.h` 中的 `APPS_STACK_TOP`）。虚拟内存营造了一种假象：每个进程都在独占地使用 `0x80400000` 以下的内存作为自己的栈。

For example, suppose two processes both use the load instruction with `0x803FFFFC` as the address to read the highest 4 bytes of their stack. To create the illusion, `0x803FFFFC` will be translated into two different addresses when accessing memory. In other words, even though the two processes execute the same instruction, the CPU can read two different parts of memory and get different bytes. This makes sense because different processes are likely to place different 4-byte values at the start of their stack.

例如，假设两个进程都使用 load 指令以 `0x803FFFFC` 作为地址来读取其栈最高处的 4 个字节。为了营造这种假象，`0x803FFFFC` 在实际访问内存时会被转换成两个不同的物理地址。换句话说，即使两个进程执行的是完全相同的指令，CPU 也能读取内存中两个不同的位置并获取不同的字节数据。这是合理的，因为不同进程在其栈顶所存放的 4 字节数值通常各不相同。

The illusion here is that a process can only see that it reads address `0x803FFFFC`, while only the operating system knows the translated address used to access the memory. We thus call `0x803FFFFC` the virtual address in this case, and for every process, it is translated into a physical address for actual memory access.

这里的关键假象在于：进程只感知到自己读取的是地址 `0x803FFFFC`，而只有操作系统才知道用于实际访问物理内存的转换后地址。因此，在这种情况下，我们称 `0x803FFFFC` 为虚拟地址（virtual address），对于每个进程，它都会被转换为用于实际内存访问的物理地址（physical address）。

### Address translation

> **地址翻译**

Let's read a data structure recording the translation from virtual to physical addresses.

让我们看一个用于记录从虚拟地址到物理地址转换的数据结构：

```c
/* library/egos.h */
#define RAM_END         0x80600000
#define APPS_PAGES_BASE 0x80400000

/* earth/cpu_mmu.c */
#define PAGE_SIZE       4096
#define APPS_PAGES_CNT  (RAM_END - APPS_PAGES_BASE) / PAGE_SIZE
struct page_info {
    int use;
    int pid;
    uint vpage_no;
} page_info_table[APPS_PAGES_CNT];
```

In egos-2000, the 2MB region [`APPS_PAGES_BASE`, `RAM_END`) holds the code, data, heap, and stack of all processes. Therefore, if a process reads a virtual address such as `0x803FFFFC`, it will be translated into a physical address within this 2MB region. This 2MB is split into 512 pages (i.e., `APPS_PAGES_CNT`=512), and each page is 4KB (i.e., `PAGE_SIZE`).

在 egos-2000 中，2MB 大小的区域 [`APPS_PAGES_BASE`, `RAM_END`) 存放了所有进程的代码段、数据段、堆和栈。因此，如果一个进程读取像 `0x803FFFFC` 这样的虚拟地址，它将被转换为这 2MB 区域内的某个物理地址。这 2MB 被划分为 512 个物理页（即 `APPS_PAGES_CNT`=512），每个页大小为 4KB（即 `PAGE_SIZE`）。

There is a struct `page_info` for each of the 512 pages, recording the translation details. Suppose the OS allocates page #6 (i.e., `[0x80406000, 0x80407000)`) for the stack of the process identified by pid=7.

这 512 个页中的每一个都有一个对应的 `struct page_info`，用于记录地址转换细节。假设操作系统为 pid=7 的进程栈分配了第 6 号物理页（即 `[0x80406000, 0x80407000)`）。

The virtual address region `[0x803FF000, 0x80400000)` of this process directly translates to the physical address region `[0x80406000, 0x80407000)`. In other words, for any XXX in [0, `PAGE_SIZE`), virtual addresses 0x803FFXXX are translated to physical addresses 0x80406XXX.

该进程的虚拟地址区域 `[0x803FF000, 0x80400000)` 直接转换为物理地址区域 `[0x80406000, 0x80407000)`。换句话说，对于 [0, `PAGE_SIZE`) 范围内的任意 XXX，虚拟地址 0x803FFXXX 都会被转换为物理地址 0x80406XXX。

To record this translation in the data structure, `page_info_table`[6].pid should be set to 7, and `page_info_table`[6].`vpage_no` should be set to `0x803FF`. We call `0x803FF` the virtual page number, `0x80406` the physical page number, 6 the physical page ID, and XXX the offset within the page.

为了在数据结构中记录此映射关系，`page_info_table[6].pid` 应当被设为 7，`page_info_table[6].vpage_no` 应当被设为 `0x803FF`。我们称 `0x803FF` 为虚拟页号（virtual page number），`0x80406` 为物理页号（physical page number），6 为物理页 ID（physical page ID），而 XXX 为页内偏移量（offset within the page）。

TIP

To make sure you understand, here is a simple exercise. Suppose the operating system allocates page #195 out of the 512 pages for the stack top of a process with pid=12. Suppose the process reads virtual address `0x803FF234` within its stack. What are the virtual page number, physical page number, physical page ID, and offset, respectively, for this address translation?

提示

为了确保你已经完全理解，这里有一个简单的思考练习：假设操作系统在 512 个页中将第 195 号页分配给 pid=12 的进程作为其栈顶所在页。假设该进程读取其栈内的虚拟地址 `0x803FF234`。在此次地址转换中，虚拟页号、物理页号、物理页 ID 以及页内偏移量分别是什么？

You should now have a concrete understanding of how the same virtual address (e.g., one in the stack) can be mapped to different physical addresses across different processes.

现在，你应该对相同的虚拟地址（例如栈空间中的地址）如何在不同进程间被映射到不同物理地址有了直观而具体的理解。

### The `mmu_map` interface

> **`mmu_map` 接口**

The `mmu_map` interface in `struct earth` records such translations, and `soft_tlb_map` implements it. For example, the translation described above can be established by calling earth->`mmu_map`(7, `0x803FF`, 6).

`struct earth` 中的 `mmu_map` 接口用于记录此类转换关系，而 `soft_tlb_map` 实现了这一接口。例如，上述映射关系可以通过调用 `earth->mmu_map(7, 0x803FF, 6)` 来建立。

```c
/* library/egos.h */
struct earth {
    ...
    void (*mmu_map)(int pid, uint vpage_no, uint ppage_id);
    ...
};

/* earth/cpu_mmu.c */
void soft_tlb_map(int pid, uint vpage_no, uint ppage_id) {
    page_info_table[ppage_id].pid      = pid;
    page_info_table[ppage_id].vpage_no = vpage_no;
}
...
earth->mmu_map = soft_tlb_map;  /* when earth->translation==SOFT_TLB */
```

Let's look at how earth->`mmu_map` is used in library/elf/`elf.c`, where the operating system initializes memory for every new process. Intuitively, when creating a process, the operating system allocates some pages for the new process's code, data, heap, and stack, triggering the `mmu_alloc` function.

让我们看看 `earth->mmu_map` 是如何在 `library/elf/elf.c` 中被使用的，操作系统在这里为每个新进程初始化内存。直观地说，在创建进程时，操作系统为新进程的代码、数据、堆和栈分配物理页，从而触发 `mmu_alloc` 函数。

```c
/* earth/cpu_mmu.c */
uint mmu_alloc() {
    for (uint i = 0; i < APPS_PAGES_CNT; i++)
        if (!page_info_table[i].use) {
            page_info_table[i].use = 1;
            return i;
        }
    FATAL("mmu_alloc: no more free memory");
}

/* library/elf/elf.c */
void elf_load(int pid, elf_reader reader, int argc, void** argv) {
   ...
   /* The pattern below appears 5 times in the code with different VPAGE_NO. */
   ppage_id = earth->mmu_alloc();
   earth->mmu_map(pid, VPAGE_NO, ppage_id);
   ...
}
```

The `mmu_alloc` function returns a physical page ID. `VPAGE_NO` stands for the virtual page number. For code, data, and heap, `VPAGE_NO` starts at `0x80400`, and the number of pages allocated and mapped depends on the size of the application binary. Two pages are allocated and mapped for `main()` arguments and system call arguments with `VPAGE_NO` `0x80300` and `0x80301`. For the stack, we map two pages at `VPAGE_NO` `0x803FE` and `0x803FF`, and we assume that processes use at most 8KB of stack memory for simplicity. The picture below illustrates `elf_load` for a process with 4 pages of code, data, and heap.

`mmu_alloc` 函数返回一个物理页 ID。`VPAGE_NO` 代表虚拟页号。对于代码段、数据段和堆，`VPAGE_NO` 从 `0x80400` 开始，具体分配与映射的页数取决于应用程序二进制文件的大小。系统为 `main()` 参数与系统调用参数分别分配并映射一个页，其 `VPAGE_NO` 分别为 `0x80300` 和 `0x80301`。对于栈空间，我们在 `VPAGE_NO` `0x803FE` 和 `0x803FF` 处映射两个页（为简单起见，我们假设进程最多使用 8KB 栈内存）。下图展示了一个拥有 4 个代码/数据/堆页面的进程在调用 `elf_load` 时的内存分配情况。

Note that the "physical pages" area simply shows one possible way `mmu_alloc` selects the physical page ID from [0, 512) for each of the 8 page allocations.

请注意，“物理页（physical pages）”区域仅展示了 `mmu_alloc` 从 [0, 512) 范围中为 8 次页面分配选择物理页 ID 的一种可能方式。

TIP

A few other places reference these numbers: the ORIGIN = `0x80200000` in library/elf/`app.lds` indicates that the compiler puts the application binary code at virtual page `0x80200`. The `li` `sp`,`0x80400000` in apps/`app.s` indicates that all applications use the virtual page `0x803FF` as the stack top. The `APPS_ARG` and `SYSCALL_ARG` in library/`egos.h` are defined as `0x80300000` and `0x80301000`, respectively.

提示

还有其他几个地方引用了这些数字：`library/elf/app.lds` 中的 `ORIGIN = 0x80200000` 表示编译器将应用程序二进制代码存放在虚拟页 `0x80200` 处。`apps/app.s` 中的 `li sp,0x80400000` 表示所有应用程序都使用虚拟页 `0x803FF` 的顶端作为栈顶。`library/egos.h` 中的 `APPS_ARG` 和 `SYSCALL_ARG` 分别定义为 `0x80300000` 和 `0x80301000`。

### The `mmu_switch` interface

> **`mmu_switch` 接口**

As you may have noticed, `proc_yield` invokes earth->`mmu_switch` for the next scheduled process, and `soft_tlb_switch` is a simple implementation of earth->`mmu_switch`. When switching from process A to process B, `soft_tlb_switch` makes two steps.

正如你可能注意到的，`proc_yield` 会为下一个被调度的进程调用 `earth->mmu_switch`，而 `soft_tlb_switch` 是 `earth->mmu_switch` 的一个简单实现。当从进程 A 切换到进程 B 时，`soft_tlb_switch` 执行以下两个步骤：

First, it copies all the pages mapped for process A back into the 512-page buffer.

第一步，它将为进程 A 映射的所有页面复制回 512 页缓冲区中。

Second, it copies all the pages mapped for process B to `[0x80200000, 0x80400000)`.

第二步，它将为进程 B 映射的所有页面复制到 `[0x80200000, 0x80400000)`。

In the two pictures above, all the areas indicate physical memory. In other words, we use the 4MB physical memory region `[0x80200000, 0x80600000)` to create the illusion that every process exclusively owns the region `[0x80200000, 0x80400000)`.

在上图所示的两个步骤中，所有区域都代表物理内存。换句话说，我们利用 `[0x80200000, 0x80600000)` 这 4MB 的物理内存区域，营造出每个进程都独占 `[0x80200000, 0x80400000)` 区域的假象。

Whenever `proc_yield` picks a process to run next, `soft_tlb_switch` makes sure that the process can access its own bytes when invoking a load or store instruction with an address in `[0x80200000, 0x80400000)`. This combination of `soft_tlb_map` and `soft_tlb_switch` for memory translation is called the software TLB mechanism. Furthermore, recall that you set a PMP region for `[0x80200000, 0x80400000)` in P3. After adding this PMP region, when a process is running in user mode, it cannot read or write other processes' memory pages, which, as shown above, are kept in the `[0x80400000, 0x80600000)` region. This achieves the purpose of virtual memory stated at the beginning.

每当 `proc_yield` 挑选出下一个要运行的进程时，`soft_tlb_switch` 都会确保该进程在使用 `[0x80200000, 0x80400000)` 范围内的地址执行加载（load）或存储（store）指令时能够访问到属于它自己的数据。这种结合 `soft_tlb_map` 与 `soft_tlb_switch` 进行内存转换的机制被称为“软件 TLB”机制。此外，回想一下在 P3 中你曾为 `[0x80200000, 0x80400000)` 配置过 PMP 区域。在加上该 PMP 区域后，当进程运行在用户模式时，便无法读写其他进程的内存页面（如上所述，这些页面安全存放在 `[0x80400000, 0x80600000)` 区域中）。这就达成了开头所述的虚拟内存隔离目标。

TIP

`soft_tlb_switch` has two loops that query `page_info_table` and conduct the memory copy described above. After calling `soft_tlb_switch`, the `soft_tlb_translate` function in earth/`cpu_mmu.c` simply returns vaddr itself as the translated physical address, assuming that vaddr is an address within `[0x80200000, 0x80400000)`.

提示

`soft_tlb_switch` 包含两个循环，用于查询 `page_info_table` 并执行上述内存复制。调用 `soft_tlb_switch` 之后，`earth/cpu_mmu.c` 中的 `soft_tlb_translate` 函数只需将 `vaddr` 本身作为转换后的物理地址返回（因为它假设 `vaddr` 是 `[0x80200000, 0x80400000)` 范围内的地址）。

## Page table translation

> **页表翻译**

Combining the software TLB with PMP provides a simple implementation of virtual memory, but memory copying is the performance bottleneck for many programs, including operating systems. To avoid this bottleneck, we introduce page table translation, which provides CPU support for a more efficient implementation of the virtual memory interface (i.e., `mmu_map`, `mmu_switch`, and `mmu_translate`).

将软件 TLB 与 PMP 结合提供了一种简单有效的虚拟内存实现，但大量的内存复制会成为许多程序（包括操作系统）的性能瓶颈。为了消除这一瓶颈，我们引入了基于硬件的页表转换（Page Table Translation），它利用 CPU 原生硬件支持更高效地实现虚拟内存接口（即 `mmu_map`、`mmu_switch` 和 `mmu_translate`）。

Read Chapters 12.1.11 and 12.3 of the RISC-V Instruction Set Manual. They describe a RISC-V page table translation scheme called Sv32. Next, we sketch only the core concepts of Sv32 and give an example of Sv32 translation. You can read more details in the manual.

请阅读《RISC-V 指令集手册》的第 12.1.11 和 12.3 节。它们描述了一种名为 Sv32 的 RISC-V 页表转换方案。接下来，我们仅概述 Sv32 的核心概念，并给出一个 Sv32 地址转换的示例。你可以在手册中阅读更多细节。

### Root and leaf page tables

> **根页表与叶页表**

In Sv32, a page table contains 1024 4-byte entries, so it occupies exactly 4KB of memory (i.e., one page). Page tables further form a two-level tree data structure with one root page table and several leaf page tables. The picture below shows an example of such a tree data structure in memory with 3 page tables.

在 Sv32 中，一个页表包含 1024 个 4 字节页表项（Page Table Entry, PTE），因此它恰好占用 4KB 的内存（即一个页）。页表进一步构成一个两级树状数据结构，包含一个根页表（root page table）和若干个叶页表（leaf page table）。下图展示了内存中由 3 个页表组成的这种树状数据结构的示例。

When tracking this tree data structure, the CPU first reads a CSR called `satp`, which stores the address of the root page table. The picture shows a case where 1022 entries in the root page table are empty, and 2 entries are used to hold the addresses of 2 leaf page tables. When we say "the address" of a page table, we mean the physical address that the CPU uses to access memory, rather than a virtual address belonging to a process.

在进行页表遍历（Page Walk）时，CPU 首先读取名为 `satp` 的 CSR（控制与状态寄存器），其中保存着根页表的物理地址。上图展示的情形是：根页表中有 1022 个页表项为空，剩余 2 个页表项分别指向 2 个叶页表。需要说明的是，当我们提到页表的“地址”时，指的是 CPU 硬件用于直接访问内存的物理地址，而不是进程使用的虚拟地址。

### Translation details

> **翻译细节**

At a high level, the operating system is responsible for allocating pages for the page tables, initializing the page table entries, and writing to the CSR `satp`. When a process is reading or writing a memory address, the CPU reads the CSR `satp` and page tables to translate the address. Below is an example of how the OS initializes the page tables so that the CPU translates the virtual addresses `0x803F_EXXX` into the physical addresses `0x804A_DXXX` for a process. Again, XXX stands for any offset in the range [0, `PAGE_SIZE`).

从整体架构来看，操作系统负责为页表分配内存页、初始化页表项，并设置 CSR `satp`；而当进程读写某个内存地址时，CPU 硬件会自动读取 CSR `satp` 并遍历页表以完成地址转换。下面给出一个具体示例，展示操作系统如何初始化页表，从而使 CPU 将进程的虚拟地址 `0x803F_EXXX` 转换为物理地址 `0x804A_DXXX`（其中 XXX 代表 [0, `PAGE_SIZE`) 范围内的任意页内偏移量）。

# Initialization of the `satp` CSR

> **satp CSR 的初始化**

The OS invokes `mmu_alloc` and allocates a page for the root page table. As shown above, this page is `0x805F_1000` (i.e., `mmu_alloc` returns physical page ID `0x805F1` - `0x80400` = `0x1F1`). With physical page number `0x805F1`, we write `0x800805F1` to the `satp` CSR, and the CPU will know where the root page table is located, according to Figure 63 of the RISC-V Reference Manual.

操作系统调用 `mmu_alloc` 并为根页表分配一个页。如上所示，该页为 `0x805F_1000`（即 `mmu_alloc` 返回物理页 ID `0x805F1` - `0x80400` = `0x1F1`）。有了物理页号 `0x805F1`，我们将 `0x800805F1` 写入 `satp` CSR，根据《RISC-V 参考手册》图 63，CPU 就能知道根页表所在的位置。

Note that the MODE bit is set to 1, meaning that page table translation is enabled. The ASID field is for performance optimization, which we can ignore for now. Lastly, we put `0x805F1` into the PPN field of `satp`.

请注意，MODE 位被设为 1，这意味着启用了页表转换。ASID 字段用于性能优化，我们目前可以忽略。最后，我们将 `0x805F1` 写入 `satp` 的 PPN 字段。

TIP

You may have noticed that `0x805F1` has 20 bits, but the PPN field of `satp` holds 22 bits. This is because a 32-bit RISC-V CPU supports up to 16GB of physical memory, which is 2^22 4KB pages. In egos-2000, all addresses are below 4GB, so 20 bits are enough.

提示

你可能已经注意到 `0x805F1` 占据 20 位，但 `satp` 的 PPN 字段有 22 位。这是因为 32 位 RISC-V CPU 最多支持 16GB 的物理内存（即 2^22 个 4KB 页）。在 egos-2000 中，所有地址都低于 4GB，因此 20 位已经足够。

# Initialization of the root page table

> **根页表的初始化**

We need to find an entry in the root page table and initialize it using the address of the leaf page table. To find the entry, read Figure 65 of the RISC-V Reference Manual.

我们需要在根页表中找到一个页表项，并使用叶页表的地址对其进行初始化。要查找该页表项，请参阅《RISC-V 参考手册》图 65。

Specifically, we wish to translate the virtual address `0x803F_EXXX`, and according to Figure 65, the VPN[1] part of `0x803F_EXXX` is 0b1000000000 or `0x200` (i.e., the most significant 10 bits). Therefore, in the picture above, we see that entry #512 (i.e., `0x200`) in the root page table is used to translate the virtual address `0x803F_EXXX`. To see how this entry shall be initialized, read Figures 66 and 67 of the RISC-V Reference Manual.

具体而言，我们希望转换虚拟地址 `0x803F_EXXX`，根据图 65，`0x803F_EXXX` 的 VPN[1] 部分为 0b1000000000 即 `0x200`（即最高 10 位）。因此在上图中，我们可以看到根页表中的第 512 号页表项（即 `0x200`）用于转换虚拟地址 `0x803F_EXXX`。要了解该页表项应如何初始化，请阅读《RISC-V 参考手册》图 66 和图 67。

Note that the leaf page table is at physical address `0x8051_5000`, so the PPN is `0x80515`. According to Figure 67, we should use `0x80515` as bits #10 to #31 of the page table entry, so we can set the page table entry as `0x2014541F` (i.e., (`0x80515`<<10) | `0x1F`), where the `0x1F` here means that we are setting the V, R, W, X, and U bits to 1. Setting these bits to 1 means that we allow code running in the user mode to read, write, or execute this page of memory after translation. We can now use page tables to control memory access instead of PMP. To summarize, we have done the following things so far.

请注意，叶页表位于物理地址 `0x8051_5000`，因此其 PPN 为 `0x80515`。根据图 67，我们应该将 `0x80515` 填入页表项的第 10 至 31 位，因此可将该页表项设置为 `0x2014541F`（即 `(0x80515<<10) | 0x1F`），其中的 `0x1F` 表示我们将 V（有效位）、R（可读）、W（可写）、X（可执行）和 U（用户态可访问）标志位全置为 1。将这些标志位置 1 意味着允许运行在用户模式下的代码在地址转换后对该内存页进行读、写或执行。至此，我们可以使用页表来实施内存访问控制，而不再依赖 PMP。总结一下，我们目前完成的操作如下：

```c
asm("csrw satp, %0" ::"r"(0x800805F1));
uint *root = (uint*)0x805F1000; /* decided by earth->mmu_alloc */
root[512]  = 0x2014541F;
```

The CPU can now find the leaf page table for translating the virtual address `0x803F_EXXX`.

CPU 现在可以找到用于转换虚拟地址 `0x803F_EXXX` 的叶页表。

# Initialization of the leaf page table

> **叶页表的初始化**

Figure 65 also shows that the VPN[0] part of `0x803F_EXXX` is 0b1111111110, or `0x3FE`. We will thus initialize entry #1022 (i.e., `0x3FE`) of the leaf page table based on the physical address `0x804A_D000`. According to Figure 67, we should use the PPN of `0x804AD` as bits #10 to #31, and write `0x2012B41F` (i.e., (`0x804AD`<<10) | `0x1F`) into entry #1022.

图 65 还显示 `0x803F_EXXX` 的 VPN[0] 部分为 0b1111111110 即 `0x3FE`。因此我们将基于物理地址 `0x804A_D000` 来初始化叶页表的第 1022 号页表项（即 `0x3FE`）。根据图 67，我们应该使用 PPN `0x804AD` 作为第 10 到第 31 位，并将 `0x2012B41F`（即 `(0x804AD<<10) | 0x1F`）写入第 1022 号页表项。

```c
uint *leaf = (uint*)0x80515000; /* decided by earth->mmu_alloc */
leaf[1022] = 0x2012B41F;
```

The initialization is done, allowing the CPU to translate the virtual address `0x803F_EXXX` into the physical address `0x804A_DXXX`, starting with the `satp` CSR.

初始化工作完成，使得 CPU 能够从 `satp` CSR 开始，将虚拟地址 `0x803F_EXXX` 转换为物理地址 `0x804A_DXXX`。

## Get started

> **开始动手**

You can now use page table translation to implement virtual memory in earth/`cpu_mmu.c`. Start from a fresh copy of egos-2000.

你现在可以在 `earth/cpu_mmu.c` 中使用页表转换来实现虚拟内存了。请从一份干净的 egos-2000 副本开始。

### Identity map

> **恒等映射**

Read the `setup_identity_region` and `pagetable_identity_map` functions, which set up an identity map, meaning that a virtual address is translated into a physical address of the same value (e.g., map `0x803F_EXXX` to `0x803F_EXXX`). They help you understand how to set up page table entries in C.

请阅读 `setup_identity_region` 与 `pagetable_identity_map` 函数，它们配置了一个恒等映射（Identity Mapping），即虚拟地址被直接转换为数值相同的物理地址（例如将 `0x803F_EXXX` 映射为 `0x803F_EXXX`）。这有助于你理解如何在 C 代码中构建和填充页表项。

Then read the `page_table_map` and `page_table_switch` functions. You can see that they simply call `soft_tlb_map` and `soft_tlb_switch`. Hence, when you choose "page tables" as shown below, egos-2000 will enable page table translation with an identity map and still use the software TLB to manage address translations.

接着阅读 `page_table_map` 和 `page_table_switch` 函数。你可以看到它们只是简单地调用了 `soft_tlb_map` 和 `soft_tlb_switch`。因此，当你按如下所示选择“page tables”时，egos-2000 将启用带恒等映射的页表转换，但仍使用软件 TLB 来管理地址映射。

```
> cd egos-2000
> make qemu
...
[CRITICAL] Choose a memory translation mechanism:
Enter 0: page tables
Enter 1: software TLB
[INFO] Page table translation is chosen
...
```

With an identity map, the CPU will perform page table translations in a pretty useless way. Still, if we make mistakes in the identity map code, there could be memory exceptions after page table translation starts to take effect. In particular, page table translation starts to take effect in egos-2000 after this `mret` in grass/`init.c`.

在恒等映射下，CPU 进行页表转换看起来并无太大实际作用（物理地址与虚拟地址数值完全相同）。尽管如此，如果我们恒等映射的代码写错了，在页表转换开始生效后就可能引发内存异常。具体来说，在 egos-2000 中，页表转换会在 `grass/init.c` 中的这条 `mret` 执行后开始生效。

```c
uint mstatus, M_MODE = 3, U_MODE = 0;
uint GRASS_MODE = (earth->translation == SOFT_TLB) ? M_MODE : U_MODE;
asm("csrr %0, mstatus" : "=r"(mstatus));
mstatus = (mstatus & ~(3 << 11)) | (GRASS_MODE << 11);
asm("csrw mstatus, %0" ::"r"(mstatus));
...
asm("mret");
```

Code running in machine mode always accesses memory using physical addresses. If earth->translation == `PAGE_TABLE`, the CPU will switch to user mode (i.e., `U_MODE`) after the `mret` and run the first process's code. Since we have enabled Sv32 in `satp`, and page table translation certainly takes effect in user mode, it starts taking effect right after this `mret`. All processes run in user mode, with page table translation taking effect, while the kernel runs in machine mode, accessing physical addresses directly.

在 M 模式（机器模式）下运行的代码总是直接使用物理地址访问内存。如果 `earth->translation == PAGE_TABLE`，CPU 将在执行 `mret` 之后切换到 U 模式（用户模式）并运行首个进程的代码。由于我们已在 `satp` 中启用了 Sv32，而页表转换在用户模式下会强制生效，因此地址转换在 `mret` 指令完成后便立即全面启动。此后，所有用户进程均运行在启用页表转换的 U 模式下，而内核则运行在直接操作物理地址的 M 模式下。

TIP

There is actually a third privilege mode called supervisor mode (`S_MODE` = 1), and the 's' in `satp` stands for supervisor mode, just like the 'm' in `mstatus` stands for machine mode. Code in machine mode can access the CSRs for supervisor mode, but not vice versa. We are not using this supervisor mode for simplicity—you only need to reason about two privilege modes in P4.

提示

实际上 RISC-V 还有第三种特权模式，称为 S 模式（监管者模式/Supervisor Mode，`S_MODE` = 1），`satp` 中的 's' 即代表 Supervisor，正如 `mstatus` 中的 'm' 代表 Machine 一样。在 M 模式下运行的代码可以访问 S 模式的 CSR，反之则不行。为保持系统简洁，我们并未开启 S 模式——在 P4 中你只需要分析和处理两种特权模式。

### Page tables per process

> **为每个进程建立页表**

After understanding the existing code for page table translation in egos-2000, your job is to create one set of page tables for every process. Read the hints in `page_table_map`. When invoking `page_table_map` for process pid for the first time, you should initialize the root and leaf page tables for pid. For system processes, this means you should set up an identity map as before. For user processes, set up an identity map only for one page at `SHELL_WORK_DIR`—the address of workdir in apps/`app.h`. Processes in egos-2000 share the same work directory, which is the directory shown in the shell. Save the address of the root page table in `pid_to_pagetable_base`[pid], and modify the page table entries based on the arguments of `page_table_map`.

在理解了 egos-2000 中现有的页表转换代码后，你的任务是为每个进程分别创建一套独立的页表。请仔细阅读 `page_table_map` 中的注释提示。当首次为进程 `pid` 调用 `page_table_map` 时，你应当为 `pid` 分配并初始化根页表与叶页表。对于系统进程（system processes），这意味着你应像此前一样建立恒等映射；对于用户进程（user processes），只需为 `SHELL_WORK_DIR` 所在的单个页面建立恒等映射（该地址对应 `apps/app.h` 中 `workdir` 的虚拟地址，因为 egos-2000 中的所有进程共享 Shell 当前显示的工作目录）。请将根页表的物理地址保存在 `pid_to_pagetable_base[pid]` 中，并根据 `page_table_map` 的传入参数更新对应的页表项。

In addition, you need to update the `page_table_switch` function by modifying the `satp` CSR to point to the root page table for the process pid. Recall that this function is invoked by `proc_yield` for context switch. Lastly, implement the `page_table_translate` function, which returns the physical address mapped from vaddr for process pid.

此外，你需要更新 `page_table_switch` 函数，通过修改 `satp` CSR 使其指向进程 `pid` 的根页表。回想一下，该函数在进行上下文切换时由 `proc_yield` 调用。最后，实现 `page_table_translate` 函数，该函数返回进程 `pid` 中由 `vaddr` 映射得到的物理地址。

It is possible to reuse both `setup_identity_region` and `pagetable_identity_map` as is, but feel free to modify them if necessary. After you finish, you should be able to boot up egos-2000 normally and run commands such as cat README or echo Hello, World!.

可以直接原封不动地复用 `setup_identity_region` 和 `pagetable_identity_map`，但如果有必要也可以随意修改它们。完成后，你应该能够正常启动 egos-2000 并运行诸如 `cat README` 或 `echo Hello, World!` 等命令。

TIP

In real-world operating systems like Linux, the difference between a process and a thread is more conceptual than at the code level. Specifically, if processes share the same root page table, we can consider them threads within a single process because they share memory.

提示

在像 Linux 这样的真实世界操作系统中，进程和线程之间的区别在概念层面大于在代码层面。具体来说，如果多个进程共享同一个根页表，我们可以把它们看作是一个进程内部的多个线程，因为它们共享内存。

### Kill malicious applications

> **终止恶意应用程序**

Recall that your code for memory protection in P3 can prevent malicious applications such as crash1 or crash2 from halting the operating system. You can now do the same with page table translation: terminate the current process in `excp_entry()` of grass/`kernel.c` if an exception is raised, and ensure that the shell can proceed after crash1 and crash2.

回想一下，你在 P3 中编写的内存保护机制能防止像 `crash1` 或 `crash2` 这样的恶意应用程序导致操作系统死机挂起。现在利用页表转换你同样可以实现该防护：当发生内存异常时，在 `grass/kernel.c` 的 `excp_entry()` 中及时终止当前违规进程，确保 Shell 在运行 `crash1` 和 `crash2` 崩溃后仍能平滑恢复并继续正常接收用户指令。

```
> make qemu
...
[CRITICAL] Choose a memory translation mechanism:
Enter 0: page tables
Enter 1: software TLB
[INFO] Page table translation is chosen
...
[CRITICAL] Welcome to the egos-2000 shell!
➜ /home/yunhao crash1
_sbrk: heap grows too large
[INFO] process 6 terminated with exception 15
➜ /home/yunhao crash2
[INFO] process 7 terminated with exception 15
...
```

### Memory overhead

> **内存开销**

We have never talked about how to free page tables when a process terminates. The key is that the `proc_free` function in grass/`process.c` invokes earth->`mmu_free`(pid).

我们之前从未讨论过当进程终止时如何释放页表。关键在于 `grass/process.c` 中的 `proc_free` 函数会调用 `earth->mmu_free(pid)`。

For all page tables allocated for process pid, if we set the pid field of the corresponding `page_info_table` entries correctly, the page tables should be freed by `mmu_free` just like the other pages of the terminated process. Nothing special needs to be done.

对于为进程 `pid` 分配的所有页表物理页，只要我们在创建时正确设置了 `page_info_table` 对应条目的 `pid` 字段，这些页表页面就会在进程终止时像该进程的其他普通数据页一样被 `mmu_free` 统一回收释放，无需专门编写额外的释放代码。

However, given that page table translation incurs memory overhead, we ask you to print out such overhead in `mmu_free()` as follows.

然而，鉴于页表转换会带来额外的内存开销，我们要求你在 `mmu_free()` 中按如下格式打印出该开销：

```
> make qemu
...
➜ /home/yunhao echo Hello, World!
Hello, World!
[INFO] mmu_free() released 11 pages (2 are page tables) for process 6
```

Make sure that you can explain the numbers printed out by `mmu_free`.

确保你能够解释 `mmu_free` 打印出来的数字。

## Accomplishments

> **成就**

You have finished reading library/elf and earth/`cpu_mmu.c` in egos-2000, and learned how to create the virtual memory illusion for different processes. You have also understood precisely the difference between threads and processes, which is closely related to the root page table of the page table translation mechanism.

恭喜你！你已深入阅读并理解了 egos-2000 中的 `library/elf` 与 `earth/cpu_mmu.c`，掌握了如何为不同进程构建隔离的虚拟内存空间假象。同时，你也深刻理解了线程与进程在底层实现上的本质区别——这一区别在基于页表的地址转换机制中正体现为是否共享根页表。
