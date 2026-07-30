# Cooperative Threads

> **协作式线程 ​**

This project helps you understand the following C program:
> 本项目将帮助你理解以下 C 程序。

```
void child(void* arg) {
    printf("%s is running.\n\r", arg);
}

int main() {
    thread_init();
    thread_create(child, "Child thread");
    printf("Main thread is running.\n\r");
    thread_exit();
}
```

12345678910

You have implemented printf in P0, and you will now implement the functions `thread_init`, `thread_create`, and `thread_exit` used in this program. Then you shall see its output:txt
> 你在 P0 中已经实现了 printf，现在你将实现该程序中使用的 `thread_init`、`thread_create` 和 `thread_exit` 函数。随后你将看到如下输出：

```
# Possible output #1
Child thread is running.
Main thread is running.

# Possible output #2

Main thread is running.
Child thread is running.
```

1234567

These two possibilities demonstrate the concepts of threads and concurrency. Consider a thread as executing a function. Initially, there is only one thread running function main; After main invokes `thread_create`, another thread is created running function child. Let's call them the main thread and the child thread. Concurrency means that the program does not enforce any ordering between the two threads. Therefore, if the child thread prints first, we will observe the first output above. Otherwise, if the main thread prints first, we will observe the second possibility.
> 这两种可能性展示了线程和并发的概念。我们可以将一个线程理解为在执行一个函数。最初，只有一个主线程在运行 main 函数；在 main 调用 `thread_create` 之后，就会创建另一个运行 child 函数的线程。我们分别称它们为主线程（main thread）和子线程（child thread）。并发意味着程序并不强制规定这两个线程之间的执行顺序。因此，如果子线程先打印，我们将看到上述第一种输出；反之，若主线程先打印，我们就会观察到第二种可能。

TIP
> 提示

We will explain the meaning of cooperative later. The key here is that everything in P1 will only use the three types of CPU instructions explained in P0 (i.e., integer computation, control transfer, and memory access). Again, we have been working hard to help you gain a precise understanding of OS concepts (e.g., threads) by connecting them to the specific CPU instructions.
> 我们稍后会解释“协作式”（cooperative）的含义。这里的关键在于，P1 中的所有内容都仅使用 P0 中介绍过的三种 CPU 指令（即整数运算、控制转移和内存访问）。我们始终致力于将操作系统概念（例如线程）与具体的 CPU 指令相挂钩，从而帮助你对其建立精准的理解。

## Get started

> **开始体验 ​**

You will start from the thread branch of egos-2000, which provides printf and `_sbrk` for malloc just like P0. You can add your P0 solution code to `print.c` for debug printing. The functions that you need to implement in P1 are listed in `thread.h` and `thread.c`.
> 你将从 egos-2000 的 thread 分支开始。与 P0 一样，该分支提供了 printf 以及用于 malloc 的 `_sbrk`。你可以将你在 P0 中完成的代码添加到 `print.c` 中用于调试打印。你在 P1 中需要实现的函数已列在 `thread.h` 和 `thread.c` 中。

Lastly, `thread.s` contains two important helper functions, `ctx_start` and `ctx_switch`:asm
> 最后，`thread.s` 包含了两个重要的辅助函数 `ctx_start` 和 `ctx_switch`：

```
ctx_start:
    addi sp,sp,-128
    SAVE_ALL_REGISTERS
    sw sp,0(a0)
    mv sp,a1
    call ctx_entry

ctx_switch:
    addi sp,sp,-128
    SAVE_ALL_REGISTERS
    sw sp,0(a0)
    mv sp,a1
    RESTORE_ALL_REGISTERS
    addi sp,sp,128
    ret
```

123456789101112131415

