/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: wrapping the CPU interface for memory management unit (MMU)
 * This file contains functions for memory allocation/free, virtual memory
 * address translation (software TLB and page table), and cache flushing.
 */

#include "egos.h"
#include <string.h>
#include "servers.h"

#define PAGE_SIZE          4096
#define PAGE_NO_TO_ADDR(x) (char*)(x * PAGE_SIZE)
#define PAGE_ID_TO_ADDR(x) ((char*)APPS_PAGES_BASE + x * PAGE_SIZE)
#define APPS_PAGES_CNT     (RAM_END - APPS_PAGES_BASE) / PAGE_SIZE

#define MAX_NPROCESS 256
static uint* pid_to_pagetable_base[MAX_NPROCESS];

struct page_info {
    int use;
    int pid;
    uint vpage_no;
} page_info_table[APPS_PAGES_CNT];

uint mmu_alloc() {
    for (uint i = 0; i < APPS_PAGES_CNT; i++)
        if (!page_info_table[i].use) {
            page_info_table[i].use = 1;
            return i;
        }
    FATAL("mmu_alloc: no more free memory");
}

void mmu_free(int pid) {
    int total_pages = 0;
    int pt_pages = 0;

    if(earth->translation == PAGE_TABLE && pid>=GPID_USER_START)
    {
        uint* root_pt = pid_to_pagetable_base[pid];
        if(root_pt)
        {
            pt_pages = 1;
            for(int i=0;i<1024;i++)
            {
                if(root_pt[i]&(0x1))
                    ++pt_pages;
            }    
        }
        pid_to_pagetable_base[pid] = 0;
    }
    for (uint i = 0; i < APPS_PAGES_CNT; i++)
        if (page_info_table[i].use && page_info_table[i].pid == pid)
        {
            ++total_pages; //统计物理页的使用
            memset(&page_info_table[i], 0, sizeof(struct page_info));
        }

    if(earth->translation == PAGE_TABLE && pid>=GPID_USER_START)
    {
        INFO("mmu_free() released %d pages (%d are page tables) for process %d",
        total_pages, pt_pages, pid);
    }

}
void soft_tlb_map(int pid, uint vpage_no, uint ppage_id) {
    page_info_table[ppage_id].pid      = pid;
    page_info_table[ppage_id].vpage_no = vpage_no;
}

void soft_tlb_switch(int pid) {
    static int curr_vm_pid = -1;
    if (pid == curr_vm_pid) return;

    /* Unmap curr_vm_pid from the user address space. */
    for (uint i = 0; i < APPS_PAGES_CNT; i++)
        if (page_info_table[i].use && page_info_table[i].pid == curr_vm_pid)
            memcpy(PAGE_ID_TO_ADDR(i),
                   PAGE_NO_TO_ADDR(page_info_table[i].vpage_no), PAGE_SIZE);

    /* Map pid to the user address space. */
    for (uint i = 0; i < APPS_PAGES_CNT; i++)
        if (page_info_table[i].use && page_info_table[i].pid == pid)
            memcpy(PAGE_NO_TO_ADDR(page_info_table[i].vpage_no),
                   PAGE_ID_TO_ADDR(i), PAGE_SIZE);

    curr_vm_pid = pid;
}

uint soft_tlb_translate(int pid, uint vaddr) {
    soft_tlb_switch(pid);
    return vaddr;
}

/* The code below creates an identity map using page tables (RISC-V Sv32). */
#define USER_RWX     (0xC0 | 0x1F)
static uint* root;
static uint* leaf;
/* Assume at most MAX_NPROCESS unique processes, just for simplicity. */

void setup_identity_region(int pid, uint addr, uint npages, uint flag) {
    uint vpn1 = addr >> 22;

    if (root[vpn1] & 0x1) {
        /* Leaf has been allocated. */
        leaf = (void*)((root[vpn1] << 2) & 0xFFFFF000);
    } else {
        /* Allocate the leaf page table. */
        uint ppage_id                 = earth->mmu_alloc();
        leaf                          = (void*)PAGE_ID_TO_ADDR(ppage_id);
        page_info_table[ppage_id].pid = pid;
        memset(leaf, 0, PAGE_SIZE);
        root[vpn1] = ((uint)leaf >> 2) | 0x1;
    }

    /* Set up the entries in the leaf page table. */
    uint vpn0 = (addr >> 12) & 0x3FF;
    for (uint i = 0; i < npages; i++)
        leaf[vpn0 + i] = ((addr + i * PAGE_SIZE) >> 2) | flag;
}

