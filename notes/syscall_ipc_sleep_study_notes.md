# 系统调用与进程间通信 (P3 System Call & IPC) 全景完整复习笔记

> **目标**：本文档结合 `egos-2000` 官方指南 `p3_syscall.md`，记录了从异常处理、IPC（进程间通信）机制，到动手实现 `sleep` 系统调用的全过程。重点深度融合了在开发中关于“虚拟内存与 PCB 中转机制”、“多核硬件闹钟寄存器”以及“CPU WFI 低功耗休眠机制”的硬核底层知识。

---

## 第一章：异常处理与 `ecall` 机制

### 1.1 中断 (Interrupt) vs 异常 (Exception) 的本质区别
- **中断 (Interrupt)**：由 CPU **外部设备**（如定时器、网卡、磁盘）触发。属于异步事件。（例如 P2 中的抢占式调度 Timer Interrupt）。
- **异常 (Exception)**：由 CPU **内部指令**执行错误或主动请求触发。属于同步事件。（例如访问非法内存、或者主动发起系统调用）。
- **硬件分发依据**：当异常或中断发生时，硬件会将原因代码写入控制状态寄存器 **`mcause`**。最高位（Bit 31）为 1 代表中断，为 0 代表异常。

### 1.2 `ecall` 指令与四关内核分发入口
- RISC-V 提供了 `ecall`（Environment Call）指令，用户态程序执行该指令会故意触发 **“环境调用异常”（Environment call exception, mcause ID 为 8 或 11）**。
- **四关极简口诀链**：
  - **`ecall`**（硬件按门铃升权）
  $\rightarrow$ **`trap_entry`**（汇编保存现场到内核栈）
  $\rightarrow$ **`kernel_entry`**（同步栈上寄存器至 PCB 并分派任务）
  $\rightarrow$ **`excp_entry`**（处理系统调用业务与跨越 `ecall`）

### 1.3 `kernel_entry` 中 PCB 上下文中转的物理过程
在 `kernel_entry` 中，包含了将汇编存放在内核栈上的 32 个通用寄存器备份至 PCB 的关键代码：
```c
#define curr_saved proc_set[curr_proc_idx].saved_registers

/* 1. 进内核：把内核栈上的 32 个通用寄存器 (128字节) 备份到 PCB 的 saved_registers 数组中 */
memcpy(curr_saved, (void*)(EGOS_STACK_TOP - 32 * 4), 32 * 4);

/* 2. 出内核前：把下一个进程 (curr_proc_idx 已在 proc_yield 中被切为进程 B) PCB 里的 saved_registers 覆写回内核栈 */
memcpy((void*)(EGOS_STACK_TOP - 32 * 4), curr_saved, 32 * 4);
```
- **核心作用**：在汇编 `trap_entry` 弹栈前，通过 `memcpy` 实现“进去备份进程 A、出来装载进程 B”的切换，使得后续 `mret` 能顺理成章地恢复进程 B 的上下文。

---

## 第二章：IPC 进程间通信与 PCB 的内存哲学

### 2.1 阻塞式 IPC 的数据结构
系统调用由 `library/syscall/syscall.h` 中的 `struct syscall` 定义：
```c
struct syscall {
    enum syscall_type type;         // SYS_SEND 或 SYS_RECV
    int sender, receiver;           // 发送方与接收方的 PID
    char content[1024];             // 1024 字节的消息载荷数据缓冲区
    enum { PENDING, DONE } status;  // 挂起还是已完成
};
```

### 2.2 深度问题解析：为什么数据必须经由内核 PCB 中转，而不直接复制？
**疑问**：在实现 `sys_send` 和 `sys_recv` 时，代码从发送方的虚拟内存把数据复制到发送方 PCB，再从发送方 PCB 复制到接收方 PCB，最后再复制回接收方虚拟内存。为什么要这么绕？
**核心原因**：
1. **MMU 虚拟地址空间隔离**：进程 A 和进程 B 的页表完全独立。当 CPU 在运行进程 A 时，内核根本“看不见”进程 B 的虚拟内存。直接内存拷贝是不可能的。
2. **异步与暂存缓冲 (Buffer)**：发送消息时，接收方可能正在睡觉或者正在干别的。内核中的 PCB (`struct process`) 是全局驻留的，充当了**绝对安全的快递中转站**。
3. **内核安全边界**：将用户空间传入的数据拷贝进内核空间再进行合法性校验，可以防止用户态程序利用多线程恶意篡改数据（TOCTOU 攻击）。