The `SAVE_ALL_REGISTERS` and `RESTORE_ALL_REGISTERS` are macros with a straightforward definition in `context.s`. Next, we explain how to use `ctx_start` to start a thread's execution, and how to use `ctx_switch` to transfer control flow to a thread that is already executing. After the explanations, you should be able to implement the 3 functions used by the multithreaded program above, and run that program.
> `SAVE_ALL_REGISTERS` 和 `RESTORE_ALL_REGISTERS` 是两个宏，它们的定义在 `context.s` 中简单明了。接下来，我们将解释如何使用 `ctx_start` 启动线程的执行，以及如何使用 `ctx_switch` 将控制流转移给一个已经在执行的线程。了解这些细节后，你应该就能实现上述多线程程序所用到的 3 个函数并运行该程序。

## Context switch

> **上下文切换 ​**

We explain the use of `ctx_start` and `ctx_switch` with one of the possible executions of the multithreaded program:
> 我们通过上述多线程程序的一种可能执行过程来解释 `ctx_start` 和 `ctx_switch` 的用法：

* The main thread calls `thread_init` and `thread_create`.
> * 主线程调用 `thread_init` 和 `thread_create`。

* The child thread starts execution, prints, and calls `thread_exit`.
> * 子线程开始执行、打印并调用 `thread_exit`。

* The main thread returns from `thread_create`, prints, and calls `thread_exit`.
> * 主线程从 `thread_create` 返回、打印并调用 `thread_exit`。

The picture below illustrates the memory layout during the three steps, which contains the four types of memory regions explained in P0 (i.e., code, data, heap, and stack).
> 下图展示了上述三个步骤中的内存布局，其中包含了 P0 中介绍过的四种内存区域（即代码段、数据段、堆和栈）。

### What is context?

> **什么是上下文？ ​**

In step #1, `thread_create` is preparing to launch the child thread, and an important part of this preparation is allocating memory for the child thread's stack, shown as the shaded region. We now have two stacks in memory, a key feature of multithreaded programs: one stack per thread.
> 在步骤 #1 中，`thread_create` 正在为启动子线程做准备，其中一个关键步骤是为子线程的栈分配内存（即图中阴影区域所示）。此时内存中有两个栈，这也是多线程程序的核心特征：每个线程拥有独立的栈。

The `sp` in the picture indicates the stack pointer. In step #1, the stack pointer points to the main thread's stack, and the program counter points to an instruction of `thread_create`. Therefore, we say that the CPU is in the main thread's context. The move from step #1 to #2 is an example of a context switch: the stack pointer now points to the child thread's stack, and the program counter points to the address of printf. The CPU is thus in the child thread's context in step #2.
> 图中的 `sp` 表示栈指针（stack pointer）。在步骤 #1 中，栈指针指向主线程的栈，程序计数器（PC）指向 `thread_create` 中的一条指令。因此，我们称 CPU 此时处于主线程的上下文（context）中。从步骤 #1 转换到步骤 #2 是上下文切换的一个范例：栈指针此时指向了子线程的栈，而程序计数器则指向 printf 的地址。因此在步骤 #2 中，CPU 处于子线程的上下文中。

### Control flow details

> **控制流细节 ​**

Here are more details about the control flow from step #1 to step #2, and then to step #3.
> 以下是关于从步骤 #1 到步骤 #2，然后再到步骤 #3 的控制流的更多细节。

* `thread_create` invokes `ctx_start`, and the `ctx_start` region in the picture has 128 bytes (i.e., the first instruction in `ctx_start` is `addi` `sp`,`sp`,-128). 128=32*4 because a 32-bit RISC-V CPU has 32 general-purpose registers, and `SAVE_ALL_REGISTERS` saves the value of all these registers on the stack.
> * `thread_create` 调用 `ctx_start`，图中 `ctx_start` 栈帧区域的大小为 128 字节（即 `ctx_start` 中的第一条指令是 `addi` `sp`,`sp`,-128）。128 = 32 × 4，因为 32 位 RISC-V CPU 拥有 32 个通用寄存器，且 `SAVE_ALL_REGISTERS` 会将所有这些寄存器的值压入栈中。

