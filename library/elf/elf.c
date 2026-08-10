/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: ELF-format executable file loader
 */

#include "egos.h"
#include "elf.h"
#include "disk.h"
#include "servers.h"
#include <string.h>

#define PAGE_SIZE          4096
#define PAGE_ID_TO_ADDR(x) ((char*)APPS_PAGES_BASE + x * PAGE_SIZE)

void elf_load(int pid, elf_reader reader, int argc, void** argv) {
    /* Load the ELF header. */
    char hbuf[BLOCK_SIZE], buf[BLOCK_SIZE];
    reader(0, hbuf);
    struct elf32_header* header          = (void*)hbuf;
    struct elf32_program_header* pheader = (void*)(hbuf + header->e_phoff);

    /* Load the code and data memory regions. */
    for (uint i = 0; i < header->e_phnum; i++) {
        uint addr = pheader[i].p_vaddr;
        if (addr < RAM_START) continue;

        uint memsz        = pheader[i].p_memsz;
        uint filesz       = pheader[i].p_filesz;
        uint curr_pageno  = addr / PAGE_SIZE;
        uint end_pageno   = (addr + memsz) / PAGE_SIZE;
        uint file_off = pheader[i].p_offset;
        uint curr_blockno = file_off / BLOCK_SIZE;
        uint buf_offset = file_off % BLOCK_SIZE;
        
        for (uint ppage_id, off = 0; off < filesz; ) {
            if (off % PAGE_SIZE == 0) {
                ppage_id = earth->mmu_alloc();
                earth->mmu_map(pid, curr_pageno++, ppage_id);
                memset(PAGE_ID_TO_ADDR(ppage_id), 0, PAGE_SIZE);
            }
            reader(curr_blockno, buf);
            
            uint copy_size = BLOCK_SIZE - buf_offset;
            uint page_remaining = PAGE_SIZE - (off % PAGE_SIZE);
            if (copy_size > page_remaining) copy_size = page_remaining;
            if (off + copy_size > filesz) copy_size = filesz - off;
            
            memcpy(PAGE_ID_TO_ADDR(ppage_id) + (off % PAGE_SIZE), buf + buf_offset, copy_size);
            
            off += copy_size;
            buf_offset += copy_size;
            if (buf_offset == BLOCK_SIZE) {
                buf_offset = 0;
                curr_blockno++;
            }
        }

        while (curr_pageno <= end_pageno) {
            uint ppage_id = earth->mmu_alloc();
            earth->mmu_map(pid, curr_pageno++, ppage_id);
            memset(PAGE_ID_TO_ADDR(ppage_id), 0, PAGE_SIZE);
        }

        /* Numbers printed should match the numbers in build/debug/sys_*.lst. */
        if (pid <= GPID_SHELL) INFO("Load 0x%x bytes to 0x%x", filesz, addr);
    }

    /* Set up a page for main() arguments (argc and argv). */
    uint ppage_id = earth->mmu_alloc();
    earth->mmu_map(pid, APPS_ARG / PAGE_SIZE, ppage_id);

    int* argc_addr = (int*)PAGE_ID_TO_ADDR(ppage_id);
    int* argv_addr = argc_addr + 1;
    int* args_addr = argv_addr + CMD_NARGS;

    /* Initialize argc and argv. */
    *argc_addr = argc;
    if (argv) memcpy(args_addr, argv, argc * CMD_ARG_LEN);
    for (uint i = 0; i < argc; i++)
        argv_addr[i] = APPS_ARG + sizeof(uint) /* argc */ +
                       sizeof(void*) * CMD_NARGS /* argv */ + i * CMD_ARG_LEN;

    /* Set up a page for system call arguments. */
    ppage_id = earth->mmu_alloc();
    earth->mmu_map(pid, SYSCALL_ARG / PAGE_SIZE, ppage_id);

    /* Set up 2 pages for the user stack (enough for teaching purposes). */
    for (uint i = 1; i <= 2; i++) {
        ppage_id = earth->mmu_alloc();
        earth->mmu_map(pid, APPS_STACK_TOP / PAGE_SIZE - i, ppage_id);
    }
}
