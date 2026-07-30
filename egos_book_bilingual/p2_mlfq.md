# Preemptive Scheduling

> **抢占式调度**

This project helps you understand the scheduler of egos-2000. Most of the egos-2000 scheduler logic is in the `proc_yield` function in grass/`kernel.c`, which, similar to the `thread_yield` function in P1, selects the next thread to be scheduled when the current thread yields. However, egos-2000 has a preemptive scheduler, meaning that the current thread is forced to yield even if it does not directly invoke the yield function. This is made possible by timer interrupts, so we start by introducing what a timer is.

本项目的目标是帮助你理解 egos-2000 的调度器。egos-2000 的大部分调度逻辑位于 `grass/kernel.c` 中的 `proc_yield` 函数里，该函数类似于 P1 中的 `thread_yield` 函数，当当前线程放弃 CPU 时，选择下一个要被调度的线程。然而，egos-2000 拥有一个抢占式调度器（preemptive scheduler），这意味着即便当前线程没有直接调用 `yield` 函数，它也会被强制放弃 CPU。这是通过定时器中断（timer interrupt）实现的，因此我们首先来介绍什么是定时器。

This screenshot is a timer in MacOS. You can set the timer to a value (e.g., 15 seconds) and start it. When this time period has elapsed, the timer will raise an interrupt (e.g., the Radial ringtone). This project shows you how to do something similar within a CPU: instead of raising ringtones, the CPU sets the program counter to the code of a special function when the time period has elapsed. This special function is called an interrupt handler, which will further invoke `proc_yield`, switching the CPU context to a different thread in egos-2000.

这张截图展示了 MacOS 中的定时器。你可以将定时器设置为一个值（例如 15 秒）并启动它。当这段时间到达后，定时器会触发一个中断（例如播放 Radial 铃声）。本项目将展示如何在 CPU 内部实现类似的操作：当设定的时间间隔到达时，CPU 不会播放铃声，而是将程序计数器（program counter）设置为一个特殊函数的入口地址。这个特殊函数被称为中断处理程序（interrupt handler），它会进一步调用 `proc_yield`，从而将 CPU 上下文切换到 egos-2000 中的另一个线程。

> [!NOTE]
> TIP

>
> We do not distinguish thread and process for now, so you can simply regard the `struct process` and `proc_yield` from egos-2000 as the `struct thread` and `thread_yield` from P1. We start to distinguish the two when introducing virtual memory in P4.

>
> 提示

>
> 我们目前暂不区分线程（thread）和进程（process），因此你可以直接将 egos-2000 中的 `struct process` 和 `proc_yield` 视为 P1 中的 `struct thread` 和 `thread_yield`。在 P4 引入虚拟内存时，我们才会开始区分这两者。

## Timer interrupt

> **定时器中断**

We start by explaining the following demo program, which prints Got a timer interrupt. repeatedly every QUANTUM time units. The time unit here is 10^-7 seconds.

我们首先讲解以下示例程序，该程序每隔 `QUANTUM` 个时间单位重复打印 `Got a timer interrupt.`。这里的时间单位为 $10^{-7}$ 秒。

```c
#define QUANTUM 10000000

int main() {
    /* Register the handler. */
    asm("csrw mtvec, %0" ::"r"(handler));

/* Set a timer period. */
> mtimecmp_set(mtime_get() + QUANTUM);

/* Enable timer interrupt. */
    asm("csrs mie, %0" ::"r"(0x80));
    asm("csrs mstatus, %0" ::"r"(0x8));

/* When a timer interrupt is raised, the CPU behaves as if a
       call to function handler() is added to this while loop. */
    while(1);
}

void handler() {
    printf("Got a timer interrupt.");
    mtimecmp_set(mtime_get() + QUANTUM);
    /* When invoking mret, the CPU behaves as if handler()
       is returning to the while loop in function main(). */
    asm("mret");
}
```

### Register the handler

> **注册中断处理程序**

The first line of the main function registers `handler()` as the interrupt handler. The syntax might look a bit scary, but its job is actually very simple.

