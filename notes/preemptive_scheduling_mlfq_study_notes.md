# 抢占式调度与多级反馈队列（P2 MLFQ 实验）全景完整复习笔记

> **目标**：本文档结合 `egos-2000` 官方实验指南 `p2_mlfq.md` 的完整理论体系与代码架构，并深度融入你在授课与探索过程中讨论的所有微观硬件、CSR 控制与状态寄存器、汇编栈保护机制以及高质量深度提问。按知识演进与系统设计逻辑整理，确保复习时透彻理解每一个底层细节。

---

## 第一章：抢占式调度与硬件中断基础理论

### 1.1 合作式 (Cooperative) 与 抢占式 (Preemptive) 调度的本质区别
- **P1 合作式调度**：进程/线程必须主动调用 `thread_yield()` 交出 CPU。若某程序陷入死循环（如 `while(1);`），整个操作系统将被死死卡死。
- **P2 抢占式调度**：依赖硬件**定时器中断（Timer Interrupt）**。即使当前进程在一个死循环中运行，硬件定时器到达指定时间后，CPU 硬件电路也会强行剥夺当前进程的 CPU 执行权，将控制权强行交给内核处理例程。

### 1.2 RISC-V 硬件定时器机制 (`mtime` 与 `mtimecmp`)
芯片内部通过两个 64 位的内存映射寄存器（MMIO）来实现硬件时钟：
- **`mtime`**：自 CPU 上电后由硬件自动以固定频率单调递增的 64 位计数器（像墙上的挂钟）。
- **`mtimecmp`**：定时器比较寄存器（像闹钟提醒时间）。
- **硬件触发条件**：当 `mtime >= mtimecmp` 时，硬件电路自动拉高定时器中断信号。

在 `earth/cpu_intr.c` 中的读写与防并发设计：
```c
/* 读 64 位 mtime 寄存器：采用 do-while 循环防止读取 low 时 high 刚好发生进位溢出 */
ulonglong mtime_get() {
    uint low, high;
    do {
        high = REGW(MTIME_BASE, 4);
        low  = REGW(MTIME_BASE, 0);
    } while (REGW(MTIME_BASE, 4) != high);
    return (((ulonglong)high) << 32) | low;
}

/* 写 64 位 mtimecmp 寄存器：先写高 32 位为 0xFFFFFFFF，防止写低 32 位时误触发中断 */
static void mtimecmp_set(ulonglong time, uint core_id) {
    REGW(MTIMECMP_BASE, core_id * 8 + 4) = 0xFFFFFFFF;
    REGW(MTIMECMP_BASE, core_id * 8 + 0) = (uint)time;
    REGW(MTIMECMP_BASE, core_id * 8 + 4) = (uint)(time >> 32);
}
```

---

## 第二章：控制与状态寄存器 (CSR) 与中断使能

### 2.1 关键 CSR 寄存器功能汇总表

| CSR 寄存器 | 全称 | 在 P2 抢占式调度中的物理作用 |
| :--- | :--- | :--- |
| **`mtvec`** | Machine Trap-Vector Base-Address | 存储中断/异常发生时，CPU 硬件强行跳转的入口地址（`trap_entry`） |
| **`mepc`** | Machine Exception Program Counter | 中断触发时，硬件自动将被打断的程序断点（原 `PC`）备份保存于此 |
| **`mscratch`**| Machine Scratch Register | 专门用于中断刚触发、通用寄存器未保存前，临时暂存进程原始栈指针 `sp` |
| **`mcause`** | Machine Cause Register | 记录引发 Trap 的原因（最高位 `bit 31` 为 1 代表中断，为 0 代表异常；值为 7 代表定时器中断） |
| **`mstatus`**| Machine Status Register | 控制 CPU 状态，`bit 3`（`MIE`）控制全局中断使能 |
| **`mie`** | Machine Interrupt Enable Register | 控制特定中断源使能，`bit 7`（`MTIE`）控制定时器中断使能 |

### 2.2 深度问题解析：为什么需要 `mstatus` 与 `mie` 双层中断开关？
- **生活大白话比喻**：`mstatus` 的 `MIE`（Bit 3，掩码 `0x8`）是**全家总电闸**；`mie` 的 `MTIE`（Bit 7，掩码 `0x80`）是**卧室定时器灯的分开关**。
- **物理必要条件**：只有当 **【总电闸 = 1】 AND 【分开关 = 1】** 时，定时器到期时 CPU 硬件才会响应中断。
- **汇编指令 `csrs` 逻辑**：`csrs mie, 0x80` 与 `csrs mstatus, 0x8` 使用按位或（Bitwise OR）操作，只把第 7 位和第 3 位拉高成 1，保持其他位不变。