### 2.3 深度问题解析：PCB (`struct process`) 的内存布局全貌
在 `grass/process.h` 中，IPC 缓存是直接**硬编码嵌套**在 PCB 结构体中的：
```c
struct process {
    int pid;                         // 进程标识符
    struct syscall syscall;          // 嵌套的系统调用/IPC消息缓冲区！
    enum proc_status status;         // 调度状态
    uint saved_registers[32];        // 寄存器现场备份
    // ... 其他统计字段与 sleep_until 字段
};
```
当匹配成功时，内核只需执行 `memcpy(dst->syscall.content, sender->syscall.content, 1024)`，这本质上就是**在内核进程数组中，安全地把 A 的成员变量复制给了 B 的成员变量**。

---

## 第三章：实现 `sleep` 与 CPU 极致低功耗优化

### 3.1 `sleep` 的生命周期与时间拦截
- 当进程调用 `sleep(usec)` 时，实际上是向 `GPID_PROCESS` 发送了一个 `PROC_SLEEP` 消息。
- 内核 `proc_sleep` 会计算唤醒的绝对时间戳：`p->sleep_until = mtime_get() + usec * 10`。
- 在 `proc_yield` 的调度循环中加入**时间拦截机制**：如果当前时间 `< sleep_until`，就跳过该进程不予调度；时间到达后清零并恢复调度。

### 3.2 深度问题解析：拒绝“忙等待”，优雅的 `wfi` 低功耗休眠
**疑问**：当所有进程都在休眠时，为什么不能直接 `return proc_yield();` 让内核不断循环判断时间？
**解答**：这种做法叫**轮询 (Polling/Busy-waiting)**，会让 CPU 占用率飙升到 100%，极度浪费电能。
**优雅解法（硬件休眠）**：
```c
if (!has_active_process) { FATAL(...); } 
curr_proc_idx = MAX_NPROCESS;
earth->timer_reset(core_in_kernel);
asm("csrs mstatus, %0" ::"r"(1 << 3));
asm("wfi");
```

### 3.3 深度核心拷问：执行 `wfi` 前的这 3 步，真的有必要吗？
在今天的讨论中，你提出了一个非常有深度的问题：“这三步前置准备看起来很多余，为什么不能直接执行 `wfi`？” 
我们通过**“如果不这么做会发生什么”**的思想实验，彻底击碎了这个疑惑：

1. **为什么必须 `curr_proc_idx = MAX_NPROCESS;`？**
   - **如果省略**：`curr_proc_idx` 还会指向刚刚休眠的用户进程。当 `wfi` 被时钟中断唤醒时，CPU 会强行进入 `trap_entry` 并保存现场。此时，内核会将**当前 `wfi` 指令的上下文（甚至是内核的垃圾寄存器）覆盖写入该用户进程的 PCB 中**！
   - **结果**：该用户进程的“复活存档”被彻底摧毁，下次调度到它时必将跑飞崩溃。将指针切到 `MAX_NPROCESS` 这个废弃占位符，就是为了提供一个安全的“垃圾桶”来吸收废弃的上下文。

2. **为什么必须 `earth->timer_reset(core_in_kernel);`？**
   - **如果省略**：既然所有进程都在休眠，如果没有人去设定下一个闹钟，系统的时钟倒计时就会停滞。
   - **结果**：CPU 执行 `wfi` 后睡死过去，再也没有时钟中断来唤醒它，整个操作系统永久卡死。

3. **为什么必须 `asm("csrs mstatus, %0" ::"r"(1 << 3));`（开启全局中断 MIE）？**
   - **进入内核时的状态**：触发 `ecall` 或异常进入内核时，硬件为了防止中断嵌套干扰，自动将 `mstatus.MIE` 置为了 0（关中断状态）。
   - **RISC-V 硬件对 `wfi` 的特殊响应规则**：
     - **若 `MIE = 1`（开中断）**：闹钟响时 $\rightarrow$ 硬件允许劫持 $\rightarrow$ **跳入 `trap_entry`** 重新开始全新调度。（预期逻辑）
     - **若 `MIE = 0`（关中断 - 省略本句）**：闹钟响时 $\rightarrow$ CPU 确实会被唤醒退出低功耗状态，**但硬件禁止跳转到 `trap_entry`**！`wfi` 指令直接执行结束，CPU 继续顺序执行 `wfi` 下方的下一条 C 语言指令（$PC = PC + 4$）。
   - **致命后果**：如果从 `wfi` 顺延往下走，代码将跌出 `if-else` 分支，拿还处于 `MAX_NPROCESS` 占位符状态的 `next_idx` 去切换页表（`earth->mmu_switch`）并恢复上下文，导致系统读取垃圾内存崩溃！打开 `MIE` 才能赋予闹钟“劫持 CPU 跳回 `trap_entry` 重新调度”的合法权限。

