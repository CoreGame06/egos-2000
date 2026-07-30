# 协作式多线程与同步机制（P1 实验）全景完整复习笔记

> **目标**：本文档严格结合 `egos-2000` 官方实验指南 `p1_ct.md` 的完整理论体系与代码架构，并融入你在探索过程中讨论的所有微观硬件与操作系统底层机制。按照知识演进逻辑整理，确保复习时绝无任何遗漏。

---

## 第一章：多线程与并发基础理论

### 1.1 什么是线程与并发 (Threads & Concurrency)
- **线程定义**：我们可以将一个线程理解为在执行一个函数。每个线程拥有**独立的栈空间（Stack）**和**独立的 CPU 寄存器上下文（Context）**，但共享进程的代码段、数据段和堆。
- **并发（Concurrency）**：并发意味着操作系统/线程库**不强制规定线程之间的执行顺序**。
  - 例如 `main` 线程调用 `thread_create(child, ...)` 后，系统中有两个活跃线程。`Child thread is running` 和 `Main thread is running` 的打印顺序是未定义的（主先子后，或子先主后均合法）。
- **0 号主线程的诞生**：
  - `main()` 函数本身就是程序启动后的第一个线程（0 号主线程）。
  - 在未调用 `thread_init()` 之前，0 号线程已经在 CPU 上运行，但在 TCB 数据结构中“无牌无户”。`thread_init()` 的本质就是**给当前运行的主线程“上户口”**：分配 TCB 并设置 `id = 0` 和 `status = THREAD_RUNNING`。

### 1.2 协作式（Cooperative）与抢占式（Preemptive）的区别
- **P1 协作式多线程**：线程通过显式调用 `thread_yield()` **主动让出 CPU 控制权**。如果一个线程不主动 `yield` 或阻塞，它将一直独占 CPU。
- **P2 抢占式多线程**：CPU 通过硬件时钟中断，在线程未主动让出 CPU 的情况下**强制剥夺其执行权**进行上下文切换。
- **历史背景**：Windows 95 和 Windows NT 之前的 16 位 Windows 以及经典 Mac OS 使用的就是协作式多任务调度。

---

## 第二章：内存布局与上下文切换物理过程

### 2.1 物理内存 4 大区域与 CPU 上下文 (Context)
内存分为 4 种区域：代码段 (Code)、数据段 (Data)、堆区 (Heap)、栈区 (Stack)。
- **CPU 上下文定义**：由 **程序计数器（PC）** 和 **通用寄存器（如栈指针 SP）** 共同决定。
  - 当 `sp` 指向主线程栈，`pc` 指向 `main` 指令时，CPU 处于主线程上下文。
  - 当 `sp` 指向子线程栈，`pc` 指向 `child` / `printf` 指令时，发生物理上下文切换。

### 2.2 32 位 RISC-V 汇编物理切栈 (`thread.s`)
在 32 位 RISC-V 架构下，包含 32 个通用寄存器（`x0` ~ `x31`），每个 4 字节，共占用 $32 \times 4 = 128$ 字节栈空间。

```text
[ 旧线程栈 (0x80400000) ]                 [ C 语言 TCB 结构体 (内存地址 0x2000) ]
+-------------------------+              +----------------------------------+
| ... 旧栈数据 ...         |              | id: 0                            |
+-------------------------+              +----------------------------------+
| 32个通用寄存器 (128B)    | <--- sp      | sp: 0x80400180  <-- sw sp, 0(a0) |
| (含返回地址 ra)          | (0x80400180) +----------------------------------+
+-------------------------+              | status: THREAD_READY             |
                                         +----------------------------------+
```

#### ① `ctx_start(&sp_old, sp_new)` 执行细节：
1. `addi sp, sp, -128`：在旧栈上开辟 128 字节空间。
2. `SAVE_ALL_REGISTERS`：将旧线程当前 32 个 CPU 寄存器压入旧栈。
3. `sw sp, 0(a0)`：根据 RISC-V ABI，`a0` 保存的是 C 语言传入的二级指针 `&old_thread->sp`。该指令把压栈后的最新 `sp` **精准写回到 TCB 结构体的 `sp` 字段**中。
4. `mv sp, a1`：**物理切栈**，把 `a1`（即 `init_sp = child_stack + STACK_SIZE` 高端地址）赋值给 CPU 的 `sp`。
5. `call ctx_entry`：在新栈上启动 C 语言包装函数。