`main` 函数的第一行将 `handler()` 注册为中断处理程序。语法看起来可能有点吓人，但它的工作实际上非常简单。

First, `mtvec` is a Control and Status Register (CSR), meaning it serves special purposes unlike general-purpose registers. The special purpose of `mtvec` is to hold the interrupt handler's starting address. In other words, the CPU would set the program counter to the value stored in `mtvec` upon receiving an interrupt. You can learn more about `mtvec` in Chapter 3.1.7 of the RISC-V Reference Manual.

首先，`mtvec` 是一个控制与状态寄存器（CSR），这意味着它与通用寄存器不同，用来实现特定用途。`mtvec` 的特殊用途是保存中断处理程序的起始地址。换句话说，在收到中断时，CPU 会将程序计数器设置为存储在 `mtvec` 中的值。你可以在《RISC-V 参考手册》的第 3.1.7 节中了解有关 `mtvec` 的更多信息。

Second, csrw is the CPU instruction for writing a CSR, and the value written to the `mtvec` CSR here is the address of the first instruction in the handler function. Say this address is `0x80000020`, we can directly write asm("csrw `mtvec`, %0" ::"r"(`0x80000020`)) as well. By using the function pointer handler, the compiler will figure out this address for us. This line of C code would likely be compiled to the following assembly:

其次，`csrw` 是用于写入 CSR 的 CPU 指令，这里写入 `mtvec` CSR 的值是 `handler` 函数中第一条指令的地址。假设该地址为 `0x80000020`，我们也可以直接编写 `asm("csrw mtvec, %0" ::"r"(0x80000020))`。通过使用函数指针 `handler`，编译器会自动帮我们确定该地址。这行 C 代码很可能会被编译成以下汇编代码：

```assembly
lui     a5,0x80000  # a5 is now 0x8000_0000
addi    a5,a5,32    # a5 is now 0x8000_0020
csrw    mtvec,a5    # write the value of a5 to mtvec
```

The compiler can also choose registers and instructions other than `a5`, `lui`, and `addi` as long as the address of the handler function's first instruction is written to `mtvec`.

只要能将 `handler` 函数第一条指令的地址写入 `mtvec`，编译器也可以选择 `a5`、`lui` 和 `addi` 以外的其他寄存器和指令。

### Set a timer period

> **设置定时器周期**

The second line of the main function sets a timer period of QUANTUM, just like we set a 15-second period in the screenshot. You can find the `mtimecmp_set` and `mtime_get` functions in earth/`cpu_intr.c` of egos-2000. They are copy-pasted from the CPU reference manual, and their job is to read or write 8-byte values at special memory addresses.

`main` 函数的第二行设置了一个大小为 `QUANTUM` 的定时器周期，就像我们在截图里设置 15 秒定时周期一样。你可以在 egos-2000 的 `earth/cpu_intr.c` 中找到 `mtimecmp_set` 和 `mtime_get` 函数。它们直接抄录自 CPU 参考手册，其作用是在特定的内存地址读取或写入 8 字节的数据。

Specifically, mtime and mtimecmp represent two special memory addresses. `mtime_get` reads 8 bytes from mtime representing how many time units have elapsed since the CPU was powered on. Hence, the 8-byte value at mtime is automatically incremented by the CPU over time, and the return value of `mtime_get` depends on when the function is called. When the value at mtime equals the value at mtimecmp, a timer interrupt will be raised—which is why this line of code sets the timer period to QUANTUM.

具体而言，`mtime` 和 `mtimecmp` 代表两个特殊的内存地址。`mtime_get` 从 `mtime` 读取 8 个字节，表示自 CPU 加电开机以来已经流逝的时间单位数。因此，`mtime` 处 8 字节的值随着时间的推移由 CPU 自动递增，而 `mtime_get` 的返回值取决于该函数何时被调用。当 `mtime` 处的数值等于 `mtimecmp` 处的数值时，CPU 将触发定时器中断——这就是为什么这行代码将定时器周期设置为 `QUANTUM`。