### 3.4 深度问题解析：`timer_reset(core_in_kernel)` 中参数的硬件含义
**疑问**：为什么要传 `core_in_kernel` 给定时器重置函数？
**解答**：这体现了操作系统的**多核架构思维**。在 RISC-V 芯片中，虽然全系统只有一个一直递增的全局大钟表（`mtime`），但每个 CPU 核心（Hart）都有自己独立的小闹钟寄存器（`mtimecmp`）。传入当前的核心 ID，是为了**只拨动当前休眠核心的闹钟**，确保等会儿时间到了，打断的是本核心，而不会错误地影响其他正在并行的核心。

---

## 第四章：全流程代码融合追踪案例 (The Big Picture)

### 案例：进程 A 调用 `sleep(5000000)` 的源码级精确穿透轨迹

假设**进程 A** (`apps/user/sleep.c`) 想要休眠，**进程 B** 是系统管理进程 (`GPID_PROCESS`)。

#### 第一幕：进程 A 在用户态发出系统调用
1. **构造消息**：进程 A 调用 `sleep(5000000)`。底层库函数组装好 `struct proc_request`（包含类型 `PROC_SLEEP` 和时间 `5000000`）。
2. **填写用户态 PCB**：在 `sys_send` (`library/syscall/syscall.c`) 中，进程 A 将上述请求 `memcpy` 拷贝到自己的**用户态虚拟内存绝对地址 `SYSCALL_ARG`** (0x80301000) 处。
3. **陷入内核**：进程 A 执行 `asm("ecall")`，CPU 跳入内核异常处理。

#### 第二幕：内核搬运数据 (MMU 翻译 -> 内核 PCB)
1. **捕获异常**：经过 `trap_entry` 保护现场后，代码流转到 `excp_entry`。
2. **虚拟转物理**：内核调用 `earth->mmu_translate(curr_pid, SYSCALL_ARG)`，查页表，找出进程 A 那 1024 字节数据在物理内存条上的真实位置。
3. **拷入发件箱**：执行 `memcpy(&proc_set[curr_proc_idx].syscall, paddr, ...)`。此时，进程 A 的请求被安全地放到了**内核里的 PCB 中转站**。进程 A 状态变为 `PROC_PENDING_SYSCALL`，等待处理。

#### 第三幕：IPC 精准投递 (内核 PCB -> 内核 PCB)
1. **尝试发送**：接着调用 `proc_try_send(&proc_set[curr_proc_idx])`。
2. **寻找收件人**：内核遍历所有进程，发现目标接收方是进程 B (`GPID_PROCESS`)，且进程 B 此时正挂起在 `SYS_RECV`（等待接收消息）状态。
3. **完成对接**：执行 `memcpy(dst->syscall.content, sender->syscall.content, SYSCALL_MSG_LEN);`
   *(这一步是点睛之笔：在内核安全的内存池里，直接完成了从进程 A 发件箱到进程 B 收件箱的数据交接！)*
4. **唤醒进程 B**：将进程 B 状态改为 `PROC_RUNNABLE`。

#### 第四幕：进程 B 接收并处理睡眠请求
1. **进程 B 拿到数据**：下一轮调度中进程 B 获得 CPU，在 `proc_try_recv` 里，内核将进程 B 的 PCB 收件箱数据，**拷贝回进程 B 的用户态虚拟内存 `SYSCALL_ARG`**。
2. **执行休眠指令**：进程 B 解析发现是 `PROC_SLEEP`，于是调用 `grass->proc_sleep`。
3. **设置唤醒时钟**：内核在进程 A 的 PCB 里写下 `sleep_until = mtime_get() + 50000000`。

#### 第五幕：调度器放弃 CPU（WFI 挂起）
1. 内核再次回到 `proc_yield` 准备找下一个活着的进程。
2. 找了一圈，发现进程 A 没到唤醒时间被 `continue` 跳过，进程 B 又去 `SYS_RECV` 挂起了，系统彻底没事可做（`next_idx == MAX_NPROCESS`）。
3. 内核机智地执行了四步曲：
   - 伪装成 `MAX_NPROCESS` 占位符；
   - 拨动小闹钟 `earth->timer_reset(core_in_kernel)`；
   - 打开耳朵 `MIE=1`；
   - 深度睡眠 `asm("wfi")`。
4. 此时整个计算机安静下来，CPU 进入低功耗睡眠。

