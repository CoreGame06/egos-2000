# Multicore & Locks

> **多核与锁**

By far, we have been running the operating system and user applications on a single CPU core, but multicore CPUs are widely used. A laptop typically has a CPU with 4 to 16 cores, whereas a server CPU in a data center can have 64 or more cores. Multicore systems deliver higher software performance because multiple processes run on different cores simultaneously, enabling software tasks to make progress in parallel.

到目前为止，我们一直在单个 CPU 核心上运行操作系统和用户应用程序，但多核 CPU 在当下已被广泛使用。一台笔记本电脑的 CPU 通常包含 4 到 16 个核心，而数据中心里的服务器 CPU 则可能拥有 64 个或更多核心。多核系统能够提供更高的软件性能，因为多个进程可以同时在不同的核心上运行，从而使软件任务能够并行推进。

Handling multiple cores involves an important concept called mutual exclusion. In general, mutual exclusion prevents multiple CPU cores from using a shared resource simultaneously. For example, when the kernel code is running on a core and updating a data structure (e.g., the PCB), it may need exclusive access to that structure. Otherwise, kernel data structures can be corrupted by simultaneous updates by multiple cores. We thus start by introducing mutual exclusion.

处理多核涉及一个重要的概念——互斥（mutual exclusion）。通常来说，互斥旨在防止多个 CPU 核心同时使用同一个共享资源。例如，当内核代码在某个核心上运行并更新某个数据结构（例如 PCB）时，它可能需要对该数据结构拥有独占访问权。否则，多个核心同时更新可能会损坏内核数据结构。因此，我们首先从介绍互斥开始。

## Mutual exclusion

> **互斥**

When you run egos-2000 on QEMU, you may have noticed that the first line printed is one of the four possibilities listed below. In other words, a core is randomly selected from #0 .. #3 to boot egos-2000 when you run it on QEMU.

当你在 QEMU 上运行 egos-2000 时，你可能已经注意到打印出的第一行是下面列出的四种可能性之一。换句话说，当你在 QEMU 上运行 egos-2000 时，会从 #0 .. #3 中随机选择一个核心来引导 egos-2000。

```
[CRITICAL] --- Booting on QEMU with core #0 ---
[CRITICAL] --- Booting on QEMU with core #1 ---
[CRITICAL] --- Booting on QEMU with core #2 ---
[CRITICAL] --- Booting on QEMU with core #3 ---
```

This is a typical example of mutual exclusion. Specifically, the public egos-2000 code only runs on a single core, so the booting code (aka. boot loader) will prevent the other 3 cores from running egos-2000 after egos-2000 has already started to run on a core. Such mutual exclusion requires a new class of CPU instructions, atomic memory operations, which is an extension to the basic RISC-V instruction set architecture.

这是互斥的一个典型示例。具体而言，公开发布的 egos-2000 代码仅在单个核心上运行，因此在 egos-2000 已经在某个核心上开始运行后，引导代码（即 boot loader）将阻止其余 3 个核心运行 egos-2000。这种互斥需要一类新的 CPU 指令，即原子内存操作（atomic memory operations），它是基础 RISC-V 指令集架构的扩展。

### Atomic memory operation

> **原子内存操作**

The assembly code below is from earth/`boot.s` and shows the first instructions that all 4 CPU cores will execute when running egos-2000.

下面的汇编代码来自 earth/`boot.s`，展示了所有 4 个 CPU 核心在运行 egos-2000 时将要执行的第一批指令。

```assembly
boot_loader:
    la t0, boot_lock          /* Load the address of boot_lock. */
    li t1, 1
    amoswap.w.aq t1, t1, (t0) /* Acquire boot_lock. */
    bnez t1, boot_loader
    li sp, 0x80200000
    call boot
.bss
    boot_lock:       .word 0
    booted_core_cnt: .word 0
```

