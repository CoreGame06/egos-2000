Virtual Memory | The EGOS book              Skip to content 

AppearanceMenuReturn to top























# Virtual Memory ​

The purpose of virtual memory is to ensure that a user process can access only its own memory. In particular, it should not be allowed to access the kernel's memory or another process's memory. You learned PMP in P3, so we start with an implementation of virtual memory that combines PMP with a caching mechanism called software TLB. It takes only 20 lines of code and requires no additional CPU support.

This simple implementation helps you gain a precise understanding of the interface and functionality of virtual memory. We then introduce page table translation, a complex but more widely used CPU support for virtual memory. Your job is to implement virtual memory using page table translation and provide the same functionalities as PMP + software TLB.

TIP

We will use processes instead of threads throughout this project. Typically, threads can access each other's memory just like what you have implemented in P1. In contrast, a process typically cannot access another process's memory, and this isolation is enforced by virtual memory. This project will show this difference through page table translation.

## Software TLB ​

Consider two processes trying to use the same memory region. For example, all processes in egos-2000 use 0x80400000 as the top of their stack (i.e., APPS_STACK_TOP in library/egos.h). Virtual memory creates the illusion that a process exclusively uses the memory below 0x80400000 for its stack.

For example, suppose two processes both use the load instruction with 0x803FFFFC as the address to read the highest 4 bytes of their stack. To create the illusion, 0x803FFFFC will be translated into two different addresses when accessing memory. In other words, even though the two processes execute the same instruction, the CPU can read two different parts of memory and get different bytes. This makes sense because different processes are likely to place different 4-byte values at the start of their stack.

The illusion here is that a process can only see that it reads address 0x803FFFFC, while only the operating system knows the translated address used to access the memory. We thus call 0x803FFFFC the virtual address in this case, and for every process, it is translated into a physical address for actual memory access.

### Address translation ​

Let's read a data structure recording the translation from virtual to physical addresses.c
```
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
123456789101112

In egos-2000, the 2MB region [APPS_PAGES_BASE, RAM_END) holds the code, data, heap, and stack of all processes. Therefore, if a process reads a virtual address such as 0x803FFFFC, it will be translated into a physical address within this 2MB region. This 2MB is split into 512 pages (i.e., APPS_PAGES_CNT=512), and each page is 4KB (i.e., PAGE_SIZE).

There is a struct page_info for each of the 512 pages, recording the translation details. Suppose the OS allocates page #6 (i.e., [0x80406000, 0x80407000)) for the stack of the process identified by pid=7.
* 

The virtual address region [0x803FF000, 0x80400000) of this process directly translates to the physical address region [0x80406000, 0x80407000). In other words, for any XXX in [0, PAGE_SIZE), virtual addresses 0x803FFXXX are translated to physical addresses 0x80406XXX.
* 

To record this translation in the data structure, page_info_table[6].pid should be set to 7, and page_info_table[6].vpage_no should be set to 0x803FF. We call 0x803FF the virtual page number, 0x80406 the physical page number, 6 the physical page ID, and XXX the offset within the page.

TIP

To make sure you understand, here is a simple exercise. Suppose the operating system allocates page #195 out of the 512 pages for the stack top of a process with pid=12. Suppose the process reads virtual address 0x803FF234 within its stack. What are the virtual page number, physical page number, physical page ID, and offset, respectively, for this address translation?

You should now have a concrete understanding of how the same virtual address (e.g., one in the stack) can be mapped to different physical addresses across different processes.

### The mmu_map interface ​

The mmu_map interface in struct earth records such translations, and soft_tlb_map implements it. For example, the translation described above can be established by calling earth->mmu_map(7, 0x803FF, 6).c
```
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
1234567891011121314

