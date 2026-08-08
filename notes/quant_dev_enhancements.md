# egos-2000 面向 Quant Dev 求职的增强方案

> 目标：在不破坏教学 OS 结构的前提下，增加 4~5 个"量化味"浓的功能/基准测试，
> 让简历和面试有具体的**数字**和**权衡讨论**可讲。
>
> Quant dev 面试官真正考察的点，和本方案每项的对应关系：
>
> | Quant dev 考察点 | 对应增强项 |
> |---|---|
> | 测量纪律（没有数字就没有优化） | P1 测量基础设施 + P6 性能报告 |
> | 陷阱/内核路径开销意识 | P2 微基准（ecall、IPC、上下文切换） |
> | 无锁编程、C/C++ 内存模型、cache line | P3 SPSC 无锁环 + false sharing 演示 |
> | 锁的可扩展性、总线争用 | P4 TTAS 内核锁 + 锁扩展性基准 |
> | kernel bypass、轮询 vs 中断、确定性延迟 | P5 网卡 TX 环吞吐基准 + busy-poll idle |

**重要前提（测量有效性）**：QEMU 不是周期精确模拟器，`rdcycle` 在 QEMU 上
反映的是宿主时间/指令数，不是真实硅片周期。所有数字应表述为
"QEMU 上的相对比较"；真实数字要在项目支持的 FPGA 板（TangNano/Arty）上跑。
**面试时主动说出这个 caveat 本身就是加分项**——说明你懂测量。

**好消息**：`apps/user/` 下新增 .c 文件会被 Makefile 的 wildcard 自动编译打包，
加 benchmark app 不需要动构建系统。

---

## P1. 测量基础设施：cycle 计数器 + 内核统计计数器（先做这个）

这是所有后续项的地基，也让 `notes/optimization_review.md` 里的 B1（FAT 缓存）
优化有了**可量化的 before/after**。

### P1a. 让用户态能读硬件周期计数器

RISC-V 的 `rdcycle` 在 U-mode 下默认不可用（`mcounteren` 默认全 0，
读了会触发 illegal instruction——这本身就是个可以讲的知识点）。

**改动**（earth/cpu_intr.c `intr_init()` 里加一行）：

```c
/* 允许 U-mode 读 cycle/instret 计数器，供 benchmark app 使用 */
asm("csrs mcounteren, %0" ::"r"(0x5)); /* bit0=CY, bit2=IR */
```

**用户态读数工具**（放 library/libc 或直接写在 bench app 里）：

```c
static inline uint cycles32() {
    uint c; asm volatile("rdcycle %0" : "=r"(c)); return c;
}
```

### P1b. 内核统计计数器 + `stats` shell 命令

**改动**：grass 层加几个全局计数器，在对应路径 ++：

```c
/* grass/kernel.c 或 process.c */
uint stat_traps, stat_ctx_switches, stat_ecalls;
uint stat_disk_reads, stat_disk_writes;   /* 单位：块 */
```

- `kernel_entry()`：stat_traps++；ecall 分支：stat_ecalls++；
- `proc_yield()` 切换进程处：stat_ctx_switches++；
- `earth/dev_disk.c` 的 `disk_read/disk_write`：按 nblocks 累加。

然后在 `sys_shell.c` 里加一个和 `coresinfo` 平级的分支：

```c
} else if (strcmp(buf, "stats") == 0) {
    /* 通过 grass 接口暴露计数器，打印 traps/ctx_switches/ecalls/disk 块数 */
}
```

（需要在 struct grass 里加一个 `void (*proc_stats)();` 函数指针，
照抄 proc_coresinfo 的接法即可。）

### 价值与面试话术

- 做完 B1（FAT 缓存）后：`stats` → `cat README` → `stats`，
  前后磁盘读块数对比就是你的**量化收益**（预期从几十块降到 0~1）。
- 话术："I instrumented the kernel with per-event counters, then measured
  the filesystem optimization: metadata disk I/O dropped from ~40 blocks to 0
  per file read."——量化公司最吃这套。

**工作量**：半天。**风险**：极低（纯计数）。

---

## P2. 微基准套件：量化你的内核路径（trap / ecall / IPC / 调度唤醒）

Quant dev 日常就是问"这条路径多少 ns"。给你的 OS 建立一套答案。

### P2a. 新增 SYS_GETPID 系统调用（约 15 行，一石二鸟）

现在用户进程不知道自己的 pid，无法点对点通信，也没法测 ecall 开销。
在 `library/syscall/syscall.h` 加 `SYS_GETPID = 4`，kernel.c 的
`proc_try_syscall()` 加一个 case：

```c
case SYS_GETPID:
    proc->syscall.content[0] = proc->pid & 0xFF;   /* pid 很小，1 字节够 */
    proc->syscall.content[1] = (proc->pid >> 8) & 0xFF;
    proc->syscall.status = DONE;
    proc->status = PROC_RUNNABLE;
    break;
```