Line 9 indicates that there is a 4-byte variable named `boot_lock` initialized to 0 before all cores execute the first instruction (i.e., the la instruction in `boot_loader`). The first two instructions load the address of `boot_lock` into register `t0` and the value 1 into register `t1`. The magic happens at the `amoswap.w.aq` instruction, where amo stands for atomic memory operation. It atomically swaps the value of `t1` with the 4 bytes at address `t0`. Atomicity means that only one of the 4 CPU cores would swap back the initial value 0 of `boot_lock`, after which all the other cores will swap back the value 1.

代码中的第 9 行表明，在所有核心执行第一条指令（即 `boot_loader` 中的 la 指令）之前，就存在一个初始化为 0 的 4 字节变量 `boot_lock`。前两条指令将 `boot_lock` 的地址加载到寄存器 `t0` 中，并将值 1 加载到寄存器 `t1` 中。神奇之处在于 `amoswap.w.aq` 指令，其中 amo 代表原子内存操作（atomic memory operation）。它原子地将 `t1` 的值与地址 `t0` 处的 4 字节数据进行交换。原子性意味着在 4 个 CPU 核心中，只有一个核心能换回 `boot_lock` 的初始值 0，在此之后，所有其他核心都将换回值 1。

The first core to complete `amoswap.w.aq` will call `boot()`, and as long as `boot_lock` continues to hold the value 1, the other cores will be trapped into an infinite loop due to the bnez instruction at line 5.

第一个完成 `amoswap.w.aq` 的核心将调用 `boot()`，只要 `boot_lock` 继续保持值 1，由于第 5 行的 bnez 指令，其他核心就会陷入死循环中。

> [!TIP]
> The magic of `amoswap.w.aq` is enforced by the CPU hardware design. The CPU hardware determines which core executes this instruction first when multiple cores attempt to execute it simultaneously. This is closely related to the memory coherence problem in computer architecture.

>
> `amoswap.w.aq` 的神奇之处是由 CPU 硬件设计强制保证的。当多个核心尝试同时执行该指令时，CPU 硬件决定哪个核心首先执行该指令。这与计算机体系结构中的内存一致性（memory coherence）问题密切相关。

We say that the code above acquires the boot lock, and one can release it by writing 0 to the address of `boot_lock`. After releasing the boot lock, another core would be able to acquire it and proceed with calling `boot()`.

我们称上述代码获取了引导锁（boot lock），可以通过向 `boot_lock` 的地址写入 0 来释放它。释放引导锁后，另一个核心将能够获取该锁并继续调用 `boot()`。

In general, a lock is simply a 4-byte variable in memory that holds either 0 or 1. Given a lock variable x in C, we have provided two macros for you in library/`egos.h`.

通常来说，锁只是内存中保存 0 或 1 的 4 字节变量。给定 C 语言中的锁变量 x，我们在 library/`egos.h` 中为你提供了两个宏：

```c
/* The __sync_lock_* functions are defined within the C compiler. */
#define release(x) __sync_lock_release(&x);
#define acquire(x) while (__sync_lock_test_and_set(&x, 1) != 0);
```

While you still need to write some assembly code in this project, you can use these macros whenever you need to acquire or release a lock in your C code. Now release the boot lock and see what happens.

虽然你在此项目中仍需要编写一些汇编代码，但只要在 C 代码中需要获取或释放锁，就可以使用这些宏。现在释放引导锁，看看会发生什么。

### A multicore boot loader

> **多核引导加载程序**

Since `booted_core_cnt` is initially 0, like `boot_lock`, the first booted core will enter the if branch in `boot()` and call 4 initialization functions: `tty_init`, `disk_init`, `mmu_init`, and `intr_init`. They initialize the TTY and disk devices, as well as the CSRs for virtual memory and interrupts. Lastly, `boot()` calls the `grass_entry` function in grass/`init.c`.

由于 `booted_core_cnt` 与 `boot_lock` 一样初始值为 0，首个启动的核心将进入 `boot()` 中的 if 分支并调用 4 个初始化函数：`tty_init`、`disk_init`、`mmu_init` 和 `intr_init`。它们初始化 TTY 和磁盘设备，以及用于虚拟内存和中断的 CSR。最后，`boot()` 调用 grass/`init.c` 中的 `grass_entry` 函数。

