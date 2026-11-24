/*
内存管理，固定使用页表
把内存按 4096 字节分成一页一页，负责分配和回收这些物理页。
用页表记录“程序使用的虚拟地址 -> 实际内存的物理地址”，并为核心切换页表。
页表本身也是放在内存里的数据，同样需要申请物理页来保存。

*/

#include "egos.h"
#include <string.h>
#include "servers.h"

// 每个物理页占 4096 字节（4 KiB）
#define PAGE_SIZE          4096
// 把物理页池下标 x 换成实际地址；例如 x=2，对应 0x80400000 + 2*4096。
#define PAGE_ID_TO_ADDR(x) ((char*)APPS_PAGES_BASE + x * PAGE_SIZE)
// 可分配范围为 [0x80400000, 0x80600000)，共 2 MiB，也就是 512 页。
#define APPS_PAGES_CNT     (RAM_END - APPS_PAGES_BASE) / PAGE_SIZE

// 最多登记 32 份页表关系，不表示 PID 必须小于 32。
#define MAX_PT_ENTRY 32

// 登记“哪个进程使用哪份页表”。本项目的页表分两级，root 指向第一级表。
static struct {
    int pid;    // 进程编号。
    uint* root; // 第一级页表的起始物理地址。
} pt_map[MAX_PT_ENTRY];

// 查找 pid 的页表登记项，没有对应记录就返回 NULL。
// 返回的是 root 这个指针字段的地址，所以调用者用 *r 取得页表地址，
// 也能通过 *r = 新地址修改登记项；因此返回类型是 uint**。
static uint** pt_root(int pid)
{
    for(uint i=0;i<MAX_PT_ENTRY;i++)
        if(pt_map[i].pid==pid) return &pt_map[i].root;
    return NULL;
}

// 为 pid 查找或占用一个登记项；这里只登记，不申请或建立真正的页表。
static uint** pt_root_alloc(int pid)
{
    uint** r = pt_root(pid);
    if(r) return r;
    for(uint i =0;i<MAX_PT_ENTRY;i++)
    {
        if(pt_map[i].pid==0)
        {
            pt_map[i].pid = pid;
            return &pt_map[i].root;
        }
    }
    FATAL("pt_map:too many page tables");
}

// 记录每个物理页是否占用、属于谁。它不是页表，不负责翻译地址。
// 程序的数据页和存放页表的页都会登记，进程退出时可以一起回收。
struct page_info {
    int use; // 0 表示空闲，1 表示已分配。
    int pid; // 拥有这一物理页的进程编号。
} page_info_table[APPS_PAGES_CNT];

// 多个核心可能同时申请内存，用锁避免它们分到同一页。
static int mmu_lock = 0;

// 申请一个空闲物理页，返回页池下标，不是地址。例如返回 2，要再用 PAGE_ID_TO_ADDR(2)。
// 这里仅标记“已占用”，不清零原内容，也不登记 PID，这两件事由后面的调用者完成。
uint mmu_alloc() {
    acquire(mmu_lock);
    for (uint i = 0; i < APPS_PAGES_CNT; i++)
        if (!page_info_table[i].use) {
            page_info_table[i].use = 1;
            release(mmu_lock);
            return i;
        }
    release(mmu_lock);
    FATAL("mmu_alloc: no more free memory");
}