> [!NOTE]
> TIP

>
> You may regard the timer as a simple device controlled by mtime and mtimecmp. In addition to mtime and mtimecmp, there are other special memory regions used to control various devices connected to the CPU. You will learn about other such regions in P5, where we will use memory-mapped I/O to control an SD card and a VGA display.

>
> 提示

>
> 你可以将定时器视为一个由 `mtime` 和 `mtimecmp` 控制的简单设备。除了 `mtime` 和 `mtimecmp` 之外，还有其他用于控制连接到 CPU 的各种设备的特殊内存区域。你将在 P5 中了解其他此类区域，届时我们将使用内存映射 I/O（memory-mapped I/O）来控制 SD 卡和 VGA 显示器。

### Enable timer interrupt

> **启用定时器中断**

The main function then sets bit#3 of `mstatus` and bit#7 of mie to 1 (i.e., `0x8` is 1<<3; `0x80` is 1<<7) using the CSR set-bit instruction csrs. According to Figures 7 and 16 of the RISC-V Reference Manual, bit#3 of `mstatus` is called `mstatus`.MIE, where MIE stands for Machine Interrupt Enable, and bit#7 of mie is called mie.MTIE, where MTIE stands for Machine Timer Interrupt Enable.

随后，`main` 函数使用 CSR 置位指令 `csrs` 将 `mstatus` 的第 3 位和 `mie` 的第 7 位置 1（即 `0x8` 是 `1<<3`；`0x80` 是 `1<<7`）。根据《RISC-V 参考手册》图 7 和图 16，`mstatus` 的第 3 位被称为 `mstatus.MIE`，其中 MIE 代表机器模式中断使能（Machine Interrupt Enable）；`mie` 的第 7 位被称为 `mie.MTIE`，其中 MTIE 代表机器模式定时器中断使能（Machine Timer Interrupt Enable）。

For the purpose of P2, it is enough to know that setting the `mstatus`.MIE and mie.MTIE bits enable timer interrupts on the CPU; setting either to 0 disables them. We recommend that you scan through Chapter 3.1.6 and 3.1.9 of the RISC-V Reference Manual for `mstatus` and mie, but don't feel obliged to understand everything. We will explain the other bits of these CSRs in the next few projects.

对于 P2 而言，只需知道将 `mstatus.MIE` 和 `mie.MTIE` 位置为 1 会在 CPU 上启用定时器中断；将其中任何一个设置为 0 则会禁用它们。我们建议你浏览一下《RISC-V 参考手册》中关于 `mstatus` 和 `mie` 的第 3.1.6 和 3.1.9 节，但不必非要理解所有内容。我们将在接下来的几个项目中解释这些 CSR 的其他位。

After enabling timer interrupts, the main function enters an infinite loop. When receiving a timer interrupt, the CPU would behave as if a function call to handler is inserted into this loop, and the handler function would reset the timer with QUANTUM again. Therefore, we expect to see Got a timer interrupt. on the screen periodically and infinitely.

启用定时器中断后，`main` 函数进入一个无限循环。当收到定时器中断时，CPU 的行为就像在该循环中插入了对 `handler` 的函数调用一样，而 `handler` 函数会再次将定时器重置为 `QUANTUM`。因此，我们期望在屏幕上周期性地看到 `Got a timer interrupt.` 的重复输出。

### Usage of `mepc` and `mret`

> **mepc 和 `mret` 的用法**

Right before handling a timer interrupt, the CPU assigns the value of its program counter to a CSR called `mepc`, so `mepc` will point to an instruction of while(1) before the handler starts to run. Conversely, when the handler invokes `mret`, the CPU loads the value of `mepc` back into the program counter, resuming the while loop. Moreover, before the CPU jumps to the handler, interrupts are automatically disabled; when the handler function invokes `mret`, CPU interrupts will be automatically re-enabled. In other words, we typically hope to handle one interrupt without being interrupted by another.