The `grass_entry` function loads the binary executable for apps/system/`sys_proc.c` into memory as the first process, and starts to run this process after the `mret` instruction. Note that after this `mret`, the first process will set the stack pointer to `0x80400000` as shown in apps/`app.s`. This means the first-booted core has finished using the kernel stack, so we can now safely release the boot lock and allow the next core to call `boot()`.

`grass_entry` 函数将 apps/system/`sys_proc.c` 的二进制可执行文件作为第一个进程加载到内存中，并在 `mret` 指令之后开始运行此进程。请注意，在此 `mret` 之后，如 apps/`app.s` 中所示，第一个进程将把栈指针设为 `0x80400000`。这意味着首个启动的核心已经使用完了内核栈，因此我们现在可以安全地释放引导锁，并允许下一个核心调用 `boot()`。

Release the boot lock at the start of the main function in apps/system/`sys_proc.c`. Note that `grass_entry()` has passed the address of `boot_lock` to this process as an argument for its main function, you can access `boot_lock` through the `struct multicore`* boot argument. Here is one possible printout after you release the boot lock.

在 apps/system/`sys_proc.c` 的 main 函数开头释放引导锁。请注意，`grass_entry()` 已将 `boot_lock` 的地址作为其 main 函数的参数传递给此进程，你可以通过 `struct multicore`* boot 参数访问 `boot_lock`。以下是释放引导锁后的一种可能打印输出：

```
> make qemu
...
[CRITICAL] --- Booting on QEMU with core #1 ---
...
[SUCCESS] Enter kernel process GPID_PROCESS
[SUCCESS] --- Core #4[INFO] Load kern el process #2: sys_tserminal
tarts running ---
[INFO] Load 0xde8 bytes to 0x80200000
[INFO] Load 0x11c bytes to 0x80208000
[SUCCESS] Enter kernel process GPID_TERMINAL
...
```

Essentially, the first process prints out Enter kernel process `GPID_PROCESS` normally, but the printing of [INFO] Load kernel process #2: `sys_terminal` is mixed with the printing of [SUCCESS] --- Core #4 starts running --- in earth/`boot.c`. This is a clear result of core #1 running the first process while core #4 runs `boot()` in parallel.

本质上，第一个进程正常打印出 Enter kernel process `GPID_PROCESS`，但 [INFO] Load kernel process #2: `sys_terminal` 的打印与 earth/`boot.c` 中 [SUCCESS] --- Core #4 starts running --- 的打印交织在了一起。这显然是核心 #1 运行第一个进程的同时核心 #4 并行运行 `boot()` 的结果。

Next, we ask you to complete the boot loader code for multicore. Start with a fresh copy of egos-2000 and incorporate your virtual memory code from P4.

接下来，我们要求你完成多核引导加载程序代码。从一份全新的 egos-2000 代码副本开始，并整合你在 P4 中编写的虚拟内存代码。

Your first task is to complete the code in apps/system/`sys_proc.c` and earth/`boot.c`, so all the cores finish booting. Specifically, the 3 cores that boot after the first one should set up their own CSRs for interrupts and virtual memory, but they do not reinitialize the devices. Note that each core owns its own set of CSRs. In particular, multicore requires page table translation, so simply do a FATAL in your code if `SOFT_TLB` has been chosen.

你的第一个任务是完成 apps/system/`sys_proc.c` 和 earth/`boot.c` 中的代码，使得所有核心完成引导。具体来说，在第一个核心之后启动的 3 个核心应该设置它们自己用于中断和虚拟内存的 CSR，但它们不需要重新初始化设备。注意每个核心都拥有自己的一组 CSR。特别是，多核需要页表转换，因此如果选择了 `SOFT_TLB`，只需在代码中触发 FATAL 即可。