---

## 第三章：上下文保存与“安全中转站”内核栈

### 3.1 汇编入口 `trap_entry` 物理过程 ([grass/kernel.s](file:///home/shenmi/egos-2000/grass/kernel.s#L12))

```assembly
trap_entry:
    csrw mscratch, sp           # 1. 借助 mscratch 暂存进程原始 sp
    li sp, 0x80200000           # 2. 强制将 sp 切换为操作系统内核栈顶 (0x80200000)

    addi sp, sp, -128           # 3. 栈顶下移 128 字节 (存放 32 个 32位通用寄存器)
    sw a0,  0(sp)
    sw a1,  4(sp)
    ...
    sw tp,  116(sp)
    csrr t0, mscratch           # 4. 从 mscratch 读出原始 sp
    sw t0,  120(sp)             # 5. 将进程原始 sp 保存到栈 120(sp) 处

    call kernel_entry           # 6. 进入 C 语言内核处理

    lw a0,  0(sp)               # 7. 从栈上恢复下一个进程的寄存器
    ...
    lw sp,  120(sp)             # 8. 恢复下一个进程的 sp

    mret                        # 9. 中断返回
```

### 3.2 深度问题解析：为什么不直接把通用寄存器存入 PCB，而必须先存入内核栈 `0x80200000`？

这是计算机体系结构最本质的问题之一：
1. **纯汇编在中断瞬间算不出 PCB 的动态物理地址**：
   在 C 语言中算 PCB 地址的公式是 `&proc_set[curr_proc_idx].saved_registers`。在汇编里要算这个地址，必须借用 2~3 个通用寄存器（如 `t0`, `t1`）。但此时所有通用寄存器里都装着用户进程正在用的重要数据！**只要动用任何一个通用寄存器去算地址，那个寄存器里的原始数据就会被瞬间冲掉破坏！**
2. **内核栈地址是固定的硬编码常数 (`0x80200000`)**：
   借助 `mscratch` 藏好 `sp` 后，直接 `li sp, 0x80200000` 压栈，不需要破坏任何通用寄存器。
3. **C 语言运行本身必须依赖安全栈**：
   C 语言函数编译后局部变量和返回地址 `ra` 必须压栈。强制切到内核栈 `0x80200000` 避免了用户进程栈溢出或野指针导致内核崩溃。
- **结论**：**内核栈在此扮演了“安全中转站”的角色！**

### 3.3 深度问题解析：中断上下文切换的“偷梁换柱”全过程
在 `kernel_entry()` 中，进程切换的数据流向如下：
```text
【中断发生，进入 kernel_entry()】 (当前 curr_proc_idx = 1，进程 A)
1. 读 mepc 保存到 proc_set[1].mepc
2. 将内核栈 128 字节的数据 memcpy 备份到 proc_set[1].saved_registers

【调用 proc_yield()】
3. 调度器挑选进程 B，将全局指针修改为 curr_proc_idx = 2！

【离开 kernel_entry() 前】 (此时 curr_proc_idx = 2，进程 B)
4. 将 proc_set[2].mepc 写回 CSR mepc
5. 将 proc_set[2].saved_registers 用 memcpy 覆盖覆写回内核栈！

【返回 trap_entry 汇编】
6. 汇编弹栈，弹出的全是进程 B 的寄存器！最后执行 mret 跳去进程 B 的 mepc 运行。
```

### 3.4 深度问题解析：为什么不能用普通的 `return` / `ret`，而必须用 `mret`？
- 普通 `ret` 寻找通用寄存器 `ra` 作为返回地址。但在中断发生时，CPU 是被硬件强行劫持的，并没有发生普通函数调用，`ra` 里不是被打断的位置。
- 硬件打断时自动把断点保存在了 CSR **`mepc`** 中。
- **`mret` 指令的物理动作**：
  1. 将 `mepc` 的值重新装载回 CPU 的 `PC`；
  2. 硬件自动把全局中断开关重新打开（恢复 `mstatus.MIE`）。

---

## 第四章：进程生命周期统计采集 (实验任务 1)

### 4.1 4 大统计指标定义与测量方法