* The `sw` `sp`,0(`a0`) in `ctx_start` saves the 4-byte value of register `sp` (i.e., the saved label in the picture) at memory address `a0`, the first argument of `ctx_start`. The purpose of saving `sp` at this point is to allow us to switch the CPU context back to the main thread.
> * `ctx_start` 中的 `sw` `sp`,0(`a0`) 指令将寄存器 `sp` 的 4 字节值（即图中标记为 saved 的位置）保存到内存地址 `a0` 中（a0 即 `ctx_start` 的第一个参数）。此时保存 `sp` 的目的是为了以后能够将 CPU 上下文切换回主线程。

* The mv `sp`,`a1` in `ctx_start` moves the stack pointer to a memory address specified by `a1`, the second argument of `ctx_start`, shown as init `sp` in the picture.
> * `ctx_start` 中的 mv `sp`,`a1` 指令将栈指针切换到由 a1（即 `ctx_start` 的第二个参数）指定的内存地址，在图中标记为 init sp。

* call `ctx_entry` invokes the `ctx_entry` function in `thread.c`. Intuitively, `ctx_entry` should find out that the child thread is the one to execute, and thus call the child function. After the child function returns, `ctx_entry` should terminate the thread by calling `thread_exit`.
> * call `ctx_entry` 调用 `thread.c` 中的 `ctx_entry` 函数。逻辑上，`ctx_entry` 应当判断出接下来要执行的是子线程，从而调用 child 函数。在 child 函数返回后，`ctx_entry` 应当通过调用 `thread_exit` 来终止该线程。

* Lastly, `thread_exit` invokes `ctx_switch`, which switches the CPU context back to the main thread. The assembly code of `ctx_switch` is similar to `ctx_start`, so figure out yourself what is happening during `ctx_switch`.
> * 最后，`thread_exit` 调用 `ctx_switch`，后者将 CPU 上下文切换回主线程。`ctx_switch` 的汇编代码与 `ctx_start` 类似，因此请自行思考 `ctx_switch` 执行过程中发生了什么。

* After `ctx_start` and `thread_create` both return, the main thread continues invoking printf and prints as shown in step #3 of the picture.
> * 在 `ctx_start` 和 `thread_create` 都返回之后，主线程继续调用 printf 进行打印，如图中步骤 #3 所示。

A few details are still missing: How does `thread_create` decide the two arguments that it should pass to `ctx_start`? How does `ctx_entry` figure out that child is the function to call? These questions lead to the concept of the Thread Control Block (TCB).
> 仍有一些细节尚未说明：`thread_create` 如何确定传给 `ctx_start` 的两个参数？`ctx_entry` 如何弄清楚 child 是要调用的函数？这些问题引出了线程控制块（Thread Control Block, TCB）的概念。

### Thread control block

> **线程控制块 ​**

The thread control block is a data structure storing the metadata of every thread, including:
> 线程控制块（TCB）是一种用于存储每个线程元数据的数据结构，包括：

* thread id: a unique identifier of a thread
> * 线程 ID：线程的唯一标识符

* status: the thread’s current execution status
> * 状态（status）：线程当前的执行状态

* stack pointer: the saved value of register `sp` before switching to another thread
> * 栈指针（stack pointer）：在切换到另一个线程之前保存的 `sp` 寄存器值

* entry function: the “main” function of a thread (e.g., the child function)
> * 入口函数（entry function）：线程的“主”函数（例如 child 函数）

* entry function arguments: the arguments for the thread’s entry function
> * 入口函数参数（entry function arguments）：传给线程入口函数的参数

* other metadata that you find helpful
> * 其他你认为有用的元数据

You can implement the thread control block as follows:
> 你可以按如下方式实现线程控制块。

```
struct thread {
    int id;
    void* sp;
    ...                // Design the rest of struct thread yourself.
};

int current_idx;       // TCB[current_idx] is the currently running thread.
struct thread TCB[32]; // Represent TCB as a simple array. You can also use
                       // a queue data structure as we will explain later.
```

123456789

The code below shows how to use the TCB for `ctx_start` in `thread_create`:
> 下面的代码展示了如何在 `thread_create` 中结合 TCB 调用 `ctx_start`。

```
char* child_stack = malloc(STACK_SIZE);
ctx_start(&TCB[current_idx].sp, child_stack + STACK_SIZE);
```