在刚要处理定时器中断时，CPU 会将其程序计数器的值赋给名为 `mepc` 的 CSR，因此在中断处理程序开始运行之前，`mepc` 将指向 `while(1)` 循环中的某条指令。反之，当中断处理程序调用 `mret` 时，CPU 将 `mepc` 的值加载回程序计数器中，从而恢复 `while` 循环。此外，在 CPU 跳转到中断处理程序之前，中断会被自动禁用；当中断处理程序调用 `mret` 时，CPU 中断将被自动重新启用。换句话说，我们通常希望在处理一个中断时不会被另一个中断打断。

Below is a screenshot of Chapter 8.2.1 in this CPU document, which describes more details.

下图是该 CPU 文档第 8.2.1 节的截图，其中描述了更多细节。

> [!NOTE]
> TIP

>
> Take your time to read the screenshot above. It is a very important part of how operating systems work. We will revisit this screenshot in P3, especially its descriptions of privilege mode.

>
> 提示

>
> 花点时间阅读上面的截图。这是操作系统工作原理中非常重要的部分。我们将在 P3 中重新讨论这张截图，特别是其中关于特权模式（privilege mode）的描述。

## Preemptive scheduling

> **抢占式调度**

The goal of this project is to help you understand preemptive scheduling, which applies timer interrupts in process scheduling. As mentioned earlier, we do not distinguish between threads and processes for now, so we will use the two terms interchangeably.

本项目的目标是帮助你理解抢占式调度（preemptive scheduling），它将定时器中断应用于进程调度。如前所述，我们目前暂不区分线程和进程，因此我们将交替使用这两个术语。

The code for timer interrupts in egos-2000 is in earth/`cpu_intr.c`, which is basically the same as the timer demo program above but registers `trap_entry` as the handler function. The `trap_entry` in grass/`kernel.s` is the starting point of preemptive scheduling.

egos-2000 中定时器中断的代码位于 `earth/cpu_intr.c`，它基本上与上面的定时器示例程序相同，不同之处在于它将 `trap_entry` 注册为中断处理函数。`grass/kernel.s` 中的 `trap_entry` 是抢占式调度的起始点。

### Save and restore registers

> **保存与恢复寄存器**

At first glance, most of the code in `trap_entry` looks similar to the `SAVE_ALL_REGISTERS` and `RESTORE_ALL_REGISTERS` in P1. Indeed, the high-level goal is the same: save the values of all general-purpose registers onto the stack when a thread yields, and restore the values when switching back to this thread.

乍一看，`trap_entry` 中的大部分代码看起来类似于 P1 中的 `SAVE_ALL_REGISTERS` 和 `RESTORE_ALL_REGISTERS`。事实上，总体目标是一致的：当线程放弃 CPU 时将所有通用寄存器的值保存到栈上，并在切换回该线程时恢复这些值。

```assembly
trap_entry:
    csrw mscratch, sp
    li sp, 0x80200000
    addi sp, sp, -128
    SAVE_ALL_REGISTERS    /* Save registers for the current thread. */
    call kernel_entry     /* Pick the next thread. */
    RESTORE_ALL_REGISTERS /* Restore registers for the next thread. */
    mret
```

There are two differences. First, egos-2000 saves the registers on the operating system's stack, which starts from `0x80200000`. Specifically, the 128-byte region below `0x80200000` is used to save the registers, indicated by the `li` and `addi` instructions above. Since they modify the `sp` register, the old value of `sp` is written to a CSR called `mscratch` and later read back in the `SAVE_ALL_REGISTERS` part, right before saving its value on the stack.

这里有两个区别。首先，egos-2000 将寄存器保存到操作系统的栈上，该栈从 `0x80200000` 开始。具体而言，`0x80200000` 下方的 128 字节区域用于保存寄存器，如上面的 `li` 和 `addi` 指令所示。由于它们修改了 `sp` 寄存器，因此 `sp` 的旧值被写入一个名为 `mscratch` 的 CSR，随后在 `SAVE_ALL_REGISTERS` 宏中将其重新读出并保存到栈上。