// 回收 pid 对应进程的内存：删除页表登记，再把属于它的所有物理页标为空闲。
void mmu_free(int pid) {
    // PID 1~4 是常驻系统服务，当前实现不回收它们的页。
    if (pid < GPID_USER_START) return;

    acquire(mmu_lock);
    int total_pages = 0;
    int pt_pages = 0;

    uint** r = pt_root(pid);
    if (r && *r) {
        uint* root_pt = *r;
        pt_pages = 1; // 第一级页表占一页；下面每个有效项对应一页第二级页表。
        for (int i = 0; i < 1024; i++)
            if (root_pt[i] & 0x1) ++pt_pages; // 统计页表占了多少页

        // 注销页表登记
        *r = 0;
        for (int i = 0; i < MAX_PT_ENTRY; i++)
            if (pt_map[i].pid == pid) {
                pt_map[i].pid = 0;
                break;
            }
    }

    // 清掉占用/所有者记录，使这些页以后可再次分配；不会把页里的原数据擦零。
    // 这里一并回收一级页表和二级页表所占用的内存
    for (uint i = 0; i < APPS_PAGES_CNT; i++)
        if (page_info_table[i].use && page_info_table[i].pid == pid) {
            ++total_pages;
            memset(&page_info_table[i], 0, sizeof(struct page_info));
        }
    release(mmu_lock);

    INFO("mmu_free() released %d pages (%d are page tables) for process %d",
         total_pages, pt_pages, pid);
}

// 页表每项既保存物理页号，也保存访问许可和状态，称为“页表项”。
// 这里允许用户读、写、执行，并预设访问/写入状态
#define USER_RWX     (0xC0 | 0x1F)

// 建立一段“虚拟地址数值 = 物理地址数值”的恒等映射。
// root：第一级表地址；pid：新页表页归谁；addr：起始地址；npages：映射多少页；flag：访问许可。
void setup_identity_region(uint* root, int pid, uint addr, uint npages, uint flag) {
    // 32 位地址分为：高 10 位选第一级表项，中间 10 位选第二级表项，低 12 位选页内字节。
    uint vpn1 = addr >> 22;
    uint* leaf;

    if (root[vpn1] & 0x1) {
        // 最低位为 1 表示这项有效，取出已经存在的第二级表地址。
        // 页表项中存的是编码后的页号，左移 2 位再清掉低 12 位，才能还原页首地址。
        leaf = (void*)((root[vpn1] << 2) & 0xFFFFF000);
    } else {
        // 没有第二级表，先申请一页存放它；一项 4 字节，共 1024 项，正好占 4096 字节。
        uint ppage_id                 = earth->mmu_alloc();
        leaf                          = (void*)PAGE_ID_TO_ADDR(ppage_id);
        page_info_table[ppage_id].pid = pid;
        memset(leaf, 0, PAGE_SIZE);
        // 页表项把物理页号放在 bit10 以上：(地址 >> 12) << 10 等价于地址 >> 2。
        // 这里只加有效位 0x1，表示“继续到第二级表查找”，还不是程序的数据页。
        root[vpn1] = ((uint)leaf >> 2) | 0x1;
    }

    // &0x3FF 只保留 10 位，取得第二级表中的起始下标。
    uint vpn0 = (addr >> 12) & 0x3FF;
    // 第二级表项才登记实际物理页和访问许可；每前进一页，地址增加 4096 字节。
    for (uint i = 0; i < npages; i++)
        leaf[vpn0 + i] = ((addr + i * PAGE_SIZE) >> 2) | flag;
}

// 为启动阶段（PID 0）或系统服务（PID 1~4）创建一份较完整的地址对照表。
// 系统服务需要直接访问许多内存和设备，因此先按原地址映射这些区域。
void pagetable_identity_map(int pid) {
    // 先申请一页存放第一级表，清零表示最初所有表项都无效。
    uint ppage_id                 = earth->mmu_alloc();
    uint* root                    = (void*)PAGE_ID_TO_ADDR(ppage_id);
    page_info_table[ppage_id].pid = pid;
    memset(root, 0, PAGE_SIZE);

    // 每轮映射 1024 页
    for (uint i = RAM_START; i < RAM_END; i += PAGE_SIZE * 1024)
        setup_identity_region(root, pid, i, 1024, USER_RWX);

    // 映射中断控制器、网卡、串口、定时器、Flash 和显存。
    setup_identity_region(root, pid, PLIC_BASE, 1024, USER_RWX);
    setup_identity_region(root, pid, ETH_CTL_BASE, 4, USER_RWX);
    setup_identity_region(root, pid, UART_BASE, 1, USER_RWX);
    setup_identity_region(root, pid, CLINT_BASE, 16, USER_RWX);
    setup_identity_region(root, pid, FLASH_ROM_BASE, 1024, USER_RWX);
    setup_identity_region(root, pid, VIDEO_FRAME_BASE, 512, USER_RWX);

    setup_identity_region(root, pid, ETH_PCI_ECAM, 1, USER_RWX);
    setup_identity_region(root, pid, SDHCI_PCI_ECAM, 1, USER_RWX);
    setup_identity_region(root, pid, SDHCI_BASE, 1, USER_RWX);

    // 先完成页表内容的写入，再把整份表的起点登记给这个 PID。
    asm volatile("fence rw, rw" ::: "memory");
    *pt_root_alloc(pid) = root;
}

