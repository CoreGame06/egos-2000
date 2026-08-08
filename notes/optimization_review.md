# egos-2000 代码审查与优化建议（v2，已二次复核）


## 建议修复优先级总览

| # | 类别 | 位置 | 问题 | 优先级 |
|---|------|------|------|--------|
| A1 | 正确性 | grass/kernel.c `intr_entry()` | 网卡接收中断处理被删除，收包会导致死机 | ★★★ |
| A2 | 正确性 | grass/process.c + earth/cpu_mmu.c | pid 只增不减，约 250 次 spawn 后 FATAL/越界 | ★★★ |
| A6 | 正确性 | grass/process.c `mlfq_update_level()` | ulonglong 先减后比较，时间配额永远用不完 | ★★★ |
| B1 | 性能 | library/file/file0.c | FAT 文件系统零缓存，读 O(n) I/O、写扩展 O(n²) | ★★★ |
| A3 | 正确性 | library/syscall/servers.c `dir_lookup()` | unsigned 下溢导致越界读 | ★★ |
| A4 | 代码质量 | earth/cpu_mmu.c | `pid_to_pagetable_base` 重复声明；MAX_NPROCESS 定义冲突 | ★★ |
| A5 | 并发 | grass/kernel.c `proc_yield()` wfi 路径 | 内核锁用普通 store 释放，缺 release 屏障 | ★★ |
| B2 | 正确性+质量 | library/libc/print.c | %c/%u/%lld 等格式符未实现（静默丢输出）；O(n²) 拼接 | ★★ |
| C1 | 工程 | build/release/user/web_server.elf | 无源码的陈旧 ELF 被打进磁盘镜像 | ★★ |
| B3 | 性能 | grass/kernel.c `proc_yield()` | 睡眠检查每进程重复 MMIO 读 mtime | ★ |
| C2 | 工程 | test_qemu.sh | sed 改源码的脆弱脚本，且已失效 | ★ |
| C3 | 代码质量 | apps/system/sys_proc.c | `shell_waiting` 未初始化即使用 | ★ |

---

## A. 正确性问题

### A1. 网卡接收中断处理缺失（死机隐患）★★★ —— 复核：确认必须改

**位置**：`grass/kernel.c` `intr_entry()`（约 L100–114）

**原因**：P8 阶段的提交把外部中断（id==11）的处理代码删掉了
（可用 `git diff eace988 0e9bb0d -- grass/kernel.c` 查看被删内容），
但外部中断的使能链路仍然完整：

- `earth/cpu_intr.c:52` 开启了 `mie` 的外部中断位（1<<11）；
- `earth/cpu_intr.c:54-56` 在 PLIC 中启用了 ETH_IRQ；
- `earth/boot.c:100-119` 初始化了 e1000 的 RX 环并使能接收。

**后果**：一旦有以太网帧到达（按 runtip.md 接上 tap 网卡演示 UDP 时），
PLIC 中断没有被 claim/complete，MEIP 一直保持 pending，
CPU 每次 `mret` 后立刻再次陷入 trap，形成无限 trap 循环，整机卡死。
目前默认 `make qemu` 不挂网卡所以从未暴露。**这不是风格问题，是硬伤。**

**修改建议**：在 `intr_entry()` 中恢复外部中断处理，并把原来硬编码的
`PLIC_CAUSE(0)` 改为 `PLIC_CAUSE(core_in_kernel)`（原版写死 core 0，
4 核下其他核收到中断时会 claim 错上下文——这是相对原 P7 代码的改进点）：