The second difference is that `mret` is used at the end of `trap_entry` instead of `ret`. In P1, threads need to explicitly call `thread_yield`, which further calls `ctx_switch`, so the value of register `ra` saved on the stack by `ctx_switch` is an address in `thread_yield`. With this value of `ra`, `ctx_switch` can return to `thread_yield` simply by calling `ret`. In P2, threads are preempted by a timer interrupt rather than voluntarily calling a yield function. Therefore, `mepc` and `mret` are used to save and restore the program counter.

第二个区别是在 `trap_entry` 的末尾使用的是 `mret` 而不是 `ret`。在 P1 中，线程需要显式调用 `thread_yield`，它会进一步调用 `ctx_switch`，因此由 `ctx_switch` 保存到栈上的寄存器 `ra` 的值是 `thread_yield` 中的一个地址。有了这个 `ra` 值，`ctx_switch` 只需调用 `ret` 即可返回到 `thread_yield`。在 P2 中，线程是被定时器中断抢占的，而不是主动调用 `yield` 函数。因此，需要使用 `mepc` 和 `mret` 来保存和恢复程序计数器。

The `kernel_entry` highlighted above is in grass/`kernel.c`, and it does 3 things:
> * Copy `mepc` and the saved registers of the current thread into its process control block, a data structure similar to the thread control block in P1.

* Pick the next thread by calling `proc_yield`, a function similar to `thread_yield` in P1.
> * Restore `mepc` and the saved registers for the next thread from its process control block.

上面高亮显示的 `kernel_entry` 位于 `grass/kernel.c` 中，它做了 3 件事：
> * 将当前线程的 `mepc` 和保存的寄存器复制到其进程控制块（process control block）中，进程控制块是类似于 P1 中线程控制块的数据结构。

* 通过调用 `proc_yield` 选择下一个线程，该函数类似于 P1 中的 `thread_yield`。
> * 从下一个线程的进程控制块中恢复其 `mepc` 和保存的寄存器。

### Process control block

> **进程控制块**

In addition to the `mepc` and `saved_registers` highlighted below, the process control block of egos-2000 is defined in grass/`process.h`, which contains a few other fields:
> * a process identifier pid

* a data structure for system calls, which will be explained in P3
> * the status of a process, which is closely related to the lifecycle of a process

除了下面高亮显示的 `mepc` 和 `saved_registers` 之外，egos-2000 的进程控制块在 `grass/process.h` 中定义，包含以下其他几个字段：
> * 进程标识符 `pid`

* 用于系统调用的数据结构，将在 P3 中解释
> * 进程的状态，与进程的生命周期密切相关

```c
struct process {
    int pid;
    struct syscall syscall;
    enum proc_status status;
    uint mepc, saved_registers[32];
};
```

The lifecycle of a process is illustrated by the enum `proc_status` in grass/`process.h`.

进程的生命周期由 `grass/process.h` 中的 `enum proc_status` 说明。

```c
enum proc_status {
    PROC_UNUSED,
    PROC_LOADING,
    PROC_READY,
    PROC_RUNNING,
    PROC_RUNNABLE,
    PROC_PENDING_SYSCALL  /* We will explain this one in P3. */
};
```

Specifically, the status in a `struct process` would experience the following stages:
> * When creating a new process, the status is set from `PROC_UNUSED` to `PROC_LOADING`. This happens in the `proc_alloc` function in grass/`process.c`.

* The `elf_load` function in library/elf/`elf.c` loads the code and data for the process from disk into memory, and then the status is set from `PROC_LOADING` to `PROC_READY`. We will explain the details of `elf_load` in P4, and you can ignore it for now.
> * After loading the code and data, the `proc_yield` in grass/`kernel.c` will schedule this new process and set the status from `PROC_READY` to `PROC_RUNNING`.