The goal is to see [SUCCESS] --- Core #? starts running --- for all 3 cores, so we know that all 4 cores have booted. However, you will likely meet exceptions and FATAL in `excp_entry()` in the kernel since we have not yet protected the kernel code with locks.

目标是看到所有 3 个核心都输出 [SUCCESS] --- Core #? starts running ---，这样我们就知道所有 4 个核心都已启动。然而，你很可能会在内核的 `excp_entry()` 中遇到异常和 FATAL，因为我们尚未用锁保护内核代码。

### A multicore kernel

> **多核内核**

As we mentioned, if multiple CPU cores access the kernel stack or update kernel data structures simultaneously, these memory regions can be corrupted. Therefore, we define `kernel_lock` in grass/`kernel.s` to ensure that at any time only one core can access the kernel stack or data structures. Your job is to protect the kernel by acquiring and releasing the `kernel_lock`.

正如我们所提到的，如果多个 CPU 核心同时访问内核栈或更新内核数据结构，这些内存区域可能会被破坏。因此，我们在 grass/`kernel.s` 中定义了 `kernel_lock`，以确保在任何时候只有一个核心可以访问内核栈或数据结构。你的任务是通过获取和释放 `kernel_lock` 来保护内核。

Recall the `trap_entry` defined in grass/`kernel.s`, which is first introduced in P2. It is the entry point for all interrupts and exceptions, trapping a CPU core in the kernel. If multiple cores get trapped at the same time, they will all execute the instructions in `trap_entry`, thereby using the kernel stack at `0x80200000`.

回顾一下在 P2 中首次介绍的、定义在 grass/`kernel.s` 中的 `trap_entry`。它是所有中断和异常的入口点，用于将 CPU 核心陷入内核。如果多个核心同时陷入内核，它们都将执行 `trap_entry` 中的指令，从而使用位于 `0x80200000` 的内核栈。

Intuitively, you will need to acquire the kernel lock before switching to the kernel stack and release it right before the `mret` in `trap_entry`. There is one key difference from how this is done in earth/`boot.s`. Recall that `trap_entry` saves all the registers on the kernel stack before calling `kernel_entry`, and restores them before `mret`. This requires that your code for acquiring or releasing the lock does not modify the value of any registers. This can be achieved by using the so-called scratch CSRs such as `mscratch` and `sscratch`. For example, to preserve the value of the `sp` register, `trap_entry` already uses the `mscratch` CSR to record the old value of `sp`.

直观地说，你需要在切换到内核栈之前获取内核锁，并在 `trap_entry` 中的 `mret` 之前立即释放它。这与在 earth/`boot.s` 中的实现方式有一个关键区别。回忆一下，`trap_entry` 在调用 `kernel_entry` 之前会将所有寄存器保存到内核栈上，并在 `mret` 之前恢复它们。这就要求你用于获取或释放锁的代码不能修改任何寄存器的值。这可以通过使用所谓的暂存 CSR（如 `mscratch` 和 sscratch）来实现。例如，为了保留 `sp` 寄存器的值，`trap_entry` 已经使用了 `mscratch` CSR 来记录 `sp` 的旧值。

After your modifications to grass/`kernel.s`, there is only one more thing you need to do in grass/`kernel.c`. Given multiple CPU cores, it is possible that a core must remain idle, i.e., the scheduler cannot find a RUNNABLE process to run on it. As a result, we need to ask an idle core to do nothing before it gets the next timer interrupt. You have seen this situation when implementing sleep in P3, and you need to handle it again in `proc_yield`.

对 grass/`kernel.s` 进行修改后，在 grass/`kernel.c` 中你只需要再做一件事。在存在多个 CPU 核心的情况下，某个核心可能必须保持空闲，即调度器找不到可以在其上运行的 RUNNABLE 进程。因此，我们需要让空闲的核心在收到下一个定时器中断之前什么也不做。你在 P3 实现 sleep 时已经见过这种情况，在 `proc_yield` 中你需要再次处理它。