```c
#define INTR_ID_EXTERNAL 11

/* 在 intr_entry() 的 timer 分支之后添加 */
if (id == INTR_ID_EXTERNAL) {
    /* (1) Claim 当前核的 PLIC 中断 */
    uint irq = REGW(PLIC_CAUSE(core_in_kernel), 0);
    if (irq == ETH_IRQ) {
        /* (2)(3) 读 ICR 清网卡中断，并排空 RX 环 */
        REGW(ETH_CTL_BASE, 0xC0);

        static int rx_head = 0;   /* 内核锁保证同一时刻只有一个核在此，static 安全 */
        while (rx_desc[rx_head].status & 1) {
            ushort len = rx_desc[rx_head].length;
            rx_buffer[rx_head][len < 2047 ? len : 2047] = '\0';

            uchar* pkt = (uchar*)rx_buffer[rx_head];
            ushort ethertype = (pkt[12] << 8) | pkt[13];
            uchar proto = pkt[23];
            INFO("Got pkt: rxdesc[%d], len=%d, ethertype=0x%x, proto=%d",
                 rx_head, len, ethertype, (uint)proto);
            if (ethertype == 0x0800 && proto == 17) {
                SUCCESS("Get %d bytes from rxdesc[%d]: %s",
                        len, rx_head, &rx_buffer[rx_head][42]);
            }

            rx_desc[rx_head].status = 0;
            REGW(ETH_CTL_BASE, 0x2818) = rx_head;  /* 写 RDT 归还描述符 */
            rx_head = (rx_head + 1) % 128;
        }
    }
    /* (4) Complete，让 PLIC 可以触发下一个中断；irq==0 是伪中断，不写 */
    if (irq) REGW(PLIC_CAUSE(core_in_kernel), 0) = irq;
    return proc_yield();
}
```

**验证**：按 runtip.md 配置 virbr0 + tap，`sudo make qemu`（Makefile 里
挂上 `QEMU_ETHERNET_LINUX`），宿主机 `nc -u 192.168.18.2 8002` 发包，
应看到 "Got pkt" 日志且系统不卡死。

---

### A2. pid 只增不减导致页表数组溢出 ★★★ —— 复核：确认必须改，且比 v1 判断的更早爆

**位置**：
- `grass/process.c:27-30`：`static uint curr_pid = 0; proc_set[i].pid = ++curr_pid;`
- `earth/cpu_mmu.c:19-20`：`#define MAX_NPROCESS 256; static uint* pid_to_pagetable_base[MAX_NPROCESS];`
- `earth/cpu_mmu.c:181`：`if (pid >= MAX_NPROCESS) FATAL("page_table_map: pid too large");`

**原因**：进程槽只有 16 个、可复用，但 pid 是单调递增的静态计数器，
从不回收；而页表基址数组按 pid 索引。

**后果（复核后补充）**：
- 每执行一条 shell 命令消耗一个 pid，**约 250 条命令后 FATAL 死机**；
- 更早的隐患：`page_table_translate()`（cpu_mmu.c:250）做
  `pid_to_pagetable_base[pid]` 时**没有越界检查**，pid≥256 时先越界读，
  之后 `page_table_map` 才 FATAL。现场演示连续敲命令就能踩到。

**修改建议（方案 A，推荐）**：页表不再按 pid 直接索引，改维护一个
可复用的 pid→根页表 映射表，改动集中在 cpu_mmu.c：

```c
/* 替换 pid_to_pagetable_base[MAX_NPROCESS] */
#define MAX_PT_ENTRY 32
static struct { int pid; uint* root; } pt_map[MAX_PT_ENTRY]; /* pid=0 表示空槽 */

static uint** pt_root(int pid) {          /* 查找，找不到返回 NULL */
    for (uint i = 0; i < MAX_PT_ENTRY; i++)
        if (pt_map[i].pid == pid) return &pt_map[i].root;
    return NULL;
}
static uint** pt_root_alloc(int pid) {    /* 查找或分配一个空槽 */
    uint** r = pt_root(pid);
    if (r) return r;
    for (uint i = 0; i < MAX_PT_ENTRY; i++)
        if (pt_map[i].pid == 0) { pt_map[i].pid = pid; return &pt_map[i].root; }
    FATAL("pt_map: too many page tables");
}
```