library 侧：

```c
int getpid() { sc->type = SYS_GETPID; asm("ecall"); return sc->content[0] | (sc->content[1] << 8); }
```

### P2b. bench app：四个微基准（新建 apps/user/bench.c）

```c
uint bench_ecall() {          /* ecall 往返开销 */
    uint c0 = cycles32();
    for (int i = 0; i < 1000; i++) getpid();
    return (cycles32() - c0) / 1000;
}
```

1. **ecall 开销**：如上，测 trap 进入 + 内核分发 + 返回的周期数；
2. **IPC ping-pong**：父进程 spawn 子进程（argv 传入父 pid），
   双方用 sys_send/sys_recv 互相打 1000 个来回，测每跳周期数
   ——这量化了你的调度器 + rendezvous IPC 的总开销；
3. **调度唤醒延迟**：`sleep(1)` 后用 mtime 测实际睡了多久，
   重复 100 次输出 min/max/分布——你会直观看到 QUANTUM=10ms 的
   时间片粒度造成的唤醒抖动（这是引出 P5 busy-poll 的伏笔）；
4. **上下文切换**：后台跑两个 `loop &` 制造抢占，前台进程记录
   被抢占前后 cycles32() 的差值分布。

输出格式建议直接给 p50/p99/max（排序后取分位即可），
而不是平均数——**quant 公司只关心尾部延迟**，这个细节会被注意到。

### 面试话术

- "我的 OS 一次 syscall 往返是 X 千周期，其中 trap 保存恢复占 Y——
  因为每次 trap 都要 memcpy 128 字节上下文，这是教学 OS 的简化，
  Linux 用 per-task 内核栈避免这次拷贝。"
- "唤醒延迟的 p99 是一个 QUANTUM，因为睡眠进程要等下一个时钟中断
  才被检查——Linux 用 hrtimer + 定时唤醒队列解决。"

**工作量**：1~2 天。**风险**：低。SYS_GETPID 是纯增量 syscall，
不影响现有逻辑。

---

## P3. 无锁 SPSC 环形队列 + false sharing 演示（最核心的 C++ 考点）

量化岗 C++ 面试几乎必考：acquire/release 语义、cache line、false sharing、
为什么 lock-free 队列需要 power-of-two 容量。你的项目现在只有一个
"纯自旋 flag"版的 producer_consumer（apps/user/producer_consumer.c），
正好升级它。

### 现成的基础设施

`earth/cpu_mmu.c` 的 `page_table_map()` 为**每个**用户进程恒等映射了
`SHELL_WORK_DIR` 页（代码注释里写了"实现共享内存"），
`apps/app.h` 的 workdir 宏就在用。**不同进程天然共享这一页物理内存**
——你的跨进程无锁队列/锁测试不用做任何内核改动，直接用这页的
空闲区域（偏移 3072 起，避开 workdir 字符串和 udp_demo 用的 1024/2048）。

### P3a. SPSC 无锁环（新建 apps/user/spsc_bench.c）

```c
#define RING_SZ 64   /* 2 的幂，index 用 & (RING_SZ-1) 取模——面试必问为什么 */
struct spsc {
    volatile uint head __attribute__((aligned(64)));  /* 消费者写 */
    volatile uint tail __attribute__((aligned(64)));  /* 生产者写 */
    int items[RING_SZ];
};
/* 放在 SHELL_WORK_DIR + 3072 的共享页上 */

void push(struct spsc* q, int v) {
    uint t = q->tail;
    if (t - q->head >= RING_SZ) return;            /* 满 */
    q->items[t & (RING_SZ - 1)] = v;
    __atomic_store_n(&q->tail, t + 1, __ATOMIC_RELEASE);  /* 先写数据再发布 tail */
}
int pop(struct spsc* q, int* out) {
    uint h = q->head;
    if (h == q->tail) return 0;                     /* 空 */
    *out = q->items[h & (RING_SZ - 1)];
    __atomic_store_n(&q->head, h + 1, __ATOMIC_RELEASE);
    return 1;
}
```

父进程当生产者、spawn 的子进程当消费者（子进程 pid 用 P2 的 getpid 拿不到
父 pid 的话，把"角色"写进共享页让先到的当生产者），跑 100 万次，
用 P1 的 rdcycle 测吞吐；再给一个用 acquire(lock) 保护的对照版本，
对比两者。

### P3b. false sharing 演示（同一个 app 里加一个模式）

两个后台进程各自 ++ 一个计数器 100 万次：
- 模式 A：两个计数器相邻（同一 cache line）；
- 模式 B：两个计数器相隔 64 字节。
对比耗时。QEMU 上差异可能不明显（模拟器不精确建模缓存协议），
**但代码和原理是对的**，在 FPGA 板上会有真实差异；面试时讲原理即可：
MESI 协议下两个核交替写同一 line 会不断 invalidate，line 在两个 L1 间
来回弹（ping-pong），吞吐掉几倍。