12

TCB[`current_idx`].`sp`, as a pointer in a 32-bit program, occupies 4 bytes in the memory. &TCB[`current_idx`].`sp` is the address of these 4 bytes, and the `sw` `sp`,0(`a0`) instruction in `ctx_start` stores the 4 bytes of register `sp` into TCB[`current_idx`].`sp`. Unlike int* which stores the address of an integer, void* is used when we simply need to store a 32-bit memory address without any type information. In this case, we simply need to store the address shown by the saved label in step #2 of the picture above.
> TCB[`current_idx`].`sp` 作为 32 位程序中的指针，在内存中占用 4 个字节。&TCB[`current_idx`].`sp` 是这 4 个字节的内存地址，而 `ctx_start` 中的 `sw` `sp`,0(`a0`) 指令会将 `sp` 寄存器的 4 字节值写入 TCB[`current_idx`].`sp` 中。与用于存储整数地址的 int* 不同，当我们需要在不附带任何类型信息的情况下仅存储一个 32 位内存地址时，会使用 void*。在本例中，我们只需要存储上图步骤 #2 中标记为 saved 的地址。

Note that malloc returns the lowest address of the allocated memory, so `child_stack` + `STACK_SIZE` gives us the highest address shown as init `sp` in the picture. As the second argument of `ctx_start`, this value is stored in register `a1`, and thus mv `sp`,`a1` moves the stack pointer from the main thread's stack to the child thread's stack.
> 请注意，malloc 返回已分配内存块的低端地址（lowest address），因此 `child_stack` + `STACK_SIZE` 计算得出的是高端地址（即图中所示的 init sp）。作为 `ctx_start` 的第二个参数，该值会被传入 `a1` 寄存器，随后的 mv `sp`,`a1` 指令便将栈指针从主线程的栈切换到了子线程的栈。

Figure out yourself how to store and use the entry function in `struct thread`—you need the concept of a function pointer in C. A function pointer holds 4 bytes as the address of a function's first instruction in the code region (e.g., the first instruction of function child).
> 请自行思考如何在 `struct thread` 中存储并使用入口函数——这需要用到 C 语言中的函数指针概念。函数指针占用 4 个字节，保存着代码段中函数第一条指令的地址（例如 child 函数的首条指令）。

TIP
> 提示

Both the main and child threads call `thread_exit()`. When the last thread calls `thread_exit()`, `thread_exit()` should call `_end`() in `thread.s`, halting the execution of the whole program. Read the comments at the end of `thread.c` for more details.
> 主线程和子线程都会调用 `thread_exit()`。当最后一个线程调用 `thread_exit()` 时，`thread_exit()` 应当调用 `thread.s` 中的 `_end`()，从而终止整个程序的执行。有关更多细节，请阅读 `thread.c` 末尾的注释。

## Thread yield

> **线程让出 ​**

Before you start, let us introduce the last function you need to implement for multithreading, `thread_yield`. In the 3-step execution discussed above, the CPU context switches back to the main thread only after the child thread exits. However, if the child thread yields before it exits, it will relinquish CPU control sooner, and the main thread will resume execution. Here is an example of two threads yielding to each other:
> 在你开始编码之前，我们再介绍实现多线程所需的最后一个函数 `thread_yield`。在上面讨论的三步执行流程中，只有在子线程退出之后，CPU 上下文才会切换回主线程。然而，如果子线程在退出之前主动让出 CPU，它就会提前交出 CPU 控制权，使主线程得以恢复执行。下面是两个线程互相让出 CPU 的示例代码：

```
void child(void* arg) {
    for (int i = 0; i < 10; i++) {
        printf("%s is in for loop i=%d\n\r", arg, i);
        thread_yield();
    }
}

int main() {
    thread_init();
    thread_create(child, "Child thread");
    for (int i = 0; i < 10; i++) {
        printf("Main thread is in for loop i=%d\n\r", i);
        thread_yield();
    }
    thread_exit();
}
```

12345678910111213141516

