/* 把 ELF 程序装入指定进程的地址空间，并准备参数页和用户栈。 */

#include "egos.h"
#include "elf.h"
#include "disk.h"
#include "servers.h"
#include <string.h>

#define PAGE_SIZE          4096
// 根据物理页池的下标算出这一页的物理地址。
#define PAGE_ID_TO_ADDR(x) ((char*)APPS_PAGES_BASE + x * PAGE_SIZE)

// reader 提供读取程序文件的方法。加载完成后，另由 Grass 安排进程执行。
void elf_load(int pid, elf_reader reader, int argc, void** argv) {
    // 1. 读文件第 0 块。文件头里的 e_phoff 指出程序头表从哪个字节开始。
    // 程序头表描述每个段在文件里的位置，以及运行时要放在哪个虚拟地址。
    char hbuf[BLOCK_SIZE], buf[BLOCK_SIZE];
    reader(0, hbuf);
    struct elf32_header* header          = (void*)hbuf;
    struct elf32_program_header* pheader = (void*)(hbuf + header->e_phoff);

    // 2. 按程序头逐段加载，通常是代码段和数据段。
    for (uint i = 0; i < header->e_phnum; i++) {
        uint addr = pheader[i].p_vaddr;
        if (addr < RAM_START) continue;

        // filesz 是要从文件复制的字节数，memsz 是段在内存中需要的字节数。
        // 两者的差额通常是 .bss 等运行前要清零的数据。
        uint memsz        = pheader[i].p_memsz;
        uint filesz       = pheader[i].p_filesz;
        uint curr_pageno  = addr / PAGE_SIZE;
        uint end_pageno   = (addr + memsz) / PAGE_SIZE;
        // 把文件字节位置换算成块号和块内偏移。例如 600 对应第 1 块的第 88 字节。
        uint file_off = pheader[i].p_offset;
        uint curr_blockno = file_off / BLOCK_SIZE;
        uint buf_offset = file_off % BLOCK_SIZE;
        
        // off 是当前段已复制的字节数。代码按段起始虚拟地址已对齐到页边界来加载。
        for (uint ppage_id, off = 0; off < filesz; ) {
            // 每开始写一页，就申请物理页并将它映射到 pid 的页表。
            if (off % PAGE_SIZE == 0) {
                ppage_id = earth->mmu_alloc();
                earth->mmu_map(pid, curr_pageno++, ppage_id);
                memset(PAGE_ID_TO_ADDR(ppage_id), 0, PAGE_SIZE);
            }
            reader(curr_blockno, buf);
            
            // 一次最多复制到文件块、目标页或段内容的末尾，三个边界中谁先到就停在哪。
            uint copy_size = BLOCK_SIZE - buf_offset;
            uint page_remaining = PAGE_SIZE - (off % PAGE_SIZE);
            if (copy_size > page_remaining) copy_size = page_remaining;
            if (off + copy_size > filesz) copy_size = filesz - off;
            
            memcpy(PAGE_ID_TO_ADDR(ppage_id) + (off % PAGE_SIZE), buf + buf_offset, copy_size);
            
            // 推进读写位置。读完当前文件块后，下一次从下一块的开头读。
            off += copy_size;
            buf_offset += copy_size;
            if (buf_offset == BLOCK_SIZE) {
                buf_offset = 0;
                curr_blockno++;
            }
        }

        // 段在内存中还需要空间时，补齐并清零。当前用 <=，段末尾恰好页对齐时会多分一页。
        while (curr_pageno <= end_pageno) {
            uint ppage_id = earth->mmu_alloc();
            earth->mmu_map(pid, curr_pageno++, ppage_id);
            memset(PAGE_ID_TO_ADDR(ppage_id), 0, PAGE_SIZE);
        }

        // 启动时打印系统服务各段的加载位置。
        if (pid <= GPID_SHELL) INFO("Load 0x%x bytes to 0x%x", filesz, addr);
    }

    // 3. 准备 main 的参数页，目标进程通过 APPS_ARG 访问。
    uint ppage_id = earth->mmu_alloc();
    earth->mmu_map(pid, APPS_ARG / PAGE_SIZE, ppage_id);

    // 一页中依次放 argc、16 个 argv 指针的位置、参数字符串。
    // 加载器通过物理页地址填内容。存进 argv 的指针则是目标进程用来读字符串的虚拟地址。
    int* argc_addr = (int*)PAGE_ID_TO_ADDR(ppage_id);
    int* argv_addr = argc_addr + 1;
    int* args_addr = argv_addr + CMD_NARGS;

    // 每个参数字符串占 CMD_ARG_LEN 字节。复制字符串后，再逐个填写指向它们的指针。
    *argc_addr = argc;
    if (argv) memcpy(args_addr, argv, argc * CMD_ARG_LEN);
    for (uint i = 0; i < argc; i++)
        argv_addr[i] = APPS_ARG + sizeof(uint) /* argc */ +
                       sizeof(void*) * CMD_NARGS /* argv */ + i * CMD_ARG_LEN;

    // 4. 另外分一页给系统调用，映射到 SYSCALL_ARG。同一进程的线程共用这一页。
    ppage_id = earth->mmu_alloc();
    earth->mmu_map(pid, SYSCALL_ARG / PAGE_SIZE, ppage_id);

    // 5. 在 APPS_STACK_TOP 下方映射两页。app.s 会把栈指针设到这个栈顶。
    for (uint i = 1; i <= 2; i++) {
        ppage_id = earth->mmu_alloc();
        earth->mmu_map(pid, APPS_STACK_TOP / PAGE_SIZE - i, ppage_id);
    }
}