// 给 pid 建立“一页虚拟内存 -> 一页物理内存”的对应关系
// vpage_no 是虚拟地址 / 4096，ppage_id 是 mmu_alloc 返回的物理页池下标。
// 例：page_table_map(5, 0x80200, 2)，表示 PID 5 的 0x80200000 对应物理地址 0x80402000。
void page_table_map(int pid, uint vpage_no, uint ppage_id) {
    // 先找这个进程的第一级表；登记项或表地址不存在时，需要先建立。
    uint** r = pt_root(pid);
    if(!r || !*r)
    {
        // PID小于的对应的是系统服务进程，系统服务进程先建立上面的广泛映射，可以访问一些底层内存
        //普通用户进程只准备需要的共享区域。
        if(pid<GPID_USER_START)
            pagetable_identity_map(pid);
        else
        {
            // 申请内存存放第一级页表
            uint ppage_id_root = earth->mmu_alloc();
            uint* root = (void*)PAGE_ID_TO_ADDR(ppage_id_root);
            page_info_table[ppage_id_root].pid = pid; // 记录这一页第一级表的所有者。
            memset(root,0,PAGE_SIZE);
        
            // 工作目录页供 Shell 和 cd 等进程共享，让它们看到同一份目录信息。
            setup_identity_region(root, pid, SHELL_WORK_DIR, 1, USER_RWX);

            // 给当前进程配置一些必要的映射，关于显示、网卡等等
            setup_identity_region(root, pid, FLASH_ROM_BASE, 512, USER_RWX);
            setup_identity_region(root, pid, VIDEO_FRAME_BASE, 512, USER_RWX);
            // 固定映射 QEMU 的网卡和中断控制器，供网络演示使用。
            setup_identity_region(root, pid, ETH_PCI_ECAM, 1, USER_RWX);
            setup_identity_region(root, pid, ETH_CTL_BASE, 4, USER_RWX);
            setup_identity_region(root, pid, PLIC_BASE, 1024, USER_RWX);
            asm volatile("fence rw, rw" ::: "memory");
            *pt_root_alloc(pid) = root;  // 将新建的第一级表登记到 pt_map。
        }
        r = pt_root(pid);
    } 

    // 建立具体的虚拟与物理的映射
    uint* root = *r;

    // 把两种页号分别换回地址。虚拟页号和物理页池下标不能直接混用。
    uint vaddr = vpage_no * PAGE_SIZE;
    uint paddr = (uint)PAGE_ID_TO_ADDR(ppage_id);

    uint vpn1 = vaddr >> 22;
    uint* leaf;

    if (root[vpn1] & 0x1) {
        // 通过第一级表找到已有的第二级表。
        leaf = (void*)((root[vpn1] << 2) & 0xFFFFF000);
    } else {
        // 申请第二级表。这个块里的同名 ppage_id 指页表页，块外的仍指参数中的数据页。
        uint ppage_id                 = earth->mmu_alloc();
        leaf                          = (void*)PAGE_ID_TO_ADDR(ppage_id);
        page_info_table[ppage_id].pid = pid;
        memset(leaf, 0, PAGE_SIZE);
        asm volatile("fence rw, rw" ::: "memory");
        root[vpn1] = ((uint)leaf >> 2) | 0x1;
    }
    // 中间 10 位选第二级表项，填入物理页号和访问许可。
    uint vpn0 = (vaddr >> 12) & 0x3FF;
    leaf[vpn0] = (paddr>>2) | USER_RWX;
    asm volatile("fence rw, rw" ::: "memory");
    page_info_table[ppage_id].pid = pid; // 记录实际数据页属于哪个进程，以便退出时回收。
}