#### ② `ctx_switch(&sp_old, sp_new)` 执行细节：
1. `addi sp, sp, -128` & `SAVE_ALL_REGISTERS`：保存旧线程 32 个寄存器。
2. `sw sp, 0(a0)`：更新旧线程 TCB 中的 `sp`。
3. `mv sp, a1`：将 CPU `sp` 切换到目标线程保存在 TCB 中的 `sp`。
4. `RESTORE_ALL_REGISTERS` & `addi sp, sp, 128`：从目标线程栈中弹出 32 个寄存器。
5. `ret`：安全返回到目标线程上次调用 `ctx_switch` / `ctx_start` 的下一行代码继续运行。

### 2.3 `ctx_entry()` 的“悬崖”安全拦截机制
- **角色**：新线程在新的栈上运行的第一行 C 语言代码。
- **机制**：当 `ctx_start` 切到新栈时，新栈底没有调用者返回地址。如果直接调用 `entry(arg)`，`entry` 返回时执行 `ret` 会跳入野内存悬崖崩溃。
- **解法**：`ctx_entry()` 包装了 `entry(args);`，并在其下方紧接着写了 **`thread_exit();`**。一旦业务函数运行完返回，`ctx_entry` 强制将线程送入 `thread_exit()` 安全清理并切走。

### 2.4 `_end()` 死循环与 Bare-metal 系统停机哲学
- `egos-2000` 运行在 RISC-V QEMU 裸机（Bare-metal）环境下，没有上层操作系统回收进程。
- 当所有线程都调用 `thread_exit()` 退出后，系统终结的唯一方式就是执行无限死循环 `1: j 1b` 让 CPU 挂起。

---

## 第三章：线程控制块 (TCB) 与资源管理

### 3.1 TCB 结构体元数据 (Metadata)
TCB 保存每个线程的元数据：
- `id`：线程唯一标识符。
- `status`：线程状态（`UNUSED`, `RUNNING`, `READY`, `TERMINATED`, `WAITING`）。
- `sp`：物理切换前保存的 `sp` 寄存器值 (`void*`)。
- `entry`：线程入口函数指针 (`void (*entry)(void *arg)`)。
- `args`：传入入口函数的无类型指针 (`void*`)。
- `stack_base`：通过 `malloc` 分配的栈基地址，用于终止时 `free` 释放。

### 3.2 内存回收的严苛约束
- **主线程栈**：主线程栈由编译器/系统初始化分配，**绝不能用 `free()` 释放**。
- **子线程栈**：子线程栈通过 `malloc(STACK_SIZE)` 分配，`child_stack + STACK_SIZE` 计算得出高端初始栈顶。
- **释放时机（扫尾清理）**：正在运行的线程在执行 `thread_exit()` 和 `ctx_switch()` 时仍在使用当前栈，**绝不能在退出当前瞬间释放自己的栈**。必须移入 `zombie_queue`（僵尸队列），等上下文切到其他活线程后再进行垃圾回收。

---

## 第四章：并发同步与条件变量 (Conditional Variables)

### 4.1 条件变量 3 大原语
解决多线程等待特定条件（如数据包到位、缓冲区非空非满）：
- `cv_init(struct cv *cv)`：初始化条件变量等待队列。
- `cv_wait(struct cv *cv)`：将当前线程移出 TCB 就绪列表，加入条件变量等待队列，设为 `WAITING` 并让出 CPU。
- `cv_signal(struct cv *cv)`：从条件变量等待队列中取出一个阻塞线程，将其放回 TCB 就绪列表中，设为 `READY`。

### 4.2 经典的等待范式与伪唤醒
```c
while (!condition) {
    cv_wait(&cond_cv);
}
```
必须使用 `while` 循环而非 `if` 判断：防止多线程并发唤醒时的竞态条件（Spurious Wakeup），确保醒来后重新检查条件是否成立。

### 4.3 生产者-消费者模型 (Producer-Consumer)
- 限制：缓冲区固定容量（如 3）。
- 条件变量定义：
  - `nonfull`：缓冲区未满（生产者等待 `count == 3`，生产后 `cv_signal(&nonempty)`）。
  - `nonempty`：缓冲区非空（消费者等待 `count == 0`，消费后 `cv_signal(&nonfull)`）。