1. **周转时间 (Turnaround Time)**：进程创建 (`proc_alloc`) 到终止 (`proc_free`) 的总时间。
   $$\text{turnaround} = \text{mtime\_get}() - \text{p.start\_time}$$
2. **响应时间 (Response Time)**：进程创建 (`proc_alloc`) 到第一次被调度运行 (`proc_yield`) 的等待时间。
   $$\text{response} = \text{p.first\_time} - \text{p.start\_time}$$
3. **CPU 运行时间 (CPU Time)**：进程处于 `PROC_RUNNING` 实际在 CPU 上执行的累计时间。
   在 `proc_yield()` 中切出时累加：`p.cpu_time += (now - p.last_run_time)`。
4. **定时器中断次数 (Timer Interrupts Count)**：在 `intr_entry()` 中遇到 `id == INTR_ID_TIMER` 时让当前进程 `++proc_set[curr_proc_idx].time_intr_time`。

> **单位转换与强转细节**：$\text{毫秒 (ms)} = \frac{\Delta \text{mtime}}{10000}$。在传给 `INFO` 的 `%d` 时，必须写成 `(uint)(turnaround / 10000)`，显式强转为 32 位整数，防止 64 位 `ulonglong` 导致变长参数错位打印负数乱码。

### 4.2 深度问题解析：`loop 500` 与 `loop 500 silent` 的巨大数据差异

在实测中，两者的统计数据呈现巨大对比：
- `loop 500`（带打印）：中断 **0 次**，周转时间 **1980ms**。
- `loop 500 silent`（静默）：中断 **83 次**，周转时间 **878ms**。

**原因剖析**：
1. **I/O 密集型 vs CPU 密集型**：`loop 500` 每次循环调用 `printf` 系统调用（I/O 操作），在 1 秒定时器响前自己就频繁主动交出了 CPU，因此遭遇定时器抢占中断次数为 0。`loop 500 silent` 是纯死循环加法计算，绝对不主动让出 CPU，系统全靠定时器强行剥夺打断它 83 次！
2. **周转时间差异**：控制台屏幕显示（串口通信）比 CPU 运算慢上千倍，`loop 500` 大量时间卡在等待屏幕刷新上，因此总时间长得多。

---

## 第五章：多级反馈队列 (MLFQ) 调度器 (实验任务 2)

### 5.1 MLFQ 5 层队列架构与规则

```text
Level #0 [最高优先级] ──► 时间片 100ms (用完降级到 Level 1)
Level #1 [中高优先级] ──► 时间片 200ms (用完降级到 Level 2)
Level #2 [中优先级]   ──► 时间片 300ms (用完降级到 Level 3)
Level #3 [中低优先级] ──► 时间片 400ms (用完降级到 Level 4)
Level #4 [最低优先级] ──► 时间片 500ms (留在 Level 4 持续轮转)
```

1. **最高优先级优先**：`proc_yield()` 扫描进程表时，永远挑选 `PROC_READY` / `PROC_RUNNABLE` 且 **`p->level` 最小** 的进程。
2. **Periodic Boost**：每隔 10 秒（`MLFQ_RESET_PERIOD`），将所有进程的 `level` 重置为 0，防止低优先级进程“饿死”。
3. **Shell 交互加速**：检测到键盘有新的输入（`!earth->tty_input_empty()`），立刻将 Shell 进程（`GPID_SHELL`）提升回 Level 0。

### 5.2 核心函数实现代码总结

#### ① `mlfq_update_level()` ([grass/process.c](file:///home/shenmi/egos-2000/grass/process.c#L87))
```c
void mlfq_update_level(struct process* p, ulonglong runtime) {
    /* 内核系统服务进程 (pid < GPID_USER_START) 不参与降级，永远保持 Level 0 */
    if (p->pid < GPID_USER_START) return;

    if ((p->level_time_left -= runtime) <= 0) {
        if (p->level < MLFQ_NLEVELS - 1)
            ++p->level;
        p->level_time_left = MLFQ_LEVEL_RUNTIME(p->level); // 重置新层级时间片
    }
}
```