// 让当前核心改用 pid 对应的页表，改变地址翻译依据。
// satp 为当前核心保存页表设置的寄存器，每个核心都要分别设置。
void page_table_switch(int pid) {
    uint** r = pt_root(pid);
    if(!r || !*r) r = pt_root(0); // 没找到目标表时尝试启动表
    uint* root_pt = *r;

    // root_pt >> 12 换成物理页号；最高位设 1，选择本项目使用的两级 Sv32 页表格式。
    asm("csrw satp, %0" :: "r"(((uint)root_pt>>12)|(1<<31)));
    // 切换完后刷新TLB
    earth->mmu_flush_cache();
}

// 查询 pid 的虚拟地址 vaddr 对应哪个物理地址，供内核读取系统调用参数等场景使用。
// 普通用户程序访问内存时，CPU 会自动按页表翻译，不需要每次调用这个 C 函数。
uint page_table_translate(int pid, uint vaddr) {
    uint** r = pt_root(pid);
    if(!r || !*r) r = pt_root(0);
    uint* root_pt = *r;
    uint vpn1 = vaddr >> 22;
    // 当前遇到无效项会返回原地址，不是完整的地址合法性/权限检查。
    if (!root_pt || !(root_pt[vpn1] & 0x1)) return vaddr;
    uint* leaf = (void*)((root_pt[vpn1] << 2) & 0xFFFFF000);

    uint vpn0 = (vaddr >> 12) & 0x3FF;
    if (!leaf) return vaddr;
    uint pte = leaf[vpn0];
    if (!(pte & 0x1)) return vaddr;

    // 左边还原物理页起点，右边保留原地址在页内的位置，两者拼成完整物理地址。
    uint paddr = ((pte << 2) & 0xFFFFF000) | (vaddr & 0xFFF);
    return paddr;
}

// 刷新CPU缓存的地址翻译结果TLB
void flush_cache() {
    asm("sfence.vma zero,zero"); // 让当前核心不再使用旧的页表翻译结果。
}

// boot.c 调用的初始化入口：登记接口 -> 设置访问许可 -> 建启动页表 -> 告诉当前核心。
void mmu_init() {
    // 先登记分配、回收和刷新接口，后面创建页表时就能通过 Earth 接口申请物理页。
    earth->mmu_free        = mmu_free;
    earth->mmu_alloc       = mmu_alloc;
    earth->mmu_flush_cache = flush_cache;

    // PMP 是物理内存保护：先允许访问 0~4 GiB 的物理地址范围，再由用户页表限制具体访问。
    asm("csrw pmpaddr0, %0" :: "r"(0x40000000));
    asm("csrw pmpcfg0, %0" :: "r"(0xF));

    // 先在内存中建立一份启动页表0，再把这份页表的位置告诉当前 CPU 核心
    // 把页表位置及翻译方式写进 CPU 的 satp 寄存器
    pagetable_identity_map(0);
    asm("csrw satp, %0" :: "r"(((uint)*pt_root(0) >> 12) | (1 << 31)));

    // 登记后续加载程序、调度和处理系统调用时使用的接口；赋值本身不执行这些函数。
    earth->mmu_map       = page_table_map;
    earth->mmu_switch    = page_table_switch;
    earth->mmu_translate = page_table_translate;

    INFO("Sv32 page table translation is enabled");
}
