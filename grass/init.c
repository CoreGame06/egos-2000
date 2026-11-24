/*
Grass 的启动入口
Earth 完成主要底层初始化后，在 boot() 中调用这里的 grass_entry()。
这里登记进程管理等接口，准备第一个进程 sys_proc，然后让 CPU 开始执行它。
sys_proc 运行后，再由它启动终端、文件和 Shell 等系统服务。
*/

#include "process.h"
#include "elf.h"

// 供 elf_load 调用：读取 sys_proc 可执行文件，放到 dst（至少 512 字节）。
static void sys_proc_read(uint block_no, char* dst) {
    earth->disk_read(SYS_PROC_EXEC_START + block_no, 1, dst);
}

// core_id 是执行主要初始化的那个核心编号
void grass_entry(uint core_id) {
    SUCCESS("Enter the grass layer");

    // 1. 登记 Grass 接口。这里只保存函数地址，没有立即创建、回收或切换进程。
    // 例如，之后调用 grass->proc_alloc()，实际就会执行 process.c 中的 proc_alloc()。
    grass->proc_free      = proc_free;
    grass->proc_alloc     = proc_alloc;
    grass->proc_set_ready = proc_set_ready;
    grass->sys_send       = sys_send;
    grass->sys_recv       = sys_recv;
    grass->proc_sleep     = proc_sleep;
    grass->proc_coresinfo = proc_coresinfo;

    // 2. 把第一个进程 sys_proc 的程序加载到内存，准备它的页表、参数页和栈等。
    // GPID_PROCESS 是预定的 PID 1；sys_proc_read 告诉加载器怎样从磁盘取得程序内容。
    INFO("Load kernel process #%d: sys_process", GPID_PROCESS);
    elf_load(GPID_PROCESS, sys_proc_read, 0, 0);

    // 3. 分配进程记录（PCB），保存进程状态和执行现场；第一次分配得到 PID 1。
    // 上面的 elf_load 准备程序和内存，这里的 proc_alloc 准备内核管理这个进程的记录。
    int pid = proc_alloc();

    core_to_proc_idx[core_id] = pid - 1;
    proc_set_running(pid);

    // 4. 让当前核心使用 sys_proc 的页表，并刷新当前核心缓存的地址翻译结果（TLB）。
    earth->mmu_switch(GPID_PROCESS);
    earth->mmu_flush_cache();


    // 设置好运行权限、程序入口和启动参数，然后把 CPU 的执行权交给第一个进程 sys_proc
    uint mstatus;
    asm("csrr %0, mstatus" : "=r"(mstatus));
    mstatus &= ~(3 << 11);
    asm("csrw mstatus, %0" ::"r"(mstatus));

    asm("csrw mepc, %0" ::"r"(APPS_ENTRY));

    asm("mv a0, %0" ::"r"(APPS_ARG));

    asm("mv a1, %0" ::"r"(&boot_lock));

    asm("mret");
}
