# System Call & Protection

> **系统调用与保护**

You have seen how an operating system manages the life cycles of threads (or processes, since we don't distinguish between them yet) and preemptively schedules multiple threads using timer interrupts. This project now helps you understand two more things.

你已经了解了操作系统如何管理线程（或进程，因为我们目前尚未对其进行区分）的生命周期，以及如何利用定时器中断来抢占式地调度多个线程。本项目将帮助你进一步理解另外两件事。

* How do threads invoke system calls to communicate with each other?
> * 线程如何通过系统调用相互通信？

* How does an operating system protect its memory, so malicious threads cannot corrupt the system calls by modifying the code or data of the operating system?
> * 操作系统如何保护自身的内存，从而防止恶意线程通过修改操作系统的代码或数据来破坏系统调用？

System calls and memory protection are supported by exception handling, which is similar to interrupt handling. To begin, we introduce exception handling.

系统调用和内存保护均由异常处理机制提供支持，异常处理与中断处理类似。首先，我们来介绍异常处理。

## Exception handling

> **异常处理**

An exception happens if something goes wrong when the CPU executes an instruction. For example, an exception occurs when an instruction attempts to access memory at an invalid address. Instead of ignoring the problem and proceeding to the next instruction, the CPU automatically jumps to a special function called an exception handler, just as it jumps to the interrupt handler when receiving a timer interrupt. We will use the same function to handle both interrupts and exceptions, and use the `mcause` CSR to show why the CPU jumped to the handler.

当 CPU 在执行指令的过程中发生错误时，就会触发异常（exception）。例如，当一条指令试图访问无效地址处的内存时就会发生异常。CPU 不会忽略该问题并继续执行下一条指令，而是会自动跳转到一个被称为异常处理程序（exception handler）的特殊函数中，这与收到定时器中断时跳转到中断处理程序类似。我们将使用同一个函数来处理中断和异常，并通过 `mcause` CSR（控制与状态寄存器）来指示 CPU 跳转到处理程序的原因。

### The `mcause` CSR

> **mcause CSR**

Below are screenshots of Tables 22 and 23 from this manual, which describe the `mcause` CSR. You can also read Chapter 3.1.15 of the RISC-V Reference Manual. When an exception or interrupt occurs, the CPU sets `mcause` before jumping to the handler.

下图展示了手册中描述 `mcause` CSR 的表 22 和表 23。你也可以阅读 RISC-V 参考手册的第 3.1.15 节。当异常或中断发生时，CPU 会在跳转到处理程序之前设置 `mcause` 的值。

For example, `mcause` is set to `0x80000007` when the CPU receives a timer interrupt. Bit#31 of `mcause` is set to 1 because the timer interrupt is an interrupt. The exception code is set to 0b0000000111 because 7 is the machine timer interrupt code. Similarly, when the CPU encounters an illegal instruction, meaning the 4 bytes pointed to by the program counter cannot be decoded into a CPU instruction, `mcause` is set to `0x2` before the CPU jumps to the handler.

例如，当 CPU 收到定时器中断时，`mcause` 会被设置为 `0x80000007`。`mcause` 的第 31 位（Bit#31）被设置为 1，因为定时器中断属于中断。异常代码（exception code）被设置为 0b0000000111，因为 7 是机器模式定时器中断（machine timer interrupt）的代码。类似地，当 CPU 遇到非法指令（即程序计数器指向的 4 个字节无法被解码为一条有效的 CPU 指令）时，在 CPU 跳转到处理程序之前，`mcause` 会被设置为 `0x2`。

TIP

提示

Exceptions are different from interrupts. Exceptions are triggered by CPU instructions that cause something wrong. Interrupts are triggered by devices outside of the CPU, such as a timer, a disk, or a network interface controller. The `mcause` CSR helps the operating system understand which interrupt or exception needs to be handled.

异常与中断不同。异常是由引起错误的 CPU 指令触发的；而中断则是由 CPU 外部的设备（如定时器、磁盘或网络接口控制器）触发的。`mcause` CSR 能够帮助操作系统了解当前需要处理的是哪种中断或异常。

### The `ecall` exception

> **ecall 异常**

Most exceptions occur due to errors, but RISC-V provides a special instruction, `ecall`, which intentionally triggers the so-called environment call exception (see exception #8 and #11 in Table 23). This instruction is for system calls: when a thread invokes `ecall`, the control flow transfers to the operating system, which serves a system call for that thread.

大多数异常都是由错误引起的，但 RISC-V 提供了一条特殊的指令 `ecall`，它会主动触发所谓的环境调用异常（environment call exception，参见表 23 中的异常 #8 和 #11）。该指令专门用于系统调用：当线程执行 `ecall` 时，控制流会转移给操作系统，由操作系统为该线程提供系统调用服务。

You can find `ecall` in library/syscall/`syscall.c` of egos-2000. After asm("ecall") raises an environment call exception, the CPU jumps to `trap_entry`, as you saw in P2. `trap_entry` calls `kernel_entry`, which calls `excp_entry`, and `excp_entry` handles system calls in this condition: (id >= `EXCP_ID_ECALL_U` && id <= `EXCP_ID_ECALL_M`). `EXCP_ID_ECALL_U` and `EXCP_ID_ECALL_M` are defined as 8 and 11, respectively, according to Table 23. Read `trap_entry` and `kernel_entry` yourself and see how `mcause` is used.

你可以在 egos-2000 的 `library/syscall/syscall.c` 中找到 `ecall`。在 `asm("ecall")` 触发环境调用异常后，CPU 会跳转到 `trap_entry`（正如你在 P2 中所见）。`trap_entry` 会调用 `kernel_entry`，后者再调用 `excp_entry`。`excp_entry` 在满足条件 `(id >= EXCP_ID_ECALL_U && id <= EXCP_ID_ECALL_M)` 时处理系统调用。根据表 23，`EXCP_ID_ECALL_U` 和 `EXCP_ID_ECALL_M` 分别被定义为 8 和 11。请自行阅读 `trap_entry` 和 `kernel_entry`，了解 `mcause` 是如何被使用的。

TIP

提示

The U-Mode and M-mode in Table 23 stand for user mode and machine mode. We will cover these privilege modes very soon when we start explaining memory protection later in this project.

表 23 中的 U-Mode 和 M-Mode 分别代表用户模式（user mode）和机器模式（machine mode）。在本项目的后续部分讲解内存保护时，我们很快就会介绍这些特权模式。

### A sketch of the "kernel"

> **“内核”的简要示意**

We have been using the term kernel since P2, but we have never explained what it is. Now that you know the `mcause` CSR, we can show you a sketch of the kernel.

自 P2 以来我们一直在使用“内核（kernel）”这个术语，但尚未解释过它的具体含义。既然你现在已经了解了 `mcause` CSR，我们可以向你展示内核的示意代码。

```c
void kernel() {
    int mcause_val, id;
    asm("csrr %0, mcause" : "=r"(mcause_val));
    id = mcause_val & 0x3FF;
    if (mcause_val & (1 << 31)) {
        if (id == 7) proc_yield();
    } else {
        if (id >= 8 && id <= 11) handle_system_call();
        if (id == 1 || id == 5 || id == 7) handle_memory_access_fault();
    }
}
```

The code above sketches the core of an OS kernel:

上述代码勾勒出了操作系统内核的核心逻辑：

* handle thread scheduling upon a timer interrupt
> * 在发生定时器中断时处理线程调度

* handle system calls when a thread invokes `ecall`
> * 在线程调用 `ecall` 时处理系统调用

* handle other exceptions such as memory access faults
> * 处理其他异常，例如内存访问故障（memory access faults）

This is just a sketch, since a complete OS must handle all interrupts and exceptions; these three items are probably the most important. You have seen thread scheduling in P2, and P3 will give you hands-on experience with system calls and memory access faults.

这只是一个示意框架，因为一个完整的操作系统必须处理所有的中断和异常；而这三项可以说是最重要的内容。你在 P2 中已经体验过了线程调度，P3 则将带你动手实践系统调用和内存访问故障的处理。

## Inter-process communication

> **进程间通信**

There are only 2 types of system calls in egos-2000. They are designed for inter-process communication, meaning sending and receiving messages. Next, we introduce the system call interface for applications, and then explain what happens within the OS kernel.

egos-2000 中仅有两种类型的系统调用。它们专门用于进程间通信（inter-process communication），即发送和接收消息。接下来，我们将首先介绍面向应用程序的系统调用接口，然后解释操作系统内核内部发生的过程。

### Application-side interface

> **应用端接口**

The code below is from library/syscall/`syscall.h`, and it defines the data structures for system calls in egos-2000.

下面的代码来自于 `library/syscall/syscall.h`，它定义了 egos-2000 中系统调用所使用的数据结构。

```c
enum syscall_type {
    SYS_RECV = 1,
    SYS_SEND = 2,
};

struct syscall {
    enum syscall_type type; /* SYS_SEND or SYS_RECV */
    int sender;             /* sender process ID    */
    int receiver;           /* receiver process ID  */
    char content[1024];
    enum { PENDING, DONE } status;
};
```

The content field holds the message being sent or received. Say process A wants to send a message to process B through `SYS_SEND`; this system call may not succeed immediately. It succeeds only after process B invokes `SYS_RECV` with process A as the sender, meaning process B is ready to receive a message from process A. For this reason, before process B invokes `SYS_RECV`, the `SYS_SEND` system call made by process A has a status of PENDING rather than DONE.

`content` 字段用于存放正在发送或接收的消息。假设进程 A 想要通过 `SYS_SEND` 发送消息给进程 B，该系统调用可能不会立即成功。只有当进程 B 以进程 A 作为发送方调用 `SYS_RECV`（意味着进程 B 准备好接收来自进程 A 的消息）之后，该调用才会成功。因此，在进程 B 调用 `SYS_RECV` 之前，进程 A 发起的 `SYS_SEND` 系统调用的状态为 `PENDING`（未完成），而不是 `DONE`（已完成）。

TIP

提示

In other words, egos-2000 implements a blocking version of inter-process communication, so a system call returns only after a message has been successfully sent or received. It is also possible to implement a non-blocking version in which system calls return immediately. egos-2000 uses the blocking version for code simplicity.

换言之，egos-2000 实现的是阻塞式（blocking）进程间通信，因此系统调用只有在消息成功发送或接收之后才会返回。当然也可以实现非阻塞式（non-blocking）版本，即系统调用立即返回。为保持代码简洁，egos-2000 采用了阻塞式设计。

With this `struct syscall` in mind, the system call interface `sys_send` and `sys_recv` in library/syscall/`syscall.c` should be easy to understand.

理解了 `struct syscall` 之后，`library/syscall/syscall.c` 中的系统调用接口 `sys_send` 和 `sys_recv` 就很容易理解了。

```c
static struct syscall* sc = (struct syscall*)SYSCALL_ARG;

void sys_send(int receiver, char* msg, uint size) {
    sc->type     = SYS_SEND;
    sc->receiver = receiver;
    memcpy(sc->content, msg, size);
    asm("ecall");
}

void sys_recv(int from, int* sender, char* buf, uint size) {
    sc->type   = SYS_RECV;
    sc->sender = from;
    asm("ecall");
    memcpy(buf, sc->content, size);
    if (sender) *sender = sc->sender;
}
```

Again, the `ecall` highlighted above triggers an environment call exception, and the CPU then jumps to the exception handler, `trap_entry`, right after the `ecall` instruction.

同样，上面代码中的 `ecall` 会触发环境调用异常，紧接着 CPU 就会跳转到异常处理程序 `trap_entry`。

### Kernel-side handling

> **内核侧处理**

The exception handler in egos-2000, `trap_entry`, calls `kernel_entry()`, which then calls `excp_entry()` after an environment call exception is raised by `ecall`. We now explain the following if-statement for system calls.

当 `ecall` 引发环境调用异常后，egos-2000 中的异常处理程序 `trap_entry` 会调用 `kernel_entry()`，后者接着调用 `excp_entry()`。下面我们来解释用于处理系统调用的 `if` 语句逻辑。

```c
if (id >= EXCP_ID_ECALL_U && id <= EXCP_ID_ECALL_M) {
    /* Copy the system call arguments from user space to the kernel. */
    uint syscall_paddr = earth->mmu_translate(curr_pid, SYSCALL_ARG);
    memcpy(&proc_set[curr_proc_idx].syscall, (void*)syscall_paddr,
           sizeof(struct syscall));
    proc_set[curr_proc_idx].syscall.status = PENDING;

proc_set_pending(curr_pid);
    proc_set[curr_proc_idx].mepc += 4;
    proc_try_syscall(&proc_set[curr_proc_idx]);
    proc_yield();
    return;
}
```

First of all, `proc_set`[`curr_proc_idx`] is the `struct representing` the current process that has just invoked the system call.

首先，`proc_set[curr_proc_idx]` 是代表刚刚发起系统调用的当前进程的结构体。

* Note that this process initialized the syscall `struct at` memory address `SYSCALL_ARG`. Lines #3 and #4 copy the data structure into the kernel. You can ignore earth->`mmu_translate` for now; we will explain it in P4. Line #6 sets the system call status to PENDING, and line #8 sets the process status to `PROC_PENDING_SYSCALL`.
> * 请注意，该进程在内存地址 `SYSCALL_ARG` 处初始化了 `syscall` 结构体。第 3 行和第 4 行将该数据结构复制到内核中。你可以暂时忽略 `earth->mmu_translate`，我们将在 P4 中进行讲解。第 6 行将系统调用状态设置为 `PENDING`，第 8 行将进程状态设置为 `PROC_PENDING_SYSCALL`。

* Recall that `mepc` stands for the program counter when the exception occurs, and the value of `mepc` is read into `proc_set`[`curr_proc_idx`].`mepc` in function `kernel_entry`. Therefore, line #9 says that after the system call completes, the kernel should return to the instruction immediately after `ecall` (i.e., skip the 4-byte `ecall` instruction).
> * 回想一下，`mepc` 保存发生异常时的程序计数器（program counter）值，且 `mepc` 的值会在函数 `kernel_entry` 中被读取到 `proc_set[curr_proc_idx].mepc` 中。因此，第 9 行表示在系统调用完成后，内核应当返回到紧跟在 `ecall` 后面的那条指令继续执行（即跳过 4 字节长的 `ecall` 指令）。

* Line #10 attempts to process the `SYS_SEND` or `SYS_RECV` system call for the current process, and line #11 finds the next process to schedule. `proc_try_syscall` is also called in `proc_yield` because the scheduler repeatedly attempts to process a pending system call until it succeeds.
> * 第 10 行尝试为当前进程处理 `SYS_SEND` 或 `SYS_RECV` 系统调用，而第 11 行则寻找下一个要调度的进程。`proc_try_syscall` 也会在 `proc_yield` 中被调用，因为调度器会不断尝试处理处于挂起状态的系统调用，直至其成功完成。

Read `proc_try_syscall()`, `proc_try_send()` and `proc_try_recv()`. There are only ~40 lines of code, but they gracefully handle inter-process communication. At a high level, if a process has a pending system call, `proc_try_syscall()` will retry the system call and, if it succeeds, set the process status to `PROC_RUNNABLE`.

请阅读 `proc_try_syscall()`、`proc_try_send()` 和 `proc_try_recv()`。代码虽然只有大约 40 行，却优雅地处理了进程间通信。在宏观层面，如果某个进程有一个未完成的系统调用，`proc_try_syscall()` 会重试该系统调用；一旦成功，就会将该进程的状态设置为 `PROC_RUNNABLE`。

TIP

提示

At this point, you have finished reading grass/`kernel.c`, including `kernel_entry`, `intr_entry`, `excp_entry`, `proc_yield`, `proc_try_syscall`, `proc_try_send`, and `proc_try_recv`.

至此，你已经完成了对 `grass/kernel.c` 中所有代码的阅读，包括 `kernel_entry`、`intr_entry`、`excp_entry`、`proc_yield`、`proc_try_syscall`、`proc_try_send` 和 `proc_try_recv`。

### Introduce process sleep

> **引入进程休眠**

After getting familiar with the system call control flow, we now ask you to use system calls to introduce process sleep. As shown in library/syscall/`servers.h`, the `GPID_PROCESS` process in egos-2000 accepts 3 message types to spawn and terminate processes. Your job is to add a fourth message type, `PROC_SLEEP`, to the struct `proc_request` so that the process that sends this message to `GPID_PROCESS` will sleep for a specified amount of time before being scheduled again. Start from a fresh copy of egos-2000, and add the following code as a new file apps/user/`sleep.c`.

在熟悉了系统调用的控制流之后，现在我们要求你利用系统调用来实现进程休眠（process sleep）功能。正如 `library/syscall/servers.h` 中所示，egos-2000 中的 `GPID_PROCESS` 进程接收 3 种消息类型来创建和终止进程。你的任务是在 `struct proc_request` 中添加第四种消息类型 `PROC_SLEEP`，以便向 `GPID_PROCESS` 发送此消息的进程在再次被调度之前能够休眠指定的时间。从一份干净的 egos-2000 副本开始，添加以下代码作为新文件 `apps/user/sleep.c`。

```c
#include "app.h"

int main() {
  const uint usec_cnt = 5000000;
  printf("Start to sleep for %d microseconds.\n\r", usec_cnt);
  sleep(usec_cnt);
  printf("Woke up again after %d microseconds.\n\r", usec_cnt);
}
```

Then run make qemu, and sleep will be automatically added as a user command:

然后运行 `make qemu`，`sleep` 就会被自动添加为用户命令：

```text
> make qemu
...
➜ /home/yunhao sleep
Start to sleep for 5000000 microseconds.
Woke up again after 5000000 microseconds.
```

For now, you will see the second line printed immediately after the first because the sleep function in library/syscall/`servers.c` has not been implemented. You shall see the second line 5 seconds after the first line when you complete the following steps.

目前，你会看到第二行紧接着第一行被打印出来，因为 `library/syscall/servers.c` 中的 `sleep` 函数尚未实现。当你完成以下步骤后，第二行应该会在第一行打印出来的 5 秒后显示出来。

* Update the struct `proc_request` and the sleep function mentioned above so that this sleep function sends a `PROC_SLEEP` message to the `GPID_PROCESS` process.
> * 更新 `struct proc_request` 以及上述 `sleep` 函数，使得该 `sleep` 函数向 `GPID_PROCESS` 进程发送一个 `PROC_SLEEP` 消息。

* In apps/system/`sys_proc.c`, add a case for `PROC_SLEEP` and put debug printing there temporarily, so you know that `GPID_PROCESS` succeeds in receiving the message.
> * 在 `apps/system/sys_proc.c` 中，为 `PROC_SLEEP` 添加一个分支（`case`），并在其中临时加入调试打印信息，以便确认 `GPID_PROCESS` 成功接收到了该消息。

* Add the `proc_sleep` function in grass/`process.c` to the grass layer interface (`struct grass` in library/`egos.h`), and initialize it in grass/`init.c`, just like `proc_alloc`.
> * 将 `grass/process.c` 中的 `proc_sleep` 函数添加到 grass 层接口（`library/egos.h` 中的 `struct grass`）中，并在 `grass/init.c` 中对其进行初始化，就像 `proc_alloc` 一样。

* Invoke grass->`proc_sleep` in the `PROC_SLEEP` case you have just added in step 2. Then add debug printing in the `proc_sleep` function of grass/`process.c`, so you know that `proc_sleep` is called by `GPID_PROCESS` with the correct pid and usec arguments.
> * 在步骤 2 中刚刚添加的 `PROC_SLEEP` 分支中调用 `grass->proc_sleep`。然后在 `grass/process.c` 的 `proc_sleep` 函数中添加调试打印信息，从而确认 `GPID_PROCESS` 带着正确的 `pid` 和 `usec` 参数调用了 `proc_sleep`。

* Implement this `proc_sleep` function, which should put the process identified by pid to sleep for usec microseconds. This involves a few modifications to the kernel.
> * 实现该 `proc_sleep` 函数，使其能让由 `pid` 标识的进程休眠 `usec` 微秒。这需要对内核进行一些修改：

* Add one or more fields to `struct process`, and initialize them in `proc_alloc()`.
> * 为 `struct process` 添加一个或多个字段，并在 `proc_alloc()` 中对其进行初始化。

* Modify these fields for process pid in `proc_sleep()`. In addition to argument usec, you need `mtime_get()`, which returns the clock time in 10^-7 seconds (on QEMU).
> * 在 `proc_sleep()` 中为进程 `pid` 修改这些字段。除了参数 `usec` 之外，你还需要用到 `mtime_get()`，它在 QEMU 上以 \(10^{-7}\) 秒为单位返回时钟时间。

* Modify `proc_yield()` to schedule a process only if it is not sleeping, using the fields in `struct process` and the latest clock time from `mtime_get()`.
> * 修改 `proc_yield()`，利用 `struct process` 中的字段以及来自 `mtime_get()` 的最新时钟时间，仅当进程不在休眠状态时才对其进行调度。

* The kernel may now encounter a situation in which no process can be scheduled. You need to handle this situation by replacing the FATAL in `proc_yield()` with your code.
> * 内核现在可能会遇到没有任何进程可被调度的情形。你需要用自己的代码替换 `proc_yield()` 中的 `FATAL` 来处理这种情况。

* Remove the debug printings. Run sleep again in the egos-2000 shell, and you shall see the Woke up ... printing 5 seconds after the first line of printing.
> * 移除调试打印语句。在 egos-2000 shell 中再次运行 `sleep`，你应该会在第一行打印出来 5 秒后看到 `Woke up ...` 的输出。

## Protect the OS memory

> **保护操作系统内存**

By far, all the code we have seen runs in the so-called machine mode, which means it can freely access memory. However, user applications should not be able to read or write memory freely. Otherwise, a malicious application can corrupt the kernel's memory, causing damage. At a high level, we now ask you to do 3 things:

截至目前，我们看到的所有代码都运行在所谓的机器模式（machine mode）下，这意味着它可以自由地访问内存。然而，用户应用程序不应当能够随意读写内存。否则，恶意应用程序可能会破坏内核的内存，造成系统损坏。从宏观上讲，现在我们要求你做 3 件事：

* Specify the memory regions that code in the user mode is allowed to access.
> * 指定用户模式（user mode）下的代码允许访问的内存区域。

* Run the code of all user applications in user mode rather than machine mode.
> * 在用户模式而非机器模式下运行所有用户应用程序的代码。

* Terminate a user application if it triggers an exception by reading or writing the memory at an address outside of the allowed regions.
> * 如果用户应用程序因读写允许区域之外的内存地址而触发异常，则终止该应用程序。

### Set up a PMP region

> **设置 PMP 区域**

Read through chapter 3.7 of the RISC-V reference manual for Physical Memory Protection (PMP), and then write your code in earth/`cpu_mmu.c`:

通读 RISC-V 参考手册中关于物理内存保护（Physical Memory Protection，PMP）的第 3.7 章，然后在 `earth/cpu_mmu.c` 中编写你的代码：

```c
void mmu_init() {
    /* Set up a PMP region for the whole 4GB address space. */
    asm("csrw pmpaddr0, %0" : : "r"(0x40000000));
    asm("csrw pmpcfg0, %0" : : "r"(0xF));

/* Student's code goes here (System Call & Protection). */

/* Replace the PMP region above with two PMP NAPOT regions:
     * [APPS_ENTRY, APPS_ENTRY + 2MB) with permission R/W/X;
     * [SHELL_WORK_DIR, SHELL_WORK_DIR + 4KB) with permission R/W. */

/* Student's code ends here. */
    ...
}
```

TIP

提示

Your code should overwrite the two CSRs `pmpaddr0` and `pmpcfg0`, so the 4GB region no longer takes effect and is replaced by the 2MB region `[0x80200000, 0x80400000)`. As a result, you will only be able to choose software TLB when booting egos-2000 in the rest of P3.

你的代码应当重写 `pmpaddr0` 和 `pmpcfg0` 这两个 CSR，使原先的 4GB 区域不再生效，并替换为 2MB 区域 `[0x80200000, 0x80400000)`。因此，在 P3 后续部分启动 egos-2000 时，你将只能选择软件 TLB（software TLB）。

Specifically, code running in the user mode cannot access any memory region by default. After you finish the PMP code above, code running in user mode will be able to access two memory regions:

具体而言，默认情况下运行在用户模式下的代码无法访问任何内存区域。完成上述 PMP 代码后，运行在用户模式下的代码将能够访问两个内存区域：

* `[0x80200000, 0x80400000)`—it contains the code, data, heap, and stack of the current process. Set the permissions to read, write, and execute.
> * `[0x80200000, 0x80400000)`——该区域包含当前进程的代码、数据、堆和栈。请将其权限设置为可读、可写和可执行（R/W/X）。

* `[0x80102000, 0x80103000)`—it contains the shell's current working directory. Set the permissions to read and write, but not execute.
> * `[0x80102000, 0x80103000)`——该区域包含 shell 的当前工作目录。请将其权限设置为可读和可写（R/W），但不可执行。

However, PMP won't take any effect if we still run everything in machine mode, so we need to switch privilege modes when switching the CPU context from the kernel back to a user application process.

然而，如果我们仍然在机器模式下运行所有代码，PMP 就不会发挥任何作用。因此，在将 CPU 上下文从内核切换回用户应用程序进程时，我们需要切换特权模式（privilege modes）。

### Switch privilege modes

> **切换特权模式**

You need to understand `mstatus`.MPP and update `mstatus`.MPP in `proc_yield` according to the comments there. Recall that `mstatus`.MPP stands for bit#11 and bit#12 of `mstatus`:

你需要理解 `mstatus.MPP`，并根据 `proc_yield` 中的注释在该函数中更新 `mstatus.MPP`。回想一下，`mstatus.MPP` 代表 `mstatus` 的第 11 位和第 12 位（bit#11 和 bit#12）：

You will need to set these bits to 11 if the next scheduled process is a kernel process (i.e., pid < `GPID_USER_START`), or set them to 00 for all other processes. In RISC-V, 0 stands for user mode, and 3 (i.e., 11 in binary) stands for machine mode. To see how it works, we need to explain what happens when entering and exiting the kernel.

如果下一个被调度的进程是内核进程（即 `pid < GPID_USER_START`），你需要将这些位设置为 11；对于所有其他进程，则将其设置为 00。在 RISC-V 中，0 代表用户模式，3（即二进制的 11）代表机器模式。为了了解其工作原理，我们需要解释在进入和退出内核时会发生什么。

Upon an interrupt or exception, the CPU enters the kernel and automatically switches the privilege mode to machine mode before jumping to the `trap_entry` handler. This allows the kernel to run in machine mode and freely access the memory.

当发生中断或异常时，CPU 进入内核并在跳转到 `trap_entry` 处理程序之前自动将特权模式切换为机器模式。这使得内核能够在机器模式下运行并自由地访问内存。

Upon executing `mret` in grass/`kernel.s`, the CPU exits the kernel, and `mret` will switch the privilege mode according to `mstatus`.MPP. Therefore, if we set `mstatus`.MPP to 00 in `proc_yield`, the next scheduled process will run in user mode after the `mret` instruction.

在执行 `grass/kernel.s` 中的 `mret` 指令时，CPU 会退出内核，且 `mret` 将根据 `mstatus.MPP` 切换特权模式。因此，如果在 `proc_yield` 中将 `mstatus.MPP` 设置为 00，下一个被调度的进程将在 `mret` 指令执行后运行在用户模式下。

### Kill malicious applications

> **终止恶意应用程序**

To test whether you correctly set the PMP region and switched privilege modes, we have provided 2 malicious applications, crash1 and crash2, in the apps/user directory. The malicious applications would halt the whole operating system by corrupting the memory.

为了测试你是否正确设置了 PMP 区域并切换了特权模式，我们在 `apps/user` 目录下提供了 2 个恶意应用程序：`crash1` 和 `crash2`。这些恶意应用程序试图通过破坏内存来使整个操作系统瘫痪。

```text
>  make qemu
...
[CRITICAL] Choose a memory translation mechanism:
Enter 0: page tables
Enter 1: software TLB
[INFO] Software translation is chosen
...
[CRITICAL] Welcome to the egos-2000 shell!
➜ /home/yunhao crash1
_sbrk: heap grows too large
[FATAL] excp_entry: kernel got exception 7
```

Note that this FATAL happens at the end of function `excp_entry`. Your final task in P3 is to implement the following part of `excp_entry`.

请注意，这个致命错误（`FATAL`）发生在函数 `excp_entry` 的末尾。你在 P3 中最后的任务就是实现 `excp_entry` 中如下所示的部分。

```c
static void excp_entry(uint id) {
    ...
    /* Student's code goes here (System Call & Protection | Virtual Memory). */

/* Kill the current process if curr_pid is a user application. */

/* Student's code ends here. */
    FATAL("excp_entry: kernel got exception %d", id);
}
```

After `excp_entry` gracefully kills the malicious applications, you should see the following.

在 `excp_entry` 优雅地终止恶意应用程序后，你应该能看到如下输出：

```text
# Make sure to choose software TLB
>  make qemu
...
➜ /home/yunhao crash1
_sbrk: heap grows too large
[INFO] process 6 terminated with exception 7
➜ /home/yunhao crash2
[INFO] process 7 terminated with exception 7
> /home/yunhao
```

In other words, memory protection should work: malicious applications running in user mode trigger memory exceptions when attempting to corrupt memory. The kernel kills these malicious applications when handling such exceptions.

换句话说，内存保护应当起作用：运行在用户模式下的恶意应用程序在尝试破坏内存时会触发内存异常。内核在处理这些异常时会终止这些恶意应用程序。

## Accomplishments

> **成就与小结**

In terms of OS concepts, you have learned about exception handling, system calls, privilege modes, and inter-process communication. In terms of code reading, you have completed all the code in grass and library/syscall. The grass layer is the kernel in egos-2000.

在操作系统概念方面，你已经学习了异常处理、系统调用、特权模式以及进程间通信。在代码阅读方面，你已经完成了对 `grass` 和 `library/syscall` 中所有代码的阅读。`grass` 层即 egos-2000 中的内核。

You will read earth/`cpu_mmu.c` and library/elf/* in P4, read earth/`dev_disk.c` and earth/`dev_tty.c` in P5, and read library/file/* in P6. Then you will essentially have read all the code for egos-2000. We are halfway there!

你将在 P4 中阅读 `earth/cpu_mmu.c` 和 `library/elf/*`，在 P5 中阅读 `earth/dev_disk.c` 和 `earth/dev_tty.c`，在 P6 中阅读 `library/file/*`。届时，你就基本上读过了 egos-2000 的全部代码。我们已经完成了一半的路程！