In this example, both threads print more, and because they yield to each other in the loop, we could see interleaving prints from the two threads. Intuitively, the key to an OS is to insert yields among different tasks so that they run concurrently. Threads yield explicitly by calling `thread_yield` in P1, and in P2 we will learn how the CPU forces a thread to yield without the thread calling `thread_yield` voluntarily. This is why P1 is called cooperative threading while P2 is doing preemptive threading.
> 在该示例中，两个线程都会进行多次打印。由于它们在循环中相互让出 CPU，我们将观察到来自这两个线程的交替打印输出。直观来看，操作系统的核心机制就在于在不同任务之间插入“让出”（yield），从而使它们能够并发运行。在 P1 中，线程通过显式调用 `thread_yield` 来主动让出 CPU；而在 P2 中，我们将会学习 CPU 如何在线程未主动调用 `thread_yield` 的情况下强制剥夺其执行权并进行切换。这就是为什么 P1 被称为协作式线程（cooperative threading），而 P2 实现的则是抢占式线程（preemptive threading）。

You can now start to implement the TCB and the following functions in `thread.c`. Note that `struct thread` is defined in `thread.h`, which also contains detailed descriptions of these functions. After you finish, you can run the multithreaded programs above and observe the expected printing:
> 现在你可以开始在 `thread.c` 中实现 TCB 以及以下函数。注意 `struct thread` 定义在 `thread.h` 中，该头文件也包含了这些函数的详细说明。完成后，你可以运行上述多线程程序并观察预期的打印结果。

```
void ctx_entry();
void thread_init();
void thread_create(void (*entry)(void *arg), void *arg);
void thread_yield();
void thread_exit();
```

12345

TIP
> 提示

Again, when all threads have terminated, and therefore no thread from the TCB can run, your code should call `_end`(), which halts the whole program. Your code should also free the thread's stack memory after the thread terminates. This can be tricky because you should not free the stack of the currently running thread when it still needs its stack for `thread_exit()` and `ctx_switch()`. Moreover, the main thread's stack is not allocated with `malloc()`, so it cannot be freed (i.e., after the main thread terminates, do nothing to its stack memory).
> 再次强调，当所有线程都已终止（即 TCB 中不再有可运行的线程）时，你的代码应当调用 `_end`()，从而终止整个程序。此外，你的代码还应该在线程终止后释放该线程的栈内存。这一点需要特别注意，因为当当前运行的线程在执行 `thread_exit()` 和 `ctx_switch()` 时仍需要使用栈，你不应该过早释放当前运行线程的栈。另外，主线程的栈并不是通过 `malloc()` 分配的，因此不能被释放（即主线程终止后，无需对其栈内存进行任何释放操作）。

## Synchronization

> **同步 ​**

### Conditional variable

> **条件变量 ​**

In a multithreaded program, threads often need to wait for a certain condition to become true. For example, in a web browser, a thread that renders a webpage on the screen needs to wait until enough network packets for the webpage have been received. Such waiting is typically handled by a synchronization primitive called a conditional variable, which is widely used in real-world software. Consider a conditional variable as a C variable of type `struct cv` used by multiple threads with 3 operations:c
> 在多线程程序中，线程经常需要等待某个条件满足后才能继续执行。例如，在网页浏览器中，负责在屏幕上渲染网页的线程需要等待接收到足够多的网络数据包。这种等待机制通常由称为条件变量（conditional variable）的同步原语（synchronization primitive）来处理，条件变量在真实软件系统中有着极其广泛的应用。你可以将条件变量理解为一个可供多个线程共享的 `struct cv` 类型的 C 语言变量，它支持 3 种基本操作：

```
struct cv {
    // Define this data structure yourself.
};

void cv_wait(struct cv* condition);
void cv_signal(struct cv* condition);
void cv_broadcast(struct cv* condition);
```

1234567