替换点（全部在 cpu_mmu.c 内，对外接口签名不变）：
- `pagetable_identity_map()` 写根页表处：`*pt_root_alloc(pid) = root;`
- `page_table_map()`：开头 `if(!pid_to_pagetable_base[pid])` 改为
  `uint** r = pt_root(pid); if (!r || !*r) { ...分配并建表... r = pt_root_alloc(pid); *r = root; }`，
  函数尾部 `uint* root = pid_to_pagetable_base[pid];` 改为 `uint* root = *pt_root(pid);`，
  并删除 `pid >= MAX_NPROCESS` 的 FATAL；
- `mmu_free()`：`uint** r = pt_root(pid); if (r) { *r = 0; 把该槽 pid 清 0; }`；
- `page_table_switch()` / `page_table_translate()`：
  `uint** r = pt_root(pid); if (!r || !*r) r = pt_root(0); uint* root_pt = *r;`
  （保留"找不到就回退 pid 0 恒等映射"的原语义，越界问题自然消失）；
- `mmu_init()` 里 `pid_to_pagetable_base[0]` 改为 `*pt_root_alloc(0)`。

**为什么不用方案 B（pid=槽位+1 复用）**：一行就能改完，但复用后的 pid
理论上可能撞上 GPID_* 的消息语义（消息按 pid 投递），且统计输出里
同一个 pid 反复出现，面试被追问时解释成本高。方案 A 只动一个文件、
对外语义不变，更稳。

---

### A6. MLFQ 降级逻辑的无符号回绕 bug ★★★ —— v2 新增（v1 误判为"恰好正确"）

**位置**：`grass/process.c:94`

```c
if ((p->level_time_left -= runtime) <= 0) { ...升级并重置... }
```

**原因**：`level_time_left` 是 ulonglong。`MLFQ_LEVEL_RUNTIME(0)=1000000`，
QUANTUM=100000（cpu_intr.c:13）。理论上 10 个时间片正好扣完，
但定时器中断的实际间隔是 `>= QUANTUM` 的（mtimecmp 触发有 1~数个 tick
的抖动），10 次累减几乎必然**扣过头**：一旦 `runtime > level_time_left`，
减法回绕成接近 2⁶⁴ 的巨大正数，`<= 0` 永远为假。

**后果**：进程一旦"扣过头"就永远不会再被降级，卡在当前优先级队列，
直到 10 秒一次的 `mlfq_reset_level()` 全体重置。MLFQ 的核心机制
（惩罚 CPU 密集型进程）实际大部分时间失效。交互式演示看不出来，
但如果面试官问"你的 MLFQ 怎么降级的"，按现在的代码讲会露馅。

**修改建议**：先比较、后减：

```c
void mlfq_update_level(struct process* p, ulonglong runtime) {
    if (p->level_time_left <= runtime) {
        if (p->level < MLFQ_NLEVELS - 1) ++p->level;
        p->level_time_left = MLFQ_LEVEL_RUNTIME(p->level);
    } else {
        p->level_time_left -= runtime;
    }
}
```

**验证方法**：改完后 `loop &`（纯 CPU 密集），观察它是否在约 1~5 秒内
逐级降到 level 4（改之前大概率停在 level 0 直到 10s 重置）；
也可以临时在 mlfq_update_level 里加一行 INFO 打印 level 变化来确认。

---

### A3. dir_lookup 的 unsigned 下溢 ★★ —— 复核：确认必须改

**位置**：`library/syscall/servers.c:41`

```c
for (uint i = 0, namelen = strlen(name); i < strlen(buf) - namelen; i++)
```

**原因**：两个 unsigned 相减，当目录块内容比查找的名字短时
（比如在只有两三个条目的目录里输一个长文件名），下溢为接近 2³² 的值。

**后果**：循环越界读 buf（栈上 512 字节之外），可能崩溃或返回错误 ino。

**修改建议**：

```c
int dir_lookup(int dir_ino, char* name) {
    char buf[BLOCK_SIZE];
    file_read(dir_ino, 0, buf);

    uint blen = strlen(buf), namelen = strlen(name);
    if (blen <= namelen) return -1;          /* 防下溢；相等也不可能匹配（条目后必跟空格+ino） */

    for (uint i = 0; i < blen - namelen; i++)
        if (!strncmp(name, buf + i, namelen) && buf[i + namelen] == ' ' &&
            (i == 0 || buf[i - 1] == ' '))
            return atoi(buf + i + namelen);
    return -1;
}
```