#### 第六幕：闹钟唤醒与进程复苏
1. 5 秒钟后，底层硬件中的 `mtime` 终于追上了 `mtimecmp`！
2. 硬件电路瞬间拉高中断线，CPU 被惊醒，强行跳入 `trap_entry`。
3. 此时原本 `wfi` 的垃圾断点被安全地保存到了占位符 `MAX_NPROCESS` 里。
4. 内核顺理成章地进入 `proc_yield`，再次遍历进程表，发现进程 A 的 `sleep_until` 已经过期。
5. 内核将进程 A 恢复为 `PROC_READY`，把它选为下一个 `next_idx`，执行 `mret` 返回。
6. 进程 A 苏醒，继续在屏幕上打印出：“Woke up again after 5000000 microseconds.”！

**闭环达成！这就是一次系统调用在软硬件深度的完美共舞。**

---

## 第五章：系统保护与微内核的生死边界 (System Protection)

### 5.1 物理内存保护 (PMP) 与 Software TLB 的“笨拙感”
在 P3 的内存保护阶段，内核通过配置 RISC-V 的 `pmpaddr` 和 `pmpcfg` 寄存器（采用 NAPOT 模式拼接基址和大小），强制限定用户程序只能访问 `0x80200000` (前台 2MB) 和 Shell 工作目录区。
- **直觉的碰撞**：当存在多个用户进程时，当前的 **Software TLB** 方案显得非常“笨拙”——它依赖于在每次切换进程时，通过 `memcpy` 把内存数据在后台缓冲区和前台 2MB 舞台之间疯狂搬运。
- **向 P4 虚拟内存的过渡**：这种低效的拷贝仅仅是为了在 P3 中避开复杂的页表概念。真正的现代 OS（P4 阶段）将引入**硬件多级页表**，届时每个进程的数据都静静地躺在各自的物理内存中，切换进程时只需拨动 `satp` 寄存器，MMU 硬件瞬间完成地址重定向，实现零拷贝。

### 5.2 特权级切换机制 (`mstatus.MPP`)
PMP 硬件防线搭建完毕后，还必须配合特权级降权。CPU 是在执行 `mret` 退出内核时，根据 `mstatus.MPP`（Bit 11-12）的值来决定下一指令运行在什么权限下：
- `MPP = 00` (二进制)：进入 User Mode（用户模式）。
- `MPP = 11` (二进制)：进入 Machine Mode（机器模式）。
在 `proc_yield` 中，调度器会利用汇编的位操作来动态切换模式：
1. `csrc mstatus, 3 << 11`：默认清零，将 MPP 设为 `00`（降权为 User Mode）。
2. `csrs mstatus, 3 << 11`：按需置 1，如果下一个进程是内核服务进程（如管家、文件系统），则设为 `11`（维持 M-Mode）。

### 5.3 异常处决与“代写遗言”的零拷贝 IPC 哲学
当用户恶意程序（如 `crash1`）越权访问内存时，PMP 硬件会强制抛出内存访问故障（异常 ID 1/5/7），打断其执行并跌入内核 `excp_entry`。此时若直接在内核中调用 `proc_free` 将其“火化”，会导致极其隐蔽的死锁 Bug。
- **微内核进程管家 (`GPID_PROCESS`)**：在 EGOS-2000 的微内核架构下，内核极度精简，真正的进程管理业务交由用户态的“进程管家”处理。Shell 启动程序后，会挂起死等管家的完工通知。
- **卡死之谜**：若内核直接释放了崩溃进程，管家将永远收不到崩溃进程发来的 `PROC_EXIT` 辞职信，进而无法通知 Shell 苏醒，导致终端再也不会弹出 `➜` 提示符。
- **终极解决方案：零拷贝 IPC 篡改**：
  内核在 `excp_entry` 中截获异常后，**直接越权篡改崩溃进程 PCB 中的 `syscall.content` 缓存**，强行在其中写下一封 `PROC_EXIT` 的遗书，并将发件信封寄给 `GPID_PROCESS`：
  ```c
  struct proc_request *req = (struct proc_request *)proc_set[curr_proc_idx].syscall.content;
  req->type = PROC_EXIT;  // 直接向 PCB 结构体中的 1024 字节内容块写入 4 字节数据，无需冗余的 500 字节 memcpy 拷贝
  proc_set[curr_proc_idx].syscall.type = SYS_SEND;
  proc_set[curr_proc_idx].syscall.receiver = GPID_PROCESS;
  // ...调用 proc_try_syscall 投递并 yield
  ```
- **核心思想升华**：这一神来之笔不仅展示了 **Zero-Copy（零拷贝）** 极速写入 4 字节的极致性能优化，更完美地维系了微内核消息驱动的设计哲学，让进程的生死流转无缝闭环！