---

## 第五章：调度数据结构演进 (固定数组 vs `TAILQ`)

### 5.1 数组模式的“包场与饥饿”根源
- 在固定数组模式下，`cv_signal` 线性扫描数组激活第一个 `WAITING` 线程（如 Producer 0），`thread_yield` 从索引 0 开始寻找 `READY` 线程。刚被唤醒的线程瞬间抢占 CPU，形成抢占闭环，导致后创建的 Producer 1~499 彻底饥饿（Starvation）。

### 5.2 `<sys/queue.h>` 选型：为什么用 `TAILQ` 不用 `SLIST`？
- **`SLIST`（单链表）**：仅支持 `INSERT_HEAD`（头插）。若做就绪队列会变成 LIFO（栈），先排队的线程会被永远插队而饿死。
- **`TAILQ`（双向尾队列）**：支持 `TAILQ_INSERT_TAIL`（尾插）与 `TAILQ_FIRST` + `TAILQ_REMOVE`（头摘），保证严格的 **FIFO（先来先服务/Round-Robin）** 公平轮转。

### 5.3 宏（Macro）的文本替换机制
- `TAILQ_INSERT_TAIL(&queue, old_thread, entries)` 中的 `entries` 是 C 语言宏的文本标记。宏内部将其替换拼接为 `old_thread->entries.tqe_next`，因此传的是成员变量名字而非指针。

---

## 第六章：高阶系统架构与内存安全思考

### 6.1 线程栈位于全局堆区的本质风险与工业级 OS 解法
- **疑问**：子线程栈是通过 `malloc(1024)` 从全局堆区分配的，共享堆空间不会出乱子吗？
- **深层剖析**：
  - **隐患**：
    1. **栈溢出踩坏堆 (Stack Overflow)**：子线程栈仅 1024 字节，深递归或大局部变量会向下踩烂相邻堆数据/TCB。
    2. **数据竞态 (Data Race)**：共享堆空间无保护并发读写会导致数据覆盖破坏。
    3. **缺少硬件隔离**：Bare-metal 下无 MMU 保护页拦截越界。
  - **工业级 OS（Linux/Windows）解决方案**：
    - **警戒页（Guard Page）**：MMU 在每个线程栈底设置一页无读写权限的保护页，一旦越界触发 `Segmentation Fault` 杀死线程保护堆区。
    - **线程局部内存池 (Thread-Local Cache)**：如 `tcmalloc`，为每个线程分配独立的堆内存缓冲区。

---

## 第七章：与 egos-2000 操作系统内核的演进联系

- P1 实现的 `ctx_switch` 汇编与 `egos-2000` 内核 `grass/kernel.s` 中的进程上下文切换代码完全一致。
- P1 实现的 `thread_yield` 与 `grass/kernel.c` 中的 `proc_yield` 类似。
- 在 P2（抢占式调度）和 P3（进程管理与系统调用）中，你将继续修改 `proc_yield` 并彻底掌握 `grass` 目录下的所有内核源码！

---

## 第八章：`<sys/queue.h>` 常用 API 查表

| 宏 API | 作用 | 物理含义与底层原理 |
| :--- | :--- | :--- |
| `TAILQ_ENTRY(thread) entries;` | 结构体挂钩 | 在 `struct thread` 内插入 `tqe_next` 和 `tqe_prev` |
| `TAILQ_HEAD(thread_queue, thread);` | 声明队列头类型 | 生成包含队头指针 `tqh_first` 和队尾指针 `tqh_last` 的头结构体 |
| `TAILQ_INIT(&queue)` | 初始化队列 | 将队列的头尾指针设为 NULL |
| `TAILQ_INSERT_TAIL(&queue, elm, entries)` | 队尾入队 | 尾插法，按 FIFO 顺序排队 |
| `TAILQ_FIRST(&queue)` | 获取队头 | 返回队列第一个元素指针 |
| `TAILQ_REMOVE(&queue, elm, entries)` | 摘除节点 | 将节点从队列链表中解绑 |
| `TAILQ_EMPTY(&queue)` | 判空 | 检查队列是否为空 |