---

### A4. pid_to_pagetable_base 重复声明 & MAX_NPROCESS 冲突 ★★ —— 复核：确认要改（若采用 A2 方案 A 则自动解决）

**位置**：`earth/cpu_mmu.c:20` 与 `earth/cpu_mmu.c:105`（两处相同的
`static uint* pid_to_pagetable_base[MAX_NPROCESS];`）；
`earth/cpu_mmu.c:19` 本地 `#define MAX_NPROCESS 256`，而
`grass/process.h:37` 中是 16。

**原因**：两处都是文件作用域的 tentative definition，GCC 恰好把它们
合并成一个符号所以能编译（已实测确认），但这依赖编译器行为；
同名宏 256（pid 上界）与 16（进程槽数）语义完全不同，极易误导读者。

**修改建议**：若做 A2 方案 A，数组整个被 pt_map 替换，此问题自动消失；
否则删除 L105 的重复声明，并把 L19 的宏改名为 `MAX_PID`。

---

### A5. wfi 路径释放内核锁缺 release 屏障 ★★ —— 复核：确认要改，一行零风险

**位置**：`grass/kernel.c:192` `kernel_lock = 0;`

**原因**：RV32 弱内存序，普通 store 之前的内存访问（如刚更新的
`curr_proc_idx`、PCB 字段）可能被重排到锁释放之后，其他核拿锁后
读到旧值。项目其他地方都用 `release()` 宏（`__sync_lock_release`，
带屏障），唯独这处是裸赋值。QEMU 上大概率观测不到，但这是**概念正确性**
问题——面试讲多核时若被问"你释放锁的地方都带屏障吗"，这是唯一的反例。

**修改建议**：改为 `release(kernel_lock);`

**进阶（可选）**：`grass/kernel.s` Step7 用 `sw zero,0(t0)` 释放锁，
更严谨的写法是 `amoswap.w.rl zero, zero, (t0)`（rl = release 语义），
与 Step1 的 `amoswap.w.aq` 构成标准 acquire/release 对——这是个很好的
面试谈资（能讲清 aq/rl 位的作用）。

---

## B. 性能问题

### B1. FAT 文件系统无缓存 ★★★ —— 复核：问题成立，但实现方案必须改（v1 方案会崩）

**位置**：`library/file/file0.c` 的 `get_inode/set_node/get_fat/set_fat`
以及 `mydisk_read/mydisk_write/mydisk_setsize`。

**问题（成立，数据复核无误）**：每次访问 FAT 表项或 inode 都是完整的
磁盘块读（`set_fat` 是读+写两次）。由此：

1. `mydisk_read(ino, offset)` 从链头走到 offset：**O(offset) 次磁盘 I/O**；
2. `mydisk_write` 每扩展一块都要重新走到链尾：扩展 n 块合计 **O(n²)**；
3. 每个操作还要先读一次 superblock。

2MB FS 分区共 4096 块，FAT 表只有 F=32 块 = **16KB**，inode 表
I=2 块 = **1KB**（mydisk_create 里 I=(ninodes+63)/64，NINODES=128）。
全部常驻内存完全可行。

**⚠️ v2 重要修正：不能用 malloc 分配缓存。**
复核发现：页表模式下用户进程的堆**只有 bss 最后一页可用**。
`library/elf/app.lds` 的 `.heap (NOLOAD)` 只放了一个
`__heap_start` 符号（大小为 0），不会扩大 PT_LOAD 段的 memsz；
`elf_load()` 只映射到 bss 末尾。因此 brk 一旦长出 bss 所在页，
`_sbrk` 返回的地址就是**未映射的虚拟地址**，一访问就 page fault
（现在 sys_file 里 mydisk/treedisk 的几次小 malloc 恰好落在 bss 同页内
才没出事——这本身也是个潜在坑，见"附带发现"）。