/*
 * 【系统进程的直接映射 (Identity Mapping) 与开机自举机制】
 * 1. 本函数的作用：为系统进程（PID 0, 1, 2）分配根页表和所有需要的叶页表，
 *    并将整个 6MB 的物理内存以及所有硬件外设（网卡、串口等）进行 1:1 恒等映射。
 * 2. 为什么必须建立直接映射：正是因为操作系统在一开始（自举阶段）需要
 *    【直接读取和操控真实的物理内存地址】（如分配物理页、清零页表、读写硬件）。
 *    如果虚拟地址和物理地址不同，一旦开启虚拟内存（MMU），内核代码瞬间就会因为
 *    地址对不上而崩溃。因此必须用直接映射来确保“内核想访问的物理地址”等于“CPU翻译后的地址”。
 *    这也就是为什么必须先启用系统进程，然后才可以有用户进程（先有地基，后建大楼）。
 * 
 * TODO 架构优化点（线程与进程的本质区别）：
 * 目前为了代码简单，每次启动一个系统进程，都会重新调用本函数建立一份独立的 6MB 映射。
 * 在现代工业级操作系统（如 Linux）的设计中，所有的系统进程（内核线程）
 * 完全可以共享同一份内核页表。这样不仅能省去大量的页表内存开销，
 * 还能免去在系统进程间切换时刷新 TLB 和寄存器的开销。留待后续优化！
 */
void pagetable_identity_map(int pid) {
    /* Allocate the root page table. */
    uint ppage_id                 = earth->mmu_alloc();
    root                          = (void*)PAGE_ID_TO_ADDR(ppage_id);
    page_info_table[ppage_id].pid = pid;
    pid_to_pagetable_base[pid]    = root;
    memset(root, 0, PAGE_SIZE);

    /* Set up the identity map for various memory regions. */
    for (uint i = RAM_START; i < RAM_END; i += PAGE_SIZE * 1024)
        setup_identity_region(pid, i, 1024, USER_RWX);

    setup_identity_region(pid, ETH_CTL_BASE, 4, USER_RWX);
    setup_identity_region(pid, UART_BASE, 1, USER_RWX);
    setup_identity_region(pid, CLINT_BASE, 16, USER_RWX);
    setup_identity_region(pid, FLASH_ROM_BASE, 1024, USER_RWX);
    setup_identity_region(pid, VIDEO_FRAME_BASE, 512, USER_RWX);

    if (earth->platform == QEMU) {
        setup_identity_region(pid, ETH_PCI_ECAM, 1, USER_RWX);
        setup_identity_region(pid, SDHCI_BASE, 1, USER_RWX);
    } else {
        setup_identity_region(pid, SDSPI_BASE, 1, USER_RWX);
        setup_identity_region(pid, WIFI_BASE, 1, USER_RWX);
        setup_identity_region(pid, ETH_BUF_BASE, 2, USER_RWX);
    }
}