Let cond denote a condition that some threads need to wait for, and let `cond_cv` denote a conditional variable defined in the program for cond. A thread could wait for this condition using a loop while(!cond) `cv_wait`(&`cond_cv`);. After such a loop, the thread can assume that condition cond is true and thus proceed with its work. If cond is true right before the loop, the thread will skip the loop without calling `cv_wait` at all.
> 假设 cond 表示某些线程需要等待的条件，`cond_cv` 表示在程序中为 cond 定义的条件变量。线程可以使用循环 while(!cond) `cv_wait`(&`cond_cv`); 来等待该条件。退出该循环后，线程即可假定条件 cond 成立（为真），从而继续向下执行。如果在进入循环前 cond 就已经成立，线程将直接跳过循环，完全不会调用 `cv_wait`。

If cond is false before the loop, the thread will call `cv_wait`, which yields the thread. If no other thread calls `cv_signal` or `cv_broadcast` on &`cond_cv`, the CPU context will never switch back to the waiting thread. The point is that, if another thread finds cond to be true after some work, this other thread could call `cv_signal` or `cv_broadcast` on &`cond_cv` in order to wake up the waiting thread.
> 如果进入循环前 cond 不成立（为假），线程就会调用 `cv_wait` 并让出 CPU。如果没有其他线程对 &`cond_cv` 调用 `cv_signal` 或 `cv_broadcast`，CPU 上下文就永远不会切换回这个等待线程。因此关键在于，当另一个线程在完成某些工作并使 cond 成立后，该线程可以对 &`cond_cv` 调用 `cv_signal` 或 `cv_broadcast` 来唤醒等待中的线程。

Technically, `struct cv` should maintain an array or queue of `struct thread` just like the TCB. Upon `cv_wait`, the `struct thread` of the currently running thread is removed from the TCB and added to the conditional variable. This thread will never be switched back to until its `struct thread` is added back to the TCB. To this end, `cv_signal` will remove one `struct thread` from a conditional variable and add it back to the TCB, and `cv_broadcast` will do the same for all `struct threads` that are waiting on a conditional variable.
> 从底层实现来看，`struct cv` 应该像 TCB 一样维护一个 `struct thread` 的数组或队列。当调用 `cv_wait` 时，当前正在运行线程的 `struct thread` 会从 TCB 中移除，并放入条件变量的等待队列中。在它的 `struct thread` 被重新放回 TCB 之前，CPU 绝不会切换回该线程。为此，`cv_signal` 会从条件变量的等待队列中取出一个 `struct thread` 并将其放回 TCB，而 `cv_broadcast` 则会将等待在该条件变量上的所有 `struct thread` 全部放回 TCB。

In this project, you only need to implement `cv_wait`, `cv_signal`, and the `cv_init` function to initialize a `struct cv`, so you can run the following multithreaded program.
> 在本项目中，你只需要实现 `cv_wait`、`cv_signal` 以及用于初始化 `struct cv` 的 `cv_init` 函数，便可运行下面的多线程程序。

### Producer-consumer

> **生产者-消费者 ​**

A classic problem in computer science is the producer-consumer problem. In the example below, we have a buffer with a fixed size of 3. A program could create many threads, each a producer or consumer (i.e., running either produce or consume as its entry function). Producers add items to the buffer, and consumers remove them. The constraint is that producers must wait if the buffer is full, and consumers must wait if the buffer is empty. Therefore, we have two conditional variables, nonempty and nonfull, and the associated conditions are highlighted in the code below:
> 计算机科学中一个经典的同步问题是生产者-消费者（producer-consumer）问题。在下面的示例中，我们有一个容量固定为 3 的缓冲区。程序可以创建多个线程，每个线程充当生产者或消费者（即以 produce 或 consume 作为其入口函数）。生产者向缓冲区中添加数据项，消费者从中取出数据项。同步约束条件是：如果缓冲区已满，生产者必须等待；如果缓冲区为空，消费者必须等待。为此，我们使用了两个条件变量 nonempty 和 nonfull，与之相关的判断条件在下面的代码中得以展现。