> [!TIP]
> We use a single lock to protect the whole kernel because it is simple. In many operating systems, there are separate kernel stacks for different CPU cores, so they don't need to be protected by locks. There are also different locks protecting the other kernel data structures.

>
> 我们使用单个锁来保护整个内核是因为它足够简单。在许多操作系统中，不同的 CPU 核心拥有独立的内核栈，因此不需要由锁来保护它们。同时还有不同的锁用来保护其他内核数据结构。

## Running processes in parallel

> **并行运行进程**

Now, let us run multiple processes on different CPU cores and see whether our kernel can indeed handle multicore scheduling. You will implement the `proc_coresinfo` function at the end of grass/`kernel.c` and add this function into `struct grass`, so the shell can call this function for its built-in command coresinfo (see hints in apps/system/`sys_shell.c`). After you finish, run multiple processes in the background and try coresinfo.

现在，让我们在不同的 CPU 核心上运行多个进程，看看我们的内核是否确实能够处理多核调度。你将在 grass/`kernel.c` 末尾实现 `proc_coresinfo` 函数，并将该函数添加到 `struct grass` 中，以便 shell 能够为其内置命令 coresinfo 调用此函数（参见 apps/system/`sys_shell.c` 中的提示）。完成后，在后台运行多个进程并尝试运行 coresinfo。

```
> make qemu
...
[CRITICAL] Welcome to the egos-2000 shell!
➜ /home/yunhao loop &
[INFO] process 6 running in the background
➜ /home/yunhao loop &
[INFO] process 7 running in the background
➜ /home/yunhao loop &
[INFO] process 8 running in the background
➜ /home/yunhao loop &
[INFO] process 9 running in the background
➜ /home/yunhao coresinfo
[INFO] ==============Core ID / Process ID==============
[INFO] Core #1 is running pid=6
[INFO] Core #2 is running pid=4 (GPID_SHELL)
[INFO] Core #3 is running pid=7
[INFO] Core #4 is running pid=9
➜ /home/yunhao coresinfo
[INFO] ==============Core ID / Process ID==============
[INFO] Core #1 is running pid=8
[INFO] Core #2 is running pid=9
[INFO] Core #3 is running pid=4 (GPID_SHELL)
[INFO] Core #4 is running pid=7
...
```

In this demo, we started 4 processes in the background, and if we run coresinfo multiple times, different cores would be running different processes. For the first coresinfo above, processes #6, #7, and #9 were running, while #8 was not (i.e., RUNNABLE). You can also try the built-in killall command, after which all background loops will terminate. Here is a video with more details of this demo.

在此演示中，我们在后台启动了 4 个进程，如果多次运行 coresinfo，不同的核心将运行不同的进程。对于上面第一次运行 coresinfo，进程 #6、#7 和 #9 正在运行，而 #8 没有运行（即处于 RUNNABLE 状态）。你还可以尝试内置的 killall 命令，之后所有后台循环都会终止。有关此演示的更多细节请参阅随附视频。

## Finding concurrency bugs

> **查找并发缺陷**

Being able to run a demo does not mean your code is bug-free. Concurrency bugs occur when multiple programs run concurrently. Here, a concurrency bug could be programs running on different cores modifying a certain kernel data structure at the same time. While we have protected the kernel with the kernel lock, such concurrency bugs still exist. See whether you can find some, trigger them, and fix them.

能够运行演示并不意味着你的代码就没有 Bug 了。并发 Bug（concurrency bugs）发生在多个程序并发运行时。在这里，并发 Bug 表现为运行在不同核心上的程序同时修改了某个内核数据结构。虽然我们已经用内核锁保护了内核，但此类并发 Bug 依然可能存在。看看你是否能找到它们、触发它们并予以修复。

一般来说，并发 Bug 很难被发现和调试，因为它们通常既不是确定性发生的，也不常发生。考虑到 egos-2000 代码库较小且使用了简单的数据结构，消除并发 Bug 的好方法是对所有数据结构的使用逻辑进行严密推导与分析。