* Upon a timer interrupt, the control flow enters `proc_yield` again, and if it picks another process to run next, the status of the current process will be set from `PROC_RUNNING` to `PROC_RUNNABLE`. The status then alternates between `PROC_RUNNABLE` and `PROC_RUNNING` because `proc_yield` is executed on every timer interrupt.
> * When a process terminates, the `proc_free` function in grass/`process.c` is called, which sets the status back to `PROC_UNUSED`.

具体而言，`struct process` 中的状态会经历以下阶段：
> * 创建新进程时，状态从 `PROC_UNUSED` 设置为 `PROC_LOADING`。这发生在 `grass/process.c` 中的 `proc_alloc` 函数里。

* `library/elf/elf.c` 中的 `elf_load` 函数将进程的代码和数据从磁盘加载到内存中，然后状态从 `PROC_LOADING` 设置为 `PROC_READY`。我们将在 P4 中解释 `elf_load` 的细节，目前你可以忽略它。
> * 在加载代码和数据后，`grass/kernel.c` 中的 `proc_yield` 会调度该新进程，并将状态从 `PROC_READY` 设置为 `PROC_RUNNING`。

* 发生定时器中断时，控制流再次进入 `proc_yield`，如果它选择另一个进程接下来运行，当前进程的状态将从 `PROC_RUNNING` 设置为 `PROC_RUNNABLE`。由于每次定时器中断都会执行 `proc_yield`，因此状态随后会在 `PROC_RUNNABLE` 和 `PROC_RUNNING` 之间交替切换。
> * 当进程终止时，会调用 `grass/process.c` 中的 `proc_free` 函数，将状态重置回 `PROC_UNUSED`。

These bullets help you understand the lifecycle of a process. Your first task in this project is to collect statistics on each process's lifecycle.

上述几点有助于你理解进程的生命周期。你在本项目中的第一个任务是收集每个进程生命周期的统计数据。

### Collect lifecycle statistics

> **收集生命周期统计信息**

Add new fields in `struct process` and update these fields at the right places, so you can collect the following statistics for every process.
* turnaround time: the time between process creation and termination
* response time: the time between process creation and the first time scheduled
* CPU time: the accumulated time that the process is actually running on the CPU
* number of timer interrupts encountered: how many times the CPU control flow transfers from the process code to `trap_entry` due to a timer interrupt

在 `struct process` 中添加新字段并在适当的位置更新这些字段，以便收集每个进程的以下统计数据。
* 周转时间（turnaround time）：进程创建到终止之间的时间
* 响应时间（response time）：进程创建到第一次被调度运行之间的时间
* CPU 时间（CPU time）：进程实际在 CPU 上运行的累计时间
* 遇到的定时器中断次数（number of timer interrupts encountered）：由于定时器中断，CPU 控制流从进程代码转移到 `trap_entry` 的次数

Process creation, termination, and first scheduling correspond to bullet points 1, 5, and 3 in the explanation of process lifecycles above. To measure time, call the `mtime_get` function twice, and the difference of the two return values is the time elapsed in 10^-7 seconds (on QEMU). Note that `mtime_get` is also the function used by the timer demo program.

进程的创建、终止和首次调度分别对应上述进程生命周期说明中的第 1、5 和 3 个要点。要测量时间，请调用 `mtime_get` 函数两次，两个返回值的差值就是流逝的时间（在 QEMU 上单位为 $10^{-7}$ 秒）。注意，`mtime_get` 也是定时器示例程序所使用的函数。

Print the statistics when a process terminates. Note that the ms below is millisecond (10^-7 * 10000 second), and you can simply assume that the numbers printed will not overflow the int type (i.e., you can use %d within printf or INFO).

在进程终止时打印统计信息。请注意，下面的 ms 是毫秒（$10^{-7} \times 10000$ 秒），并且你可以简单地假设打印的数字不会超出 `int` 类型的范围（即你可以在 `printf` 或 `INFO` 中使用 `%d`）。