### 面试话术（这项能聊 20 分钟）

- "为什么 tail 用 release store？因为要保证 items[] 的写对消费者可见，
  release 之前的写不会被重排到它之后；消费者用 acquire load 配对。"
- "为什么 SPSC 不需要 CAS？因为单写者，普通 store 配合 release 就够。"
- "为什么容量是 2 的幂？取模变位与；且 head/tail 用无符号回绕差值判空满，
  避免 ABA 和 full/empty 歧义。"
- "生产者的 head 读可以用 relaxed，因为读到旧值只会多自旋一圈，不影响正确性。"

**工作量**：1~2 天。**风险**：低（纯新增 app）。
**注意**：线程版（thread_create）共享同一地址空间和 pid，也可以在同一
进程内做生产者/消费者，但跨进程版本更能展示你对共享内存 + 内存序的理解。

---

## P4. 内核锁升级 TTAS + 锁扩展性基准

现在的 `grass/kernel.s` 取锁是裸 `amoswap.w.aq` 自旋：N 个核同时抢锁时，
每个自旋核的 amoswap 都会独占锁所在的 cache line，把其他核的副本全部
invalidate——经典 lock contention 风暴。这是教科书级的优化点。

### 改动

kernel.s Step1 改成 **test-and-test-and-set（TTAS）**：先普通 load 观察，
只在看到锁空闲时才发 amoswap：

```asm
    csrw sscratch, t0
    csrw mscratch, t1
    la   t0, kernel_lock
.Lttas_spin:
    lw   t1, 0(t0)          /* 只读观察，命中本核 L1 缓存，不打扰别人 */
    bnez t1, .Lttas_spin
    li   t1, 1
    amoswap.w.aq t1, t1, (t0)
    bnez t1, .Lttas_spin
    csrr t1, mscratch
    csrr t0, sscratch
```

（Step7 释放可以顺手换成 `amoswap.w.rl zero, zero, (t0)`，
对应 optimization_review.md 的 A5 进阶项。）

### 配套基准（利用 P1 的共享内存页）

写一个 `lock_bench` app：N 个后台进程同时对共享页上的一个普通 int 锁
（用 __sync_lock_test_and_set 实现）做 100 万次 lock/unlock，
对比三种实现的总耗时：
1. 裸 test-and-set（现状）；
2. TTAS；
3. （进阶）ticket lock——保证公平，quant 代码库里常见。

再配 P3b 的思路说明锁变量独占 cache line 的必要性。

### 面试话术

- "我的第一版内核锁是 amoswap 自旋，N 核竞争时每次自旋都是一次
  跨核 cache line 独占请求；换成 TTAS 后自旋路径只读本地 L1，
  竞争吞吐提升 X 倍（QEMU 相对值）。真实系统里更进一步是
  MCS/CLH 队列锁，把排队分散到各自的 cache line。"
- 可以主动提 Linux 的 qspinlock 就是这条演进路线的终点。

**工作量**：1 天。**风险**：中低——kernel.s 是热路径，改完必须跑全量
冒烟测试（尤其多核 4 个后台 loop + producer_consumer）。

---

## P5. 网络：TX 描述符环吞吐基准 + busy-poll idle（HFT 核心话题）

### P5a. e1000 TX 环吞吐基准（升级 apps/user/udp_demo.c 或新建 net_bench.c）

现在 udp_demo 每次只发 1 个包、用 1 个 TX 描述符、发完忙等 DD 位。
改成真正的**描述符环 + 批量发送**：

- TDLEN 已经是 128（8 个描述符），把 8 个描述符都用起来：
  预先填好 8 个描述符（共享页上 8×16 字节），一次写 TDT=8 触发批量发送；
- 发 1000 个包，用 rdcycle 测**每包周期数**和总吞吐；
- 对照：逐个发送+逐个等 DD（现状）vs 批量 8 个。

```c
/* 伪代码骨架：描述符布局参考现有 udp_demo 的单描述符写法 */
for (int i = 0; i < 8; i++) {
    fill_desc(&descs[i], &frames[i], len, CMD_EOP | CMD_RS);
}
REGW(ETH_CTL_BASE, 0x3818) = 8;              /* 一次推 TDT 到 8 */
while (!(descs[7].status & DD));             /* 只等最后一个 */
```

### P5b. 中断合并（RX 侧，一行配置 + 一段解释）

`earth/boot.c` 现在 `REGW(ETH_CTL_BASE, 0xD0) = (1 << 7);`（RXT0 定时器）。
把这个值做成可配置，跑 P5a 时对比不同合并间隔下的中断次数
（用 P1 的 stat_traps 计数！）——**中断合并 vs 延迟**正是 HFT 网络
调优的经典权衡，你用自己 OS 的真实数据讲这个故事。