> [!TIP]
> If you wish to learn more about parallel programming in operating systems, this book is a fun read: Is Parallel Programming Hard, And, If So, What Can You Do About It?  Also, if you run your code on the Arty board, you need to add a few nops after each amoswap instruction. The reason is that the Arty board's CPU design has a flaw: it cannot complete the amoswap instruction in a single CPU cycle. By adding a few nops, the CPU core waits for amoswap to complete before proceeding into a critical section. The Tang Nano 20K board does not currently support multicore.

>
> 如果你想了解更多关于操作系统中并行编程的知识，这本书读起来很有趣：《Is Parallel Programming Hard, And, If So, What Can You Do About It?》（并行编程难吗？如果难，你能做些什么？）。此外，如果你在 Arty 开发板上运行代码，需要在每条 amoswap 指令后添加若干 nop。原因是 Arty 板的 CPU 设计存在缺陷：它无法在单个 CPU 周期内完成 amoswap 指令。通过添加一些 nop，CPU 核心会在进入临界区之前等待 amoswap 完成。Tang Nano 20K 开发板目前不支持多核。

## Multithreading on multicore

> **多核上的多线程**

Let's start by revisiting the producer-consumer code in P1.

让我们首先重温一下 P1 中的生产者-消费者代码：

```c
void produce(void* item) {
    for (int i = 0; i < 10; i++) {
        while (count == 3) cv_wait(&nonfull);
        // At this point, the buffer is not full.
        buffer[tail] = item;
        tail = (tail + 1) % 3;
        count += 1;
        cv_signal(&nonempty);
    }
}

void consume() {
    while (1) {
        while (count == 0) cv_wait(&nonempty);
        // At this point, the buffer is not empty.
        void* result = buffer[head];
        head = (head + 1) % 3;
        count -= 1;
        cv_signal(&nonfull);
    }
}
```

In P1, the producer and consumer functions run as two threads. They run on the same CPU core, which means a producer can read the count variable without worrying that it is simultaneously modified by another thread. If we allow threads to run on multiple cores, we will then have to protect the count variable with a lock.

在 P1 中，生产者和消费者函数作为两个线程运行。它们在同一个 CPU 核心上运行，这意味着生产者在读取 count 变量时，无需担心它同时被另一个线程修改。如果我们允许线程在多个核心上运行，我们就必须用锁保护 count 变量。

Indeed, in a programming language like C++, a lock must be acquired before calling the conditional variable's `cv_wait` function. When running such a C++ program, the program would spawn multiple threads, and the OS would schedule them on different CPU cores. Your job is to implement a `thread_create` system call which allows a user application to spawn child threads. You can start by spawning threads that only print a few times, just like in P1, and you may reuse the exit function in library/syscall/`servers.c` when a thread terminates. Make sure that your kernel can schedule multiple threads on different cores.

事实上，在像 C++ 样的编程语言中，在调用条件变量的 `cv_wait` 函数之前必须先获取锁。在运行这样的 C++ 程序时，程序会派生（spawn）多个线程，而操作系统会将它们调度到不同的 CPU 核心上。你的任务是实现一个 `thread_create` 系统调用，该系统调用允许用户应用程序派生子线程。你可以从派生仅打印几次信息的线程开始（就像在 P1 中那样），当线程终止时可以复用 library/syscall/`servers.c` 中的 exit 函数。确保你的内核可以在不同的核心上调度多个线程。

Next, implement the conditional variable interface functions, `cv_wait` and `cv_signal`, for this multithreaded application. This is different from P1 in two ways:

接下来，为这个多线程应用程序实现条件变量接口函数 `cv_wait` 和 `cv_signal`。这与 P1 在两方面有所不同：

* Each conditional variable now needs to associate with a lock.
> 每个条件变量现在都需要关联一个锁。

