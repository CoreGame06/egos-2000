# Grass 目录说明

Grass 负责管理进程和线程：保存和恢复 CPU 上下文、选择接下来运行谁，以及处理系统调用和进程间消息。

## 各文件的作用

| 文件 | 作用 |
| --- | --- |
| [init.c](init.c) | 接着 Earth 的初始化往下执行，登记 Grass 接口，加载并启动第一个进程 `sys_proc`。 |
| [process.h](process.h) | 定义 PCB 进程控制块，记录 PID、状态、CPU 上下文、运行时间、优先级和睡眠时间。 |
| [process.c](process.c) | 创建时从 `proc_set` 找一个空闲位置，填写 PID 和初始状态；退出时回收内存，把记录标为空闲。还负责调整优先级、设置睡眠时间和查看各核心运行的进程。 |
| [kernel.s](kernel.s) | 中断或异常发生后的汇编入口：获取锁，保存寄存器，调用 `kernel_entry()`，最后恢复所选线程的寄存器，释放锁并执行 `mret`。 |
| [kernel.c](kernel.c) | 把 CPU 上下文存进 PCB，处理中断和系统调用，选择下一条线程，再取出它的 CPU 上下文准备恢复。消息收发和线程创建也在这里处理。 |

## PCB、PID 和 CPU 核心

PCB 是内核保存进程/线程信息的结构，也就是 `struct process`。其中 `mepc` 和 `saved_registers` 保存 CPU 上下文，让线程被打断后还能继续执行；运行时间、状态和优先级等字段供内核管理和调度使用。

`proc_set[]` 是存放 PCB 的数组。下标 0～15 用来记录进程/线程；下标 16 是空闲占位记录。核心空闲时仍可能发生中断，此时把 CPU 上下文存到占位记录，避免覆盖用户线程的上下文。

`core_to_proc_idx[]` 表示每个核心当前运行的是 `proc_set` 中的哪一项。例如：

```c
core_to_proc_idx[1] = 5; // 核心 1 正在运行 proc_set[5] 对应的线程。
proc_set[5].pid = 20;   // 这个线程属于 PID 20 的进程。
proc_set[9].pid = 20;   // 同一进程的另一个线程，占用不同的数组位置。
```

PID 与数组下标不能直接对应：PID 随创建不断增加，数组位置在退出后可以复用；同一进程的多个线程还会共享 PID。当前最多 16 条有效记录，因此按 PID 遍历查找，暂不额外维护索引。

## 中断处理和上下文切换

中断、异常或系统调用发生后，CPU 进入 `kernel.s` 的 `trap_entry`。汇编先获取 `kernel_lock`，把寄存器值保存到内核栈，再调用 `kernel_entry()`。

`kernel_entry()` 主要做四件事：

1. 获取当前核心编号，找到它刚才运行的 PCB。
2. 把被打断线程的 PC 和寄存器值保存到它的 PCB。
3. 处理这次中断或异常，必要时调用 `proc_yield()` 选择接下来运行的线程。
4. 从所选线程的 PCB 取出 PC，写入 CPU 的 `mepc`；取出寄存器值，复制到内核栈顶部的保存区。

返回 `kernel.s` 后，汇编从保存区恢复真正的 CPU 寄存器，再执行 `mret`，从 `mepc` 指定的位置继续运行。


几个处理函数的分工：

- `excp_entry()`：系统调用就尝试处理请求；普通应用执行出错，就请求 `sys_proc` 处理退出。
- `intr_entry()`：时钟中断触发调度；网卡中断处理收到的数据包。
- `proc_yield()`：更新运行时间和优先级，跳过仍在睡眠或等待的线程，选择可运行且优先级最高的线程。

`proc_yield()` 的调用都从上面的汇编入口进来，因此进入函数时已经持有内核锁。锁用来保护共享的内核栈和调度数据，避免多个核心互相覆盖数据、同时选中同一条线程。

当前核心没有线程可运行时，就标为空闲，设置下一次时钟中断，释放锁，再等待中断。下次中断到来后重新检查有没有线程可以运行。

## 创建进程和创建线程

创建进程，例如在 Shell 输入 `echo hello`：

```text
Shell 向 sys_proc 发送 PROC_SPAWN 请求
    → sys_proc 调用 proc_alloc()，分配新 PID，填写 PCB
    → 调用 elf_load()，加载 echo，准备它的内存、页表、参数和栈
    → 标记为 READY，等待调度器首次选中它执行
```

代码在 [sys_proc.c](../apps/system/sys_proc.c) 的 `app_spawn()`。第一个进程 `sys_proc` 则由 `init.c` 直接启动。

创建线程，例如 [producer_consumer.c](../apps/user/producer_consumer.c) 中：

```c
thread_create(produce, NULL); // 新线程执行 produce(NULL)，主线程继续执行 main 的消费循环。
```

`thread_create()` 把入口、参数和栈顶写进系统调用请求，然后执行 `ecall`。内核的 `proc_try_thread_create()` 找一个空闲 PCB，沿用原 PID，设置新线程的入口、寄存器和栈指针，标记为可运行。



## 内核态、用户态和内存访问

本项目用 M-mode 表示内核态，U-mode 表示用户态。中断处理和调度在 M-mode 执行；普通应用和 `sys_proc` 等系统服务都在 U-mode 执行。

进入和返回的过程：

```text
用户态执行 ecall，或发生中断、异常
    → CPU 自动切到 M-mode，跳到 trap_entry
    → 内核处理事件，准备接下来运行线程的寄存器和 PC
    → 执行 mret，切回 U-mode，继续运行
```
> 这里的切换是由==硬件==完成的

两种模式的区别主要有两点：

- **指令权限不同**：内核可以修改 `mstatus`、`mtvec`、`satp` 等控制寄存器。用户态不能执行这些修改，也不能直接执行 `mret`。普通计算、函数调用和访问被允许的内存，两种模式都可以进行。
- **访问内存的方式不同**：按本项目当前设置，M-mode 内核普通访存直接使用物理地址；U-mode 程序的地址先经过自己的页表翻译，能访问哪些位置由映射和访问权限决定。

普通应用主要映射自己的代码、数据、参数和栈，也映射了共享工作目录及显示、网络演示需要的区域。系统服务的页表允许访问更多内存，包括 PCB、页表和设备区域，因此可以直接做进程管理、磁盘读写等工作。


每个进程都把系统调用请求写在自己虚拟地址空间的 `SYSCALL_ARG` 位置。内核要先查这个进程的页表，找到请求所在的物理地址，再把请求复制到 PCB 中处理：

```c
uint syscall_paddr = earth->mmu_translate(curr_pid, SYSCALL_ARG);
memcpy(&proc_set[curr_proc_idx].syscall, (void*)syscall_paddr,
       sizeof(struct syscall));
```


## 与 Linux 的区别

Linux 中，进程和线程的创建共用内核中的大量代码，通过参数决定是否共享地址空间等资源。
这个项目也统一使用 `proc_set` 保存调度信息，但创建流程分开了：进程由用户态 `sys_proc` 服务组织创建，线程由内核直接创建。

另外，Linux 的 `fork()` 先创建当前进程的副本；本项目的进程创建直接加载指定程序，例如 echo，所以两者的流程不完全相同。