### P5c. busy-poll idle（可选，进阶）

HFT 系统用 100% CPU 换确定性延迟。给 `grass/kernel.c` 的 idle 路径
（wfi 那段）加一个编译期开关：

```c
#ifdef IDLE_BUSY_POLL
    /* 不 wfi，自旋扫描有没有 RUNNABLE 进程；有就立刻调度 */
    while (1) { /* 扫描 run queue，发现可运行的就 break 去调度 */ }
#else
    earth->timer_reset(core_in_kernel);
    release(kernel_lock);
    asm("csrs mstatus, %0" ::"r"(1 << 3));
    asm("wfi");
#endif
```

配合 P2b 的"调度唤醒延迟"基准：wfi 模式下唤醒延迟最高一个 QUANTUM（10ms），
busy-poll 模式下接近 0——**用你自己的 p50/p99 数据讲
"确定性延迟 vs CPU 成本"的权衡**。

### 面试话术

- "我的网卡驱动支持中断和轮询两种收包路径，测得中断模式唤醒延迟
  p99 是 X，轮询模式是 Y，代价是空转核 100% 占用——这和 HFT 里
  DPDK/kernel bypass + pinned busy-poll core 的取舍是一样的。"
- "TX 用描述符环批量提交而不是每包等 DD，本质是把 doorbell 写（MMIO，
  贵）从每包一次摊薄到每批一次。"

**工作量**：P5a+P5b 约 1~2 天；P5c 加半天。
**风险**：低（P5a/P5c 都是增量；P5b 只是参数化现有寄存器写）。

---

## P6. 把所有数字写进仓库：PERFORMANCE.md

量化公司看简历 10 秒钟，**带数字的项目描述 >> 形容词**。
在项目根目录加 `PERFORMANCE.md`（或并入 README），包含：

1. 测量平台（QEMU -smp 4 / FPGA 板）、方法（rdcycle、样本数、分位数）；
2. 一张结果表：ecall 往返、IPC 每跳、上下文切换、锁吞吐（3 种实现）、
   网卡每包周期、唤醒延迟 p50/p99（wfi vs busy-poll）；
3. FAT 缓存优化的 before/after（磁盘块计数，来自 P1b 的 stats）；
4. 明确标注"QEMU 为功能正确性与相对比较，绝对值以 FPGA 板为准"。

简历上的对应 bullet（示例，数字以你实测为准）：

- "Built a cycle-accurate micro-benchmark suite over a teaching RISC-V OS
  (trap/syscall/context-switch/IPC latency, p50/p99 histograms via rdcycle)"
- "Implemented a lock-free SPSC ring over shared memory with acquire/release
  semantics; measured N× throughput vs spinlock on 4 cores"
- "Replaced kernel's test-and-set spinlock with TTAS; reduced lock
  contention overhead by X% under 4-core load"
- "Profiled and cached the FAT filesystem metadata, cutting per-read disk
  I/O from ~40 blocks to 0 (verified via kernel I/O counters)"
- "Benchmarked interrupt vs busy-poll idle: wakeup latency p99 Xms → Y cycles"

---

## 建议实施顺序与工作量汇总

| 顺序 | 项目 | 工作量 | 与 optimization_review.md 的关系 |
|---|---|---|---|
| 1 | P1 测量基础设施 | 0.5 天 | 是验证 A/B/C 所有修复的地基 |
| 2 | 先做 review 里的 A1/A6/A2 等修复 | 1~2 天 | 正确性优先于锦上添花 |
| 3 | P2 微基准 + SYS_GETPID | 1~2 天 | — |
| 4 | P3 SPSC + false sharing | 1~2 天 | — |
| 5 | review 里的 B1 FAT 缓存 | 1 天 | 用 P1 的 stats 量化收益 |
| 6 | P4 TTAS 锁 + 锁基准 | 1 天 | 覆盖 review 的 A5 进阶项 |
| 7 | P5 网络基准（a/b 必做，c 选做） | 1~2 天 | 依赖 review 的 A1 先恢复 RX 中断 |
| 8 | P6 PERFORMANCE.md | 0.5 天 | 收尾，把所有数字沉淀成简历素材 |

总计约 7~10 个工作日，产出 5 组可上简历的量化结果。

## 每项改动前的保底验证

每完成一项：`make clean && make install` + optimization_review.md E 节的
冒烟测试清单（coresinfo/ls/cat/cd/echo/loop&/producer_consumer/sleep/
crash1/killall），确保没有把现有功能改坏——**功能回归是底线，
面试官问"你怎么保证改动没破坏别的"时，"我有一套冒烟测试"就是答案**。