**修改建议（static BSS 缓存）**：BSS 属于 PT_LOAD 的 memsz，
elf_load 会为它分配并清零页面，天然安全：

```c
/* file0.c 顶部新增（file0.c 也会被 mkfs 在宿主机编译，static 无碍） */
#define FAT_CACHE_MAX   32   /* >= 2MB 磁盘的 F=(4096+127)/128 */
#define INODE_CACHE_MAX 2    /* >= NINODES=128 时的 I=2 */

static block_t fat_cache[FAT_CACHE_MAX];
static block_t inode_cache[INODE_CACHE_MAX];
static block_t sb_cache;
static uint cache_I, cache_F;
static int cache_ready = 0;

static void cache_init(inode_intf below) {
    if (cache_ready) return;
    below->read(below, 0, 0, &sb_cache);
    struct superblock* sb = (struct superblock*)sb_cache.bytes;
    if (sb->I > INODE_CACHE_MAX || sb->F > FAT_CACHE_MAX)
        FATAL("mydisk: cache too small for I=%d F=%d", sb->I, sb->F);
    cache_I = sb->I; cache_F = sb->F;
    for (uint b = 0; b < cache_I; b++) below->read(below, 0, 1 + b, &inode_cache[b]);
    for (uint b = 0; b < cache_F; b++) below->read(below, 0, 1 + cache_I + b, &fat_cache[b]);
    cache_ready = 1;   /* 一次性开销约 35 次块读，之后全部命中内存 */
}
```

四个辅助函数改走缓存（写操作仍写穿到盘，保证断电一致）：

```c
int get_fat(inode_intf below, uint below_ino, struct superblock* sb, uint k) {
    cache_init(below);
    return ((struct entry*)fat_cache[k / 128].bytes)[k % 128].next;
}
int set_fat(inode_intf below, uint below_ino, struct superblock* sb, uint k, int next) {
    cache_init(below);
    ((struct entry*)fat_cache[k / 128].bytes)[k % 128].next = next;
    return below->write(below, below_ino, 1 + cache_I + k / 128, &fat_cache[k / 128]);
}
/* get_inode/set_node 同理用 inode_cache（块号 1 + ino/64） */
```

配套改动：
- `mydisk_read/write/setsize` 里原来每次 `below->read(...,0,&sb_block)`
  读 superblock，改为 `cache_init(below)` 后用
  `struct superblock* sb = (struct superblock*)sb_cache.bytes;`；
  修改 free_list_head 后更新的是 sb_cache，并写穿
  `below->write(below,0,0,&sb_cache)`；
- **性能再进一步**：扩容/收缩循环里现在每块都写穿一次 sb，
  改成循环结束后只写穿一次 sb_cache（FAT 表项仍逐块写穿；
  sb 延迟到循环末尾，最坏情况崩溃时 free_list_head 与 FAT 暂时不一致，
  教学 OS 可接受，面试可以讲这个权衡）；
- `mydisk_create()` 是格式化路径（只被 mkfs 调用），保持直接写盘不动，
  缓存保持未加载（mkfs 里 create 后紧接着 mydisk_init + write，
  懒加载会读到 create 刚写好的元数据，一致）。

**预期收益**：ls/cat/cd/加载 app 的元数据磁盘 I/O 降低 90%+；
写大文件从 O(n²) I/O 降到 O(n)。sys_file.elf 的 BSS 增大约 17KB，
远小于 128KB 的 EGOS_BIN_MAX_NBYTE 限制，无风险。

**附带发现（建议记一笔，本轮不改）**：如上所述，页表模式下所有进程的
`malloc` 只能在 bss 最后一页内工作，再大就 page fault。现在没炸纯属
各 app 的 malloc 量小。彻底修法是让 `elf_load()` 为每个进程额外映射
一段固定堆，但会改变所有进程的内存布局，本轮不动——知道这个坑即可
（面试被问"你的 malloc 在虚拟内存下怎么工作"时是加分点）。