#### ② `mlfq_reset_level()` ([grass/process.c](file:///home/shenmi/egos-2000/grass/process.c#L100))
```c
void mlfq_reset_level() {
    /* 1. 键盘按键直通车：提升 Shell 到 Level 0 */
    if (!earth->tty_input_empty()) {
        for (int i = 0; i < MAX_NPROCESS; i++) {
            if (proc_set[i].pid == GPID_SHELL && proc_set[i].status != PROC_UNUSED) {
                proc_set[i].level = 0;
                proc_set[i].level_time_left = MLFQ_LEVEL_RUNTIME(0);
                break;
            }
        }
    }

    /* 2. 定时 10 秒全量 Boost */
    static ulonglong MLFQ_last_reset_time = 0;
    if (mtime_get() - MLFQ_last_reset_time >= MLFQ_RESET_PERIOD) {
        for (int i = 0; i < MAX_NPROCESS; i++) {
            proc_set[i].level = 0;
            proc_set[i].level_time_left = MLFQ_LEVEL_RUNTIME(0);
        }
        MLFQ_last_reset_time = mtime_get();
    }
}
```

#### ③ `proc_yield()` 挑选最高优先级 ([grass/kernel.c](file:///home/shenmi/egos-2000/grass/kernel.c#L102))
```c
static void proc_yield() {
    ...
    ulonglong now = mtime_get();
    ulonglong runtime = now - proc_set[curr_proc_idx].last_run_time;
    proc_set[curr_proc_idx].cpu_time += runtime;
    mlfq_update_level(&proc_set[curr_proc_idx], runtime);
    mlfq_reset_level();

    int next_idx = MAX_NPROCESS;
    int min_level = MLFQ_NLEVELS; // 初始设为 5，确保 Level 0~4 均可被正常选中

    for (uint i = 1; i <= MAX_NPROCESS; i++) {
        struct process* p = &proc_set[(curr_proc_idx + i) % MAX_NPROCESS];
        if (p->status == PROC_PENDING_SYSCALL) proc_try_syscall(p);

        if ((p->status == PROC_READY || p->status == PROC_RUNNABLE) && (p->level < min_level)) {
            next_idx = (curr_proc_idx + i) % MAX_NPROCESS;
            min_level = p->level;
        }
    }
    ...
}
```

---

## 第六章：深度实测问题与操作系统架构思考

### 6.1 深度实测对比：MLFQ 带来的巨大调度优势
- **实测数据对比**：
  - **不用 MLFQ 时（默认轮转调度器）**：后台有死循环 `loop &` 运行时，第一个 `ls` 的周转时间高达 **660ms**（响应时间 594ms）。
  - **使用了 MLFQ 调度器后**：后台死循环被迅速打入 Level 4 低优先级，前台 `ls` 在 Level 0 最高优先级运行，`ls` 的周转时间降到了 **480ms**（响应时间 415ms，无论是否加上系统服务保护，MLFQ 均能稳定锁定在 480ms）！
- **结论**：实测证明，MLFQ 调度器将后台死循环隔离开来，直接为前台交互任务节省了近 **200ms** 的响应延迟！

### 6.2 深度问题解析：为什么后台有 `loop` 时 `ls` 是 480ms，而 `killall` 后变 23ms？
- 在 QEMU 模拟器中，`mtime` 时间戳是随着 CPU 跑过的指令条数累加的。
- 当后台有 2 个死循环 `loop` 时，`ls` 在等待磁盘数据装载的阻塞期间，CPU 并没有闲着，而是在全力奔跑那 2 个 `loop` 的指令。这期间流逝的物理时间（约 450ms）被算进了 `ls` 的总周转时间中（因此稳定为 480ms）。
- `killall` 杀掉后台死循环后，等待期间没有其他程序抢跑指令，时间便降到了纯粹的 **23ms**。

---

## 第七章：常用命令与编译指令查表

| 操作 | Shell / Bash 命令 | 说明 |
| :--- | :--- | :--- |
| **配置工具链路径** | `export PATH=/home/shenmi/xpack-riscv-none-elf-gcc-14.2.0-3/bin:/home/shenmi/xpack-qemu-riscv-8.2.2-1/bin:/usr/bin:/bin:$PATH` | 添加 RISC-V GCC 编译器与 QEMU 路径 |
| **全量编译并启动** | `make clean && make && make qemu` | 清理、编译全量用户程序并启动 QEMU 模拟器 |
| **退出 QEMU 模拟器** | 按快捷键 **`Ctrl + A`** 紧接着按 **`X`** | 退出 QEMU 返回 Linux 终端 |
| **后台运行测试** | `loop &` | 在 Shell 中将 `loop` 放入后台运行 |
| **杀死所有用户进程** | `killall` | 批量杀死所有 PID `>= GPID_USER_START` 的用户进程 |