* Threads are scheduled by the kernel, so a conditional variable in a user application can no longer maintain a queue of `struct thread`.
> 线程由内核进行调度，因此用户应用中的条件变量不能再维持一个 `struct thread` 队列。

We leave the design of the conditional variable interface open-ended; feel free to design it yourself. You can refer to the POSIX interface for multithreading and conditional variables (i.e., `pthread_cond_t`). The goal is to run the producer and consumer functions as two threads within a single multithreaded application.

我们对条件变量接口的设计保持开放；你可以自由设计。你可以参考 POSIX 的多线程和条件变量接口（即 `pthread_cond_t`）。目标是在单个多线程应用程序中将生产者和消费者函数作为两个线程运行。

## A multithreaded web server

> **多线程 Web 服务器**

In P7, we disabled multicore support because handling PLIC interrupts across multiple cores can be tricky. Now is the time to figure out how to manage PLIC on multicore systems. With your P7 code, change the -smp 1 back to -smp 4, and make sure that the kernel can still reliably receive Ethernet packets in the `intr_entry` function in grass/`kernel.c`.

在 P7 中，我们禁用了多核支持，因为跨多个核心处理 PLIC（平台级中断控制器）中断可能会比较复杂。现在是弄清楚如何在多核系统上管理 PLIC 的时候了。基于你的 P7 代码，将 -smp 1 改回 -smp 4，并确保内核仍能在 grass/`kernel.c` 中的 `intr_entry` 函数中可靠地接收以太网数据包。

As another open-ended part of this project, we ask you to integrate your driver code with the multithreaded producer-consumer application. Intuitively, the Ethernet driver should be a producer that receives network packets from the Ethernet interface and buffers them in memory, with a separate consumer thread deciding how to handle them. This is the typical architecture of a web server application.

作为本项目的另一个开放部分，我们要求你将驱动程序代码与多线程生产者-消费者应用程序集成。直观地讲，以太网驱动程序应该是一个生产者，它从以太网接口接收网络数据包并将其保存在内存缓冲区中，由一个独立的消费者线程决定如何处理它们。这是 Web 服务器应用程序的典型架构。

The OS concept we would like you to explore here is signal handling. Consider the SIGIO signal widely available on many operating systems. A web server can register a handler for the SIGIO signal via a special system call, which is not available in egos-2000 right now. When the kernel receives a PLIC interrupt from Ethernet, it should send a SIGIO to the web server, so the web server can run the registered handler after the kernel schedules it. This handler should then run the driver logic, decoding the receive descriptors and reading the Ethernet controller's receive buffers.

我们希望你在此探索的操作系统概念是信号处理（signal handling）。以许多操作系统上广泛提供的 SIGIO 信号为例。Web 服务器可以通过一个特殊的系统调用注册 SIGIO 信号的处理程序（handler），该系统调用目前在 egos-2000 中尚不可用。当内核收到来自以太网的 PLIC 中断时，它应该向 Web 服务器发送一个 SIGIO，以便 Web 服务器在内核对其进行调度后运行已注册的处理程序。然后，该处理程序应当运行驱动程序逻辑，解码接收描述符并读取以太网控制器的接收缓冲区。

With signal handling, an OS can avoid putting a large amount of I/O device-driver logic into the kernel. Device drivers run in processes instead.

通过信号处理，操作系统可以避免将大量的 I/O 设备驱动程序逻辑放入内核中。设备驱动程序改为在进程中运行。

## Accomplishments

> **本章小结**

You have gained some experience with atomic memory operations, an extension to the RISC-V instruction set for multicore systems. You have seen how multiple cores run code in parallel, resulting in interleaved printing, and how to protect the kernel with a lock. You have tried to find and fix concurrency bugs that may occur infrequently or be nondeterministic.

你已经获得了一些关于原子内存操作的经验，这是用于多核系统的 RISC-V 指令集扩展。你已经看到了多个核心如何并行运行代码并导致交织的打印输出，以及如何用锁来保护内核。你尝试了查找并修复那些发生频率较低或具有非确定性的并发 Bug。