void page_table_map(int pid, uint vpage_no, uint ppage_id) {
    if (pid >= MAX_NPROCESS) FATAL("page_table_map: pid too large");

    /* Student's code goes here (Virtual Memory). */

    /* Remove the soft_tlb_map below and do the following.
     * (1) If page tables for process pid do not exist, initialize page tables.
     *   * Initialization Case#1: pid < GPID_USER_START
     * | Start Address   | # Pages | Size  | Explanation                       |
     * +-----------------+---------+-------+-----------------------------------+
     * | RAM_START       | 512     | 2 MB  | EGOS code, data, heap, stack      |
     * | APPS_ENTRY      | 512     | 2 MB  | Apps code, data, heap, stack      |
     * | APPS_PAGES_BASE | 512     | 2 MB  | Memory pages for page_alloc()     |
     * | UART_BASE       | 1       | 4 KB  | Memory-mapped registers for TTY   |
     * | SDHCI_BASE      | 1       | 4 KB  | Memory-mapped registers for SD    |
     * | CLINT_BASE      | 16      | 64 KB | Memory-mapped registers for timer |
     *
     *   * Initialization Case#2: pid >= GPID_USER_START
     * | Start Address   | # Pages | Size  | Explanation                       |
     * +-----------------+---------+-------+-----------------------------------+
     * | SHELL_WORK_DIR  | 1       | 4 KB  | Work dir (see apps/app.h)         |
     * In later projects, you can add mappings for Ethernet, Wi-Fi, or VGA/HDMI.
     *
     * (2) After page tables for process pid have been initialized, update the
     *     page tables and map *vpage_no* to *ppage_id* according to Sv32. */
    if(!pid_to_pagetable_base[pid])
    {
        if(pid<GPID_USER_START)
            pagetable_identity_map(pid);
        else
        {
            uint ppage_id_root = earth->mmu_alloc();
            root = (void*)PAGE_ID_TO_ADDR(ppage_id_root);
            page_info_table[ppage_id_root].pid = pid; //正向
            pid_to_pagetable_base[pid] = root;  // 反向
            memset(root,0,PAGE_SIZE);
        
            setup_identity_region(pid,SHELL_WORK_DIR,1,USER_RWX); // SHELL_WORK_DIR要求恒等映射，实现共享内存
            /* 2.为用户进程映射 Flash ROM (放着图片) 和 VGA 显存 (用于显示) */
            setup_identity_region(pid, FLASH_ROM_BASE, 512, USER_RWX);
            setup_identity_region(pid, VIDEO_FRAME_BASE, 512, USER_RWX);
            /* 映射 EARTH_STRUCT 以便用户态宏定义(如FLASH_ROM_BASE)中可以访问 earth->platform */
            setup_identity_region(pid, EARTH_STRUCT, 1, USER_RWX);
        }
    } 

    root = pid_to_pagetable_base[pid];

    uint vaddr = vpage_no * PAGE_SIZE;
    uint paddr = (uint)PAGE_ID_TO_ADDR(ppage_id);

    uint vpn1 = vaddr >> 22;

    if (root[vpn1] & 0x1) {
        /* Leaf has been allocated. */
        leaf = (void*)((root[vpn1] << 2) & 0xFFFFF000);
    } else {
        /* Allocate the leaf page table. */
        uint ppage_id                 = earth->mmu_alloc();
        leaf                          = (void*)PAGE_ID_TO_ADDR(ppage_id);
        page_info_table[ppage_id].pid = pid;
        memset(leaf, 0, PAGE_SIZE);
        root[vpn1] = ((uint)leaf >> 2) | 0x1;
    }
    uint vpn0 = (vaddr >> 12) & 0x3FF;
    leaf[vpn0] = (paddr>>2) | USER_RWX;
    page_info_table[ppage_id].pid = pid;
    page_info_table[ppage_id].vpage_no = vpage_no;
    /* Student's code ends here. */
}

void page_table_switch(int pid) {
    /* Student's code gvoes here (Virtual Memory). */

    /* Remove the soft_tlb_switch below and, instead, update the page table
     * base register (satp) using the value of pid_to_pagetable_base[pid].
     * An example of updating the satp CSR is given in mmu_init(). */
    uint* root_pt = pid_to_pagetable_base[pid];
    asm("csrw satp, %0" :: "r"(((uint)root_pt>>12)|(1<<31)));
    // 清楚CPU内部的TLB缓(缓存进程的虚拟内存->物理内存)
    earth->mmu_flush_cache();
    /* Student's code ends here. */
}