```
➜ /home/yunhao echo Hello, World!
Hello, World!
[INFO] process 6 terminated after 0 timer interrupts, turnaround time: 17ms, response time: 13ms, CPU time: 1ms
➜ /home/yunhao ls
./     ../     README
[INFO] process 7 terminated after 0 timer interrupts, turnaround time: 20ms, response time: 16ms, CPU time: 0ms
➜ /home/yunhao loop 500 silent
[INFO] process 8 terminated after 94 timer interrupts, turnaround time: 1188ms, response time: 19ms, CPU time: 1167ms
➜ /home/yunhao loop 500
loop #0
loop #1
...
loop #499
[INFO] process 9 terminated after 0 timer interrupts, turnaround time: 2160ms, response time: 18ms, CPU time: 1506ms
```

> [!NOTE]
> TIP

>
> Try to explain the different statistics between loop 500 and loop 500 silent. For example, why does loop 500 encounter no timer interrupts and loop 500 silent encounter 94, although the CPU time is close? Note that QEMU can run faster or slower on different machines, so you may see time statistics that differ from the above. The above statistics are generated by running egos-2000 on a MacBook Air (M2, 2022) with QEMU v7.2.5 from xPack.

>
> 提示

>
> 尝试解释 `loop 500` 和 `loop 500 silent` 之间不同的统计数据。例如，为什么 `loop 500` 没有遇到定时器中断，而 `loop 500 silent` 遇到了 94 次，尽管两者的 CPU 时间相近？请注意，QEMU 在不同的机器上可能运行得更快或更慢，因此你可能会看到与上述不同的时间统计数据。上述统计数据是通过在 MacBook Air (M2, 2022) 上使用来自 xPack 的 QEMU v7.2.5 运行 egos-2000 生成的。

## Multilevel feedback queue

> **多级反馈队列**

Your final task in this project is to implement the notion of process priority in the scheduler, which is also known as the MultiLevel Feedback Queue (MLFQ) scheduler. Specifically, you need to implement a variant of MLFQ with simple rules:
* There are 5 levels, level #0 to #4. All processes start at level #0 when created.
* Processes at level #i can run on the CPU for (i+1)*100 milliseconds. For example, if a process at level #0 has run on the CPU for more than 100ms, it will move to level #1. After running for another 200ms, it will move to level #2. Since there are 5 levels, all processes at level #4 will remain there and not move down.
* The scheduler in `proc_yield` should always choose a runnable process with the lowest level number (i.e., highest priority) as the next process.
* Every 10 seconds or so, the scheduler in `proc_yield` should move all processes to level #0. In addition, if there is keyboard input for the shell, move `GPID_SHELL` to level #0.

你在本项目中的最终任务是在调度器中实现进程优先级的概念，这也被称为多级反馈队列（MultiLevel Feedback Queue, MLFQ）调度器。具体而言，你需要实现一个具有简单规则的 MLFQ 变体：
* 共有 5 个层级，即层级 #0 到 #4。所有进程在创建时都从层级 #0 开始。
* 层级 #i 的进程可以在 CPU 上运行 `(i+1)*100` 毫秒。例如，如果层级 #0 的进程在 CPU 上运行了超过 100ms，它将移动到层级 #1。再运行 200ms 后，它将移动到层级 #2。由于共有 5 个层级，层级 #4 的所有进程将停留在该层级且不会再向下移动。
* `proc_yield` 中的调度器应始终选择层级编号最低（即优先级最高）的可运行进程作为下一个进程。
* 大约每隔 10 秒，`proc_yield` 中的调度器应将所有进程移动到层级 #0。此外，如果有针对 shell 的键盘输入，将 `GPID_SHELL` 移动到层级 #0。

Some definitions for MLFQ have been provided in egos-2000:

egos-2000 中已经提供了一些关于 MLFQ 的定义：

```c
#define MLFQ_NLEVELS          5
#define MLFQ_RESET_PERIOD     100000000         /* 10 seconds */
#define MLFQ_LEVEL_RUNTIME(x) (x + 1) * 1000000 /* e.g., 100ms for level 0 */

void mlfq_update_level(struct process* p, ulonglong runtime) {
    /* Your code goes here. */
}

void mlfq_reset_level() {
    if (!earth->tty_input_empty()) {
       /* Your code goes here. */
    }
    static ulonglong MLFQ_last_reset_time = 0;
    /* Your code goes here. */
}
```