```
void* buffer[3];
int count = 0;
int head = 0, tail = 0;
struct cv nonempty, nonfull;

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

1234567891011121314151617181920212223242526

Your job is to implement `struct cv` and the three functions. You may also need to modify the code for TCB and the `thread_*` functions (e.g., when a thread calls `thread_yield()` in `cv_wait()`, remove its `struct thread` from the TCB). However, you should not modify the function interface defined in `thread.h`. After you finish, run the producer-consumer code above, and add some debug printing to observe what happens.
> 你的任务是实现 `struct cv` 以及上述三个函数。你可能还需要修改 TCB 和 `thread_*` 函数的代码（例如，当线程在 `cv_wait()` 中调用 `thread_yield()` 时，将其 `struct thread` 从 TCB 中移除）。但请注意，你不应修改 `thread.h` 中定义的函数接口。完成实现后，请运行上述生产者-消费者代码，并添加必要的调试打印来观察程序的运行过程。

## Queues instead of arrays

> **使用队列替代数组 ​**

When representing the TCB and the waiting threads in a conditional variable, you can use a fixed-length array. Alternatively, you can use the queue data structure. The standard library provides sys/`queue.h`, which you can use to define a singly-linked list as a queue:
> 在管理 TCB 以及条件变量中的等待线程时，你可以使用固定长度的数组，也可以使用队列数据结构。标准库提供了 `<sys/queue.h>`，你可以借助它将单链表用作队列。

```
#include `<sys/queue.h>`

struct thread {
    int id;
    ...
    SLIST_ENTRY(thread) next; // next pointer for the singly-linked list
};
SLIST_HEAD(TCB, thread) TCB;

void example_TCB_operations() {
    struct thread * t3;
    for (int i = 0; i < 5; i++) {
        struct thread * t = malloc(sizeof(struct thread));
        t->id = i;
        if (i == 3) t3 = t;
        SLIST_INSERT_HEAD(&TCB, t, next);
    }

// Iterate through all the 5 threads.
    struct thread * iter;
    SLIST_FOREACH(iter, &TCB, next) printf("thread %d\n\r", iter->id);

// Remove t3, and iterate the 4 threads left.
    SLIST_REMOVE(&TCB, t3, thread, next);
    free(t3);
    SLIST_FOREACH(iter, &TCB, next) printf("thread %d\n\r", iter->id);
}
```

123456789101112131415161718192021222324252627

You can ask LLMs for more examples of using sys/`queue.h`, and you can certainly use other data structures in sys/`queue.h` such as TAILQ—TAILQ supports `INSERT_TAIL` in addition to `INSERT_HEAD`.
> 你可以向大语言模型（LLM）询问更多关于使用 `<sys/queue.h>` 的示例，当然也可以使用 `<sys/queue.h>` 中提供的其他数据结构，例如 TAILQ——TAILQ 除了支持 `INSERT_HEAD` 外还支持 `INSERT_TAIL`。

## Accomplishments

> **成就与总结 ​**

You have finished reading most of grass/`kernel.s` in egos-2000, which is very similar to `ctx_switch`. The `proc_yield` function in grass/`kernel.c` is similar to `thread_yield`. You will read and modify `proc_yield` in P2, and finish reading the rest of grass/`kernel.c` in P3. Indeed, you will finish reading everything under grass after P3.
> 至此，你已经理解了 egos-2000 中 grass/`kernel.s` 的大部分代码，它与 `ctx_switch` 非常相似。grass/`kernel.c` 中的 `proc_yield` 函数则与 `thread_yield` 类似。你将在 P2 中阅读并修改 `proc_yield`，并在 P3 中读完 grass/`kernel.c` 的剩余部分。事实上，完成 P3 后，你将彻底读懂 grass 目录下的所有代码。

TIP
> 提示

Here is a quote from the Wikipedia page for cooperative multitasking: "Cooperative multitasking was the primary scheduling scheme for 16-bit applications employed by Microsoft Windows before Windows 95 and Windows NT, and by the classic Mac OS."
> 以下是维基百科“协作式多任务”条目中的一段引用：“在 Windows 95 和 Windows NT 之前的 16 位 Microsoft Windows 应用程序以及经典的 Mac OS 中，协作式多任务是主要的调度方案。”