---

### B2. print.c 格式符缺失 + O(n²) ★★ —— 复核：按正确性改，且 v1 示例代码有 bug 已修正

**位置**：`library/libc/print.c:17-37` `format_to_str()`

**复核后的判断**：
1. **真正的问题是格式符缺失**：注释里留的作业（%c、%u、%p、%lld、
   %llu、%llx）未实现，遇到这些格式会**静默跳过**，是潜在输出 bug。
2. O(n²) 是真实的（strncat 每字符全串扫描），但**实际影响很小**：
   所有输出最终走 `uart_putc` 逐字符轮询发送，串口 I/O 比格式化慢几个
   数量级，格式化开销被完全淹没。所以这一项按正确性+代码质量改，
   不要当性能优化来讲。

**⚠️ v2 修正**：v1 给的示例代码里 `if (*fmt=='l') { fmt++; is_ll=1; ...}`
对 **%ld（单个 l）也按 64 位取参**，在 rv32 ilp32 下 `long` 只有 32 位，
`va_arg(args, longlong)` 会多吞 4 字节、带崩后面的参数。必须按 l 的
个数区分宽度：

```c
static char* fmt_u64(char* p, ulonglong v, uint base) {
    char tmp[24]; int n = 0;
    do tmp[n++] = "0123456789abcdef"[v % base]; while (v /= base);
    while (n) *p++ = tmp[--n];
    return p;
}

void format_to_str(char* out, const char* fmt, va_list args) {
    char* p = out;
    for (; *fmt; fmt++) {
        if (*fmt != '%') { *p++ = *fmt; continue; }
        fmt++;
        uint lcount = 0;
        while (*fmt == 'l') { lcount++; fmt++; }   /* %ld 按 32 位，%lld 按 64 位 */
        switch (*fmt) {
        case 's': { for (const char* s = va_arg(args, char*); *s;) *p++ = *s++; break; }
        case 'c': *p++ = (char)va_arg(args, int); break;
        case 'd': {
            long long v = (lcount >= 2) ? va_arg(args, long long)
                                        : (long long)va_arg(args, int);
            if (v < 0) { *p++ = '-'; v = -v; }
            p = fmt_u64(p, (ulonglong)v, 10); break;
        }
        case 'u': p = fmt_u64(p, (lcount >= 2) ? va_arg(args, ulonglong)
                                               : (ulonglong)va_arg(args, uint), 10); break;
        case 'x': p = fmt_u64(p, (lcount >= 2) ? va_arg(args, ulonglong)
                                               : (ulonglong)va_arg(args, uint), 16); break;
        case 'p': *p++ = '0'; *p++ = 'x';
                  p = fmt_u64(p, (ulonglong)(uint)va_arg(args, void*), 16); break;
        }
    }
    *p = 0;
}
```

注意：
- 这版去掉了 newlib `itoa` 依赖（itoa 不是标准 C，本身就是隐患）；
- LOG 宏的栈缓冲是 512 字节，现有调用都远小于此；面试若被问
  "printf 的缓冲区溢出风险"，答出这个约束即可，不必加边界检查。

---

### B3. proc_yield 中重复 MMIO 读 mtime ★ —— 复核：成立，一行改动

**位置**：`grass/kernel.c:126` 与 `L138-144`

**原因**：`proc_yield()` 开头已取 `now = mtime_get();`，睡眠检查循环里
对每个进程又调一次。mtime 是 MMIO，且实现为防进位要读两次高位。
每次上下文切换白做最多 17×2 次 MMIO 读。

**修改建议**：循环里复用 `now`：

```c
if (p->sleep_until > 0) {
    if (now < p->sleep_until) continue;
    else p->sleep_until = 0;
}
```

---

## C. 工程卫生 / 代码质量

### C1. 陈旧 web_server.elf 被打进磁盘镜像 ★★ —— 复核：确认

**现象**：`apps/user/` 下没有 web_server.c，但 `build/release/user/`
里残留 web_server.elf；`tools/mkfs.c:107` 会把该目录下**所有** .elf
打进 disk.img（当前镜像里确实有它）。