You can implement the MLFQ scheduler by implementing the two functions above and calling these functions in `proc_yield`. Specifically, add new fields to the `struct process` recording the level and the remaining runtime at that level. Suppose a process p has run for another runtime unit of time: `mlfq_update_level` updates the new fields according to rule #2. When providing the runtime argument, reuse your CPU time measurement.

你可以通过实现上述两个函数并在 `proc_yield` 中调用它们来实现 MLFQ 调度器。具体来说，向 `struct process` 添加新字段，记录进程的层级以及在该层级剩余的运行时间。假设进程 `p` 又运行了 `runtime` 个时间单位：`mlfq_update_level` 会根据规则 #2 更新这些新字段。在提供 `runtime` 参数时，请复用你测量的 CPU 时间。

The `mlfq_reset_level` function moves all processes to level #0 according to rule #4. Use earth->`tty_input_empty()` to see whether the shell gets new keyboard input, and use the `MLFQ_last_reset_time` variable to check whether `MLFQ_RESET_PERIOD` time units have elapsed since the last time moving all processes to level #0.

`mlfq_reset_level` 函数根据规则 #4 将所有进程移动到层级 #0。使用 `earth->tty_input_empty()` 查看 shell 是否获得了新的键盘输入，并使用 `MLFQ_last_reset_time` 变量检查自上次将所有进程移动到层级 #0 以来是否已经流逝了 `MLFQ_RESET_PERIOD` 个时间单位。

Lastly, you need to modify the for loop in `proc_yield()` according to rule #3. Make sure to call `proc_try_syscall()` for processes with status `PROC_PENDING_SYSCALL`, as in the current code. After you finish, test your code with the shell commands below.

最后，你需要根据规则 #3 修改 `proc_yield()` 中的 `for` 循环。确保像当前代码一样，对状态为 `PROC_PENDING_SYSCALL` 的进程调用 `proc_try_syscall()`。完成后，使用以下 shell 命令测试你的代码。

```
> make qemu
...
[CRITICAL] Welcome to the egos-2000 shell!
➜ /home/yunhao loop &
[INFO] process 6 running in the background
➜ /home/yunhao loop &
[INFO] process 7 running in the background
➜ /home/yunhao ls
./     ../     README
➜ /home/yunhao killall
➜ /home/yunhao ls
./     ../     README
```

You can certainly run the commands above with the current scheduler and with your MLFQ scheduler. With the current scheduler, the first ls would take a long time because this ls is interleaved with the background loops. After the killall, ls would become fast again. With your MLFQ scheduler, the two background loops should quickly move to lower priority, so the first ls should be fast just like the second ls. Confirm such an improvement made by MLFQ using the turnaround time printed when ls terminates.

你当然可以在当前调度器和你的 MLFQ 调度器下运行上述命令。在使用当前调度器时，第一次 `ls` 会耗费很长时间，因为该 `ls` 与后台的 `loop` 交替运行。在 `killall` 之后，`ls` 会再次变快。在使用你的 MLFQ 调度器时，两个后台 `loop` 应该会迅速移动到较低优先级，因此第一次 `ls` 应该像第二次 `ls` 一样快。请利用 `ls` 终止时打印的周转时间来确认 MLFQ 所带来的这一改进。

## Accomplishments

> **总结与成就**

You have started learning about control and status registers, which are key CPU support for operating systems. You will see more CSRs in the next few projects. You have also finished reading earth/`cpu_intr.c`, grass/`kernel.s`, and half of grass/`kernel.c` in egos-2000.

你已经开始学习控制与状态寄存器（CSR），它们是操作系统所需的关键 CPU 硬件支持。在接下来的几个项目中，你将看到更多的 CSR。你还完成了 egos-2000 中 `earth/cpu_intr.c`、`grass/kernel.s` 以及半个 `grass/kernel.c` 的代码阅读。