uint page_table_translate(int pid, uint vaddr) {
    /* Student's code goes here (Virtual Memory). */

    /* Remove the following line of code. Walk through the page tables
     * for process pid and return the physical address mapped from vaddr. */
    uint* root_pt = pid_to_pagetable_base[pid];

    // 计算一级索引
    uint vpn1 = vaddr>>22;
    leaf = (void*)((root_pt[vpn1] << 2) & 0xFFFFF000);

    uint vpn0 = (vaddr >> 12) & 0x3FF;
    uint pte = leaf[vpn0];

    // 逆向leaf[vpn0] = (paddr>>2) | USER_RWX; 然后补充偏移量
    uint paddr = ((pte<<2) & (0xFFFFF000)) | (vaddr & 0xFFF);
    return paddr;
    /* Student's code ends here. */
}

void flush_cache() {
    if (earth->platform == HARDWARE) {
        /* Flush the L1 instruction cache. */
        /* See
         * https://github.com/yhzhang0128/litex/blob/egos/litex/soc/cores/cpu/vexriscv_smp/system.h#L9-L25
         */
        asm(".word(0x100F)\nnop\nnop\nnop\nnop\nnop\n");
    }
    if (earth->translation == PAGE_TABLE) {
        /* Flush the TLB, the cache for page table entries. */
        /* See
         * https://riscv.org/wp-content/uploads/2017/05/riscv-privileged-v1.10.pdf#subsection.4.2.1
         */
        asm("sfence.vma zero,zero");
    }
}

void mmu_init() {
    earth->mmu_free        = mmu_free;
    earth->mmu_alloc       = mmu_alloc;
    earth->mmu_flush_cache = flush_cache;

    /* Set up a PMP region for the whole 4GB address space. */
    asm("csrw pmpaddr0, %0" : : "r"(0x40000000));
    asm("csrw pmpcfg0, %0" : : "r"(0xF));

    /* Student's code goes here (System Call & Protection). */

    /* Replace the PMP region above with two PMP NAPOT regions:
     * [APPS_ENTRY, APPS_ENTRY + 2MB) with permission R/W/X;
     * [SHELL_WORK_DIR, SHELL_WORK_DIR + 4KB) with permission R/W. */
     uint pmpaddr0 = (APPS_ENTRY>>2) | (((2*1024*1024)>>3)-1);
     uint pmpaddr1 = (SHELL_WORK_DIR>>2) | (((4*1024)>>3)-1);
     asm("csrw pmpaddr0, %0": : "r"(pmpaddr0));
     asm("csrw pmpaddr1, %0": : "r"(pmpaddr1));

     // 写入权限
    /*
    权限要求：R=1, W=1, X=1 → 二进制 0000 0111（十六进制 0x7）
    式要求：NAPOT 模式 → 二进制 0001 1000（十六进制 0x18）
    按位或合并：0x18 | 0x7 = 0x1F （二进制 0001 1111）
    */
    uint pmpcfg0 = 0x1f | (0x1B<<8);
    asm("csrw pmpcfg0, %0": : "r"(pmpcfg0));
    /* Student's code ends here. */

    CRITICAL("Choose a memory translation mechanism:");
    printf("Enter 0: page tables.\n\rEnter 1: software TLB.\n\r");

    char buf[2];
    for (buf[0] = 0; buf[0] != '0' && buf[0] != '1'; earth->tty_read(&buf[0]));
    earth->translation = (buf[0] == '0') ? PAGE_TABLE : SOFT_TLB;
    INFO("%s translation is chosen",
         earth->translation == PAGE_TABLE ? "Page table" : "Software");

    if (earth->translation == PAGE_TABLE) {
        /* Set up an identity map using page tables. */
        // 选择虚拟内存后，不在依赖物理内存墙来限制用户进程的访问空间
        asm("csrw pmpaddr0, %0" : : "r"(0x40000000));
        asm("csrw pmpcfg0, %0" : : "r"(0xF));
        pagetable_identity_map(0);
        asm("csrw satp, %0" ::"r"(((uint)root >> 12) | (1 << 31)));

        earth->mmu_map       = page_table_map;
        earth->mmu_switch    = page_table_switch;
        earth->mmu_translate = page_table_translate;
    } else {
        earth->mmu_map       = soft_tlb_map;
        earth->mmu_switch    = soft_tlb_switch;
        earth->mmu_translate = soft_tlb_translate;
    }
}