**后果**：镜像包含无法从源码复现的二进制；`make clean` 后重建会得到
不同的系统（命令列表少一个），项目不可复现。

**修改建议**：
1. `rm build/release/user/web_server.elf`，重新 `make install`；
2. 如果 web_server 是你想保留的功能，把源码补回仓库；
3. 可选加固：mkfs.c 里对每个 .elf 检查是否存在同名 .c，不存在就警告。

---

### C2. test_qemu.sh 是脆弱的 sed 补丁脚本 ★ —— 复核：确认，sed 已完全失效

**现象**：脚本用三条 sed 修改 `earth/boot.c` 再启动 QEMU。这些修改
早已合入 boot.c，sed 现在一条都匹配不上（静默空操作），但只要 boot.c
相关代码再变，脚本行为就不可预期。

**修改建议**：删除 sed 行，改成纯冒烟脚本：

```bash
#!/bin/bash
export PATH=$PATH:~/xpack-riscv-none-elf-gcc-14.2.0-3/bin:~/xpack-qemu-riscv-8.2.2-1/bin
make install || exit 1
(printf '0\r'; sleep 7
 printf 'coresinfo\r'; sleep 2
 printf 'loop 3 &\r'; sleep 1; printf 'loop 3 &\r'; sleep 3
 printf 'cat README\r'; sleep 3
 printf 'producer_consumer\r'; sleep 10
) | timeout 60 qemu-system-riscv32 \
    -M virt -smp 4 -m 8M -bios tools/egos.bin \
    -device VGA,addr=0x2 -serial stdio -display none \
    -drive if=pflash,format=raw,unit=1,file=tools/qemuROM.bin \
    -device sdhci-pci,addr=0x1 -device sd-card,drive=MMC \
    -drive if=none,file=tools/disk.img,format=raw,id=MMC
```

（注意输入必须用 `\r` 而不是 `\n`，egos 终端只认 0x0d 作为回车。）

---

### C3. sys_proc 中 shell_waiting 未初始化 ★ —— v2 新增

**位置**：`apps/system/sys_proc.c:34` `int sender, shell_waiting;`

**原因**：`shell_waiting` 只在 PROC_SPAWN 分支赋值，却在 PROC_EXIT
分支使用；它在 while(1) 循环体内声明，每次迭代都是"新"变量。
现在之所以"能用"，是依赖编译器复用同一栈槽、残留上一轮的值——
纯属巧合，是标准意义上的未定义行为。

**后果**：换编译器/优化等级可能行为突变（比如前台进程退出后
shell 收不到 reply，卡在 sys_recv）。

**修改建议**：`int sender, shell_waiting = 0;`（一行）。

---

## D. 澄清：这些现象不是 bug（别在面试中说错）

1. **"4 个核只有 1 个在跑进程"**：负载特性，不是调度 bug。
   稳态下三个系统服务都阻塞在 sys_recv（PROC_PENDING_SYSCALL），
   只有 shell 是 RUNNABLE。调度扫描正确排除了已在别的核上 RUNNING
   的进程，kernel_lock 保证决策串行化，不会双调度。
   想展示多核利用率：先 `loop &` 起两三个后台循环再敲 `coresinfo`。
2. **`proc_try_send` 在接收方没准备好时直接 return**：不是消息丢失。
   发送方保持 PROC_PENDING_SYSCALL，`proc_yield` 扫描每轮对 PENDING
   进程重试 `proc_try_syscall`，本质是 "rendezvous + 重试"。
3. **`-smp 1` 启动卡死**：设计使然，sys_proc 自旋等待
   `booted_core_cnt == NCORES(4)`。
4. **producer_consumer 里线程不能 exit**：线程与主线程共享 pid，
   exit 会杀掉同一 pid 的所有 PCB——已知简化，代码注释已说明。

（v1 中"mlfq_update_level 行为恰好正确"的说法**撤回**，实际是 bug，
已移入 A6。）

