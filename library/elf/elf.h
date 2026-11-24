/* ELF 文件头、程序头，以及加载器的接口。这里只使用 32 位 ELF。 */

#pragma once

// ELF 文件头位于文件开头，记录入口地址和各张表的位置。
// 当前加载器从第 0 块读取文件头和程序头表，进程由 Grass 从 APPS_ENTRY 启动。
struct elf32_header {
    uchar e_ident[16];
    ushort e_type;
    ushort e_machine;
    uint e_version;
    uint e_entry; // ELF 声明的程序入口。
    uint e_phoff; // 程序头表在文件中的字节偏移。
    uint e_shoff;
    uint e_flags;
    ushort e_ehsize;
    ushort e_phentsize;
    ushort e_phnum; // 程序头表有多少项。
    ushort e_shentsize;
    ushort e_shnum;
    ushort e_shstrndx;
};

// 每个程序头描述一个段。加载器主要用下面的文件位置、虚拟地址和长度。
struct elf32_program_header {
    uint p_type;
    uint p_offset; // 段在文件中的起始字节位置。
    uint p_vaddr; // 装入进程后使用的起始虚拟地址。
    uint p_paddr;
    uint p_filesz; // 文件里存放的字节数。
    uint p_memsz; // 段在内存中的字节数，多出的部分要清零。
    uint p_flags; // ELF 声明的段权限，当前页表统一使用 USER_RWX。
    uint p_align;
};

// 从 ELF 文件开头数第 block_no 块，读取 512 字节到 dst。
// 系统服务从磁盘固定位置读，普通命令通过文件服务读取。
typedef void (*elf_reader)(uint block_no, char* dst);
// pid 是目标进程。argc 是参数个数，argv 指向 Shell 请求里定长存放的参数字符串。
void elf_load(int pid, elf_reader reader, int argc, void** argv);