Let's look at how earth->mmu_map is used in library/elf/elf.c, where the operating system initializes memory for every new process. Intuitively, when creating a process, the operating system allocates some pages for the new process's code, data, heap, and stack, triggering the mmu_alloc function.c
```
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
123456789101112131415161718

The mmu_alloc function returns a physical page ID. VPAGE_NO stands for the virtual page number. For code, data, and heap, VPAGE_NO starts at 0x80400, and the number of pages allocated and mapped depends on the size of the application binary. Two pages are allocated and mapped for main() arguments and system call arguments with VPAGE_NO 0x80300 and 0x80301. For the stack, we map two pages at VPAGE_NO 0x803FE and 0x803FF, and we assume that processes use at most 8KB of stack memory for simplicity. The picture below illustrates elf_load for a process with 4 pages of code, data, and heap.



Note that the "physical pages" area simply shows one possible way mmu_alloc selects the physical page ID from [0, 512) for each of the 8 page allocations.

TIP

A few other places reference these numbers: the ORIGIN = 0x80200000 in library/elf/app.lds indicates that the compiler puts the application binary code at virtual page 0x80200. The li sp,0x80400000 in apps/app.s indicates that all applications use the virtual page 0x803FF as the stack top. The APPS_ARG and SYSCALL_ARG in library/egos.h are defined as 0x80300000 and 0x80301000, respectively.

### The mmu_switch interface ​

As you may have noticed, proc_yield invokes earth->mmu_switch for the next scheduled process, and soft_tlb_switch is a simple implementation of earth->mmu_switch. When switching from process A to process B, soft_tlb_switch makes two steps.

First, it copies all the pages mapped for process A back into the 512-page buffer.



Second, it copies all the pages mapped for process B to [0x80200000, 0x80400000).



In the two pictures above, all the areas indicate physical memory. In other words, we use the 4MB physical memory region [0x80200000, 0x80600000) to create the illusion that every process exclusively owns the region [0x80200000, 0x80400000).

Whenever proc_yield picks a process to run next, soft_tlb_switch makes sure that the process can access its own bytes when invoking a load or store instruction with an address in [0x80200000, 0x80400000). This combination of soft_tlb_map and soft_tlb_switch for memory translation is called the software TLB mechanism. Furthermore, recall that you set a PMP region for [0x80200000, 0x80400000) in P3. After adding this PMP region, when a process is running in user mode, it cannot read or write other processes' memory pages, which, as shown above, are kept in the [0x80400000, 0x80600000) region. This achieves the purpose of virtual memory stated at the beginning.

TIP

soft_tlb_switch has two loops that query page_info_table and conduct the memory copy described above. After calling soft_tlb_switch, the soft_tlb_translate function in earth/cpu_mmu.c simply returns vaddr itself as the translated physical address, assuming that vaddr is an address within [0x80200000, 0x80400000).

## Page table translation ​

Combining the software TLB with PMP provides a simple implementation of virtual memory, but memory copying is the performance bottleneck for many programs, including operating systems. To avoid this bottleneck, we introduce page table translation, which provides CPU support for a more efficient implementation of the virtual memory interface (i.e., mmu_map, mmu_switch, and mmu_translate).

Read Chapters 12.1.11 and 12.3 of the RISC-V Instruction Set Manual. They describe a RISC-V page table translation scheme called Sv32. Next, we sketch only the core concepts of Sv32 and give an example of Sv32 translation. You can read more details in the manual.

### Root and leaf page tables ​

In Sv32, a page table contains 1024 4-byte entries, so it occupies exactly 4KB of memory (i.e., one page). Page tables further form a two-level tree data structure with one root page table and several leaf page tables. The picture below shows an example of such a tree data structure in memory with 3 page tables.



When tracking this tree data structure, the CPU first reads a CSR called satp, which stores the address of the root page table. The picture shows a case where 1022 entries in the root page table are empty, and 2 entries are used to hold the addresses of 2 leaf page tables. When we say "the address" of a page table, we mean the physical address that the CPU uses to access memory, rather than a virtual address belonging to a process.

### Translation details ​

At a high level, the operating system is responsible for allocating pages for the page tables, initializing the page table entries, and writing to the CSR satp. When a process is reading or writing a memory address, the CPU reads the CSR satp and page tables to translate the address. Below is an example of how the OS initializes the page tables so that the CPU translates the virtual addresses 0x803F_EXXX into the physical addresses 0x804A_DXXX for a process. Again, XXX stands for any offset in the range [0, PAGE_SIZE).



# Initialization of the satp CSR ​

The OS invokes mmu_alloc and allocates a page for the root page table. As shown above, this page is 0x805F_1000 (i.e., mmu_alloc returns physical page ID 0x805F1 - 0x80400 = 0x1F1). With physical page number 0x805F1, we write 0x800805F1 to the satp CSR, and the CPU will know where the root page table is located, according to Figure 63 of the RISC-V Reference Manual.



Note that the MODE bit is set to 1, meaning that page table translation is enabled. The ASID field is for performance optimization, which we can ignore for now. Lastly, we put 0x805F1 into the PPN field of satp.

TIP

You may have noticed that 0x805F1 has 20 bits, but the PPN field of satp holds 22 bits. This is because a 32-bit RISC-V CPU supports up to 16GB of physical memory, which is 2^22 4KB pages. In egos-2000, all addresses are below 4GB, so 20 bits are enough.

# Initialization of the root page table ​

We need to find an entry in the root page table and initialize it using the address of the leaf page table. To find the entry, read Figure 65 of the RISC-V Reference Manual.



Specifically, we wish to translate the virtual address 0x803F_EXXX, and according to Figure 65, the VPN[1] part of 0x803F_EXXX is 0b1000000000 or 0x200 (i.e., the most significant 10 bits). Therefore, in the picture above, we see that entry #512 (i.e., 0x200) in the root page table is used to translate the virtual address 0x803F_EXXX. To see how this entry shall be initialized, read Figures 66 and 67 of the RISC-V Reference Manual.



Note that the leaf page table is at physical address 0x8051_5000, so the PPN is 0x80515. According to Figure 67, we should use 0x80515 as bits #10 to #31 of the page table entry, so we can set the page table entry as 0x2014541F (i.e., (0x80515<<10) | 0x1F), where the 0x1F here means that we are setting the V, R, W, X, and U bits to 1. Setting these bits to 1 means that we allow code running in the user mode to read, write, or execute this page of memory after translation. We can now use page tables to control memory access instead of PMP. To summarize, we have done the following things so far.c
```
asm("csrw satp, %0" ::"r"(0x800805F1));
uint *root = (uint*)0x805F1000; /* decided by earth->mmu_alloc */
root[512]  = 0x2014541F;
```
123

The CPU can now find the leaf page table for translating the virtual address 0x803F_EXXX.

# Initialization of the leaf page table ​

Figure 65 also shows that the VPN[0] part of 0x803F_EXXX is 0b1111111110, or 0x3FE. We will thus initialize entry #1022 (i.e., 0x3FE) of the leaf page table based on the physical address 0x804A_D000. According to Figure 67, we should use the PPN of 0x804AD as bits #10 to #31, and write 0x2012B41F (i.e., (0x804AD<<10) | 0x1F) into entry #1022.c
```
uint *leaf = (uint*)0x80515000; /* decided by earth->mmu_alloc */
leaf[1022] = 0x2012B41F;
```
12

The initialization is done, allowing the CPU to translate the virtual address 0x803F_EXXX into the physical address 0x804A_DXXX, starting with the satp CSR.

## Get started ​

You can now use page table translation to implement virtual memory in earth/cpu_mmu.c. Start from a fresh copy of egos-2000.

### Identity map ​

Read the setup_identity_region and pagetable_identity_map functions, which set up an identity map, meaning that a virtual address is translated into a physical address of the same value (e.g., map 0x803F_EXXX to 0x803F_EXXX). They help you understand how to set up page table entries in C.

Then read the page_table_map and page_table_switch functions. You can see that they simply call soft_tlb_map and soft_tlb_switch. Hence, when you choose "page tables" as shown below, egos-2000 will enable page table translation with an identity map and still use the software TLB to manage address translations.
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
12345678

With an identity map, the CPU will perform page table translations in a pretty useless way. Still, if we make mistakes in the identity map code, there could be memory exceptions after page table translation starts to take effect. In particular, page table translation starts to take effect in egos-2000 after this mret in grass/init.c.c
```
uint mstatus, M_MODE = 3, U_MODE = 0;
uint GRASS_MODE = (earth->translation == SOFT_TLB) ? M_MODE : U_MODE;
asm("csrr %0, mstatus" : "=r"(mstatus));
mstatus = (mstatus & ~(3 << 11)) | (GRASS_MODE << 11);
asm("csrw mstatus, %0" ::"r"(mstatus));
...
asm("mret");
```
1234567

Code running in machine mode always accesses memory using physical addresses. If earth->translation == PAGE_TABLE, the CPU will switch to user mode (i.e., U_MODE) after the mret and run the first process's code. Since we have enabled Sv32 in satp, and page table translation certainly takes effect in user mode, it starts taking effect right after this mret. All processes run in user mode, with page table translation taking effect, while the kernel runs in machine mode, accessing physical addresses directly.

TIP

There is actually a third privilege mode called supervisor mode (S_MODE = 1), and the 's' in satp stands for supervisor mode, just like the 'm' in mstatus stands for machine mode. Code in machine mode can access the CSRs for supervisor mode, but not vice versa. We are not using this supervisor mode for simplicity—you only need to reason about two privilege modes in P4.

### Page tables per process ​

After understanding the existing code for page table translation in egos-2000, your job is to create one set of page tables for every process. Read the hints in page_table_map. When invoking page_table_map for process pid for the first time, you should initialize the root and leaf page tables for pid. For system processes, this means you should set up an identity map as before. For user processes, set up an identity map only for one page at SHELL_WORK_DIR—the address of workdir in apps/app.h. Processes in egos-2000 share the same work directory, which is the directory shown in the shell. Save the address of the root page table in pid_to_pagetable_base[pid], and modify the page table entries based on the arguments of page_table_map.

In addition, you need to update the page_table_switch function by modifying the satp CSR to point to the root page table for the process pid. Recall that this function is invoked by proc_yield for context switch. Lastly, implement the page_table_translate function, which returns the physical address mapped from vaddr for process pid.

It is possible to reuse both setup_identity_region and pagetable_identity_map as is, but feel free to modify them if necessary. After you finish, you should be able to boot up egos-2000 normally and run commands such as cat README or echo Hello, World!.

TIP

In real-world operating systems like Linux, the difference between a process and a thread is more conceptual than at the code level. Specifically, if processes share the same root page table, we can consider them threads within a single process because they share memory.

### Kill malicious applications ​

Recall that your code for memory protection in P3 can prevent malicious applications such as crash1 or crash2 from halting the operating system. You can now do the same with page table translation: terminate the current process in excp_entry() of grass/kernel.c if an exception is raised, and ensure that the shell can proceed after crash1 and crash2.
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


### Memory overhead ​

We have never talked about how to free page tables when a process terminates. The key is that the proc_free function in grass/process.c invokes earth->mmu_free(pid).

For all page tables allocated for process pid, if we set the pid field of the corresponding page_info_table entries correctly, the page tables should be freed by mmu_free just like the other pages of the terminated process. Nothing special needs to be done.

However, given that page table translation incurs memory overhead, we ask you to print out such overhead in mmu_free() as follows.
```
> make qemu
...
➜ /home/yunhao echo Hello, World!
Hello, World!
[INFO] mmu_free() released 11 pages (2 are page tables) for process 6
```


Make sure that you can explain the numbers printed out by mmu_free.

## Accomplishments ​

You have finished reading library/elf and earth/cpu_mmu.c in egos-2000, and learned how to create the virtual memory illusion for different processes. You have also understood precisely the difference between threads and processes, which is closely related to the root page table of the page table translation mechanism.