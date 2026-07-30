Multicore & Locks | The EGOS book              Skip to content 

AppearanceMenuReturn to top























# Multicore & Locks ​

By far, we have been running the operating system and user applications on a single CPU core, but multicore CPUs are widely used. A laptop typically has a CPU with 4 to 16 cores, whereas a server CPU in a data center can have 64 or more cores. Multicore systems deliver higher software performance because multiple processes run on different cores simultaneously, enabling software tasks to make progress in parallel.

Handling multiple cores involves an important concept called mutual exclusion. In general, mutual exclusion prevents multiple CPU cores from using a shared resource simultaneously. For example, when the kernel code is running on a core and updating a data structure (e.g., the PCB), it may need exclusive access to that structure. Otherwise, kernel data structures can be corrupted by simultaneous updates by multiple cores. We thus start by introducing mutual exclusion.

## Mutual exclusion ​

When you run egos-2000 on QEMU, you may have noticed that the first line printed is one of the four possibilities listed below. In other words, a core is randomly selected from #0 .. #3 to boot egos-2000 when you run it on QEMU.
```
[CRITICAL] --- Booting on QEMU with core #0 ---
[CRITICAL] --- Booting on QEMU with core #1 ---
[CRITICAL] --- Booting on QEMU with core #2 ---
[CRITICAL] --- Booting on QEMU with core #3 ---
```


This is a typical example of mutual exclusion. Specifically, the public egos-2000 code only runs on a single core, so the booting code (aka. boot loader) will prevent the other 3 cores from running egos-2000 after egos-2000 has already started to run on a core. Such mutual exclusion requires a new class of CPU instructions, atomic memory operations, which is an extension to the basic RISC-V instruction set architecture.

### Atomic memory operation ​

The assembly code below is from earth/boot.s and shows the first instructions that all 4 CPU cores will execute when running egos-2000.asm
```
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
12345678910

Line 9 indicates that there is a 4-byte variable named boot_lock initialized to 0 before all cores execute the first instruction (i.e., the la instruction in boot_loader). The first two instructions load the address of boot_lock into register t0 and the value 1 into register t1. The magic happens at the amoswap.w.aq instruction, where amo stands for atomic memory operation. It atomically swaps the value of t1 with the 4 bytes at address t0. Atomicity means that only one of the 4 CPU cores would swap back the initial value 0 of boot_lock, after which all the other cores will swap back the value 1.

The first core to complete amoswap.w.aq will call boot(), and as long as boot_lock continues to hold the value 1, the other cores will be trapped into an infinite loop due to the bnez instruction at line 5.

TIP

The magic of amoswap.w.aq is enforced by the CPU hardware design. The CPU hardware determines which core executes this instruction first when multiple cores attempt to execute it simultaneously. This is closely related to the memory coherence problem in computer architecture.

We say that the code above acquires the boot lock, and one can release it by writing 0 to the address of boot_lock. After releasing the boot lock, another core would be able to acquire it and proceed with calling boot().

In general, a lock is simply a 4-byte variable in memory that holds either 0 or 1. Given a lock variable x in C, we have provided two macros for you in library/egos.h.c
```
/* The __sync_lock_* functions are defined within the C compiler. */
#define release(x) __sync_lock_release(&x);
#define acquire(x) while (__sync_lock_test_and_set(&x, 1) != 0);
```
123

While you still need to write some assembly code in this project, you can use these macros whenever you need to acquire or release a lock in your C code. Now release the boot lock and see what happens.

### A multicore boot loader ​

Since booted_core_cnt is initially 0, like boot_lock, the first booted core will enter the if branch in boot() and call 4 initialization functions: tty_init, disk_init, mmu_init, and intr_init. They initialize the TTY and disk devices, as well as the CSRs for virtual memory and interrupts. Lastly, boot() calls the grass_entry function in grass/init.c.

The grass_entry function loads the binary executable for apps/system/sys_proc.c into memory as the first process, and starts to run this process after the mret instruction. Note that after this mret, the first process will set the stack pointer to 0x80400000 as shown in apps/app.s. This means the first-booted core has finished using the kernel stack, so we can now safely release the boot lock and allow the next core to call boot().

Release the boot lock at the start of the main function in apps/system/sys_proc.c. Note that grass_entry() has passed the address of boot_lock to this process as an argument for its main function, you can access boot_lock through the struct multicore* boot argument. Here is one possible printout after you release the boot lock.
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
1234567891011

Essentially, the first process prints out Enter kernel process GPID_PROCESS normally, but the printing of [INFO] Load kernel process #2: sys_terminal is mixed with the printing of [SUCCESS] --- Core #4 starts running --- in earth/boot.c. This is a clear result of core #1 running the first process while core #4 runs boot() in parallel.

Next, we ask you to complete the boot loader code for multicore. Start with a fresh copy of egos-2000 and incorporate your virtual memory code from P4.

Your first task is to complete the code in apps/system/sys_proc.c and earth/boot.c, so all the cores finish booting. Specifically, the 3 cores that boot after the first one should set up their own CSRs for interrupts and virtual memory, but they do not reinitialize the devices. Note that each core owns its own set of CSRs. In particular, multicore requires page table translation, so simply do a FATAL in your code if SOFT_TLB has been chosen.

The goal is to see [SUCCESS] --- Core #? starts running --- for all 3 cores, so we know that all 4 cores have booted. However, you will likely meet exceptions and FATAL in excp_entry() in the kernel since we have not yet protected the kernel code with locks.

### A multicore kernel ​

As we mentioned, if multiple CPU cores access the kernel stack or update kernel data structures simultaneously, these memory regions can be corrupted. Therefore, we define kernel_lock in grass/kernel.s to ensure that at any time only one core can access the kernel stack or data structures. Your job is to protect the kernel by acquiring and releasing the kernel_lock.

Recall the trap_entry defined in grass/kernel.s, which is first introduced in P2. It is the entry point for all interrupts and exceptions, trapping a CPU core in the kernel. If multiple cores get trapped at the same time, they will all execute the instructions in trap_entry, thereby using the kernel stack at 0x80200000.

Intuitively, you will need to acquire the kernel lock before switching to the kernel stack and release it right before the mret in trap_entry. There is one key difference from how this is done in earth/boot.s. Recall that trap_entry saves all the registers on the kernel stack before calling kernel_entry, and restores them before mret. This requires that your code for acquiring or releasing the lock does not modify the value of any registers. This can be achieved by using the so-called scratch CSRs such as mscratch and ssratch. For example, to preserve the value of the sp register, trap_entry already uses the mscratch CSR to record the old value of sp.

After your modifications to grass/kernel.s, there is only one more thing you need to do in grass/kernel.c. Given multiple CPU cores, it is possible that a core must remain idle, i.e., the scheduler cannot find a RUNNABLE process to run on it. As a result, we need to ask an idle core to do nothing before it gets the next timer interrupt. You have seen this situation when implementing sleep in P3, and you need to handle it again in proc_yield.

TIP

We use a single lock to protect the whole kernel because it is simple. In many operating systems, there are separate kernel stacks for different CPU cores, so they don't need to be protected by locks. There are also different locks protecting the other kernel data structures.

## Running processes in parallel ​

Now, let us run multiple processes on different CPU cores and see whether our kernel can indeed handle multicore scheduling. You will implement the proc_coresinfo function at the end of grass/kernel.c and add this function into struct grass, so the shell can call this function for its built-in command coresinfo (see hints in apps/system/sys_shell.c). After you finish, run multiple processes in the background and try coresinfo.
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
123456789101112131415161718192021222324

In this demo, we started 4 processes in the background, and if we run coresinfo multiple times, different cores would be running different processes. For the first coresinfo above, processes #6, #7, and #9 were running, while #8 was not (i.e., RUNNABLE). You can also try the built-in killall command, after which all background loops will terminate. Here is a video with more details of this demo.

## Finding concurrency bugs ​

Being able to run a demo does not mean your code is bug-free. Concurrency bugs occur when multiple programs run concurrently. Here, a concurrency bug could be programs running on different cores modifying a certain kernel data structure at the same time. While we have protected the kernel with the kernel lock, such concurrency bugs still exist. See whether you can find some, trigger them, and fix them.

In general, concurrency bugs are difficult to find and debug because they typically happen neither deterministically nor often. Given that egos-2000 has a small codebase and uses simple data structures, a good way to eliminate concurrency bugs is to reason carefully about how all data structures are used.

TIP

If you wish to learn more about parallel programming in operating systems, this book is a fun read: Is Parallel Programming Hard, And, If So, What Can You Do About It?  Also, if you run your code on the Arty board, you need to add a few nops after each amoswap instruction. The reason is that the Arty board's CPU design has a flaw: it cannot complete the amoswap instruction in a single CPU cycle. By adding a few nops, the CPU core waits for amoswap to complete before proceeding into a critical section. The Tang Nano 20K board does not currently support multicore.

## Multithreading on multicore ​

Let's start by revisiting the producer-consumer code in P1.c
```
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
123456789101112131415161718192021

In P1, the producer and consumer functions run as two threads. They run on the same CPU core, which means a producer can read the count variable without worrying that it is simultaneously modified by another thread. If we allow threads to run on multiple cores, we will then have to protect the count variable with a lock.

Indeed, in a programming language like C++, a lock must be acquired before calling the conditional variable's cv_wait function. When running such a C++ program, the program would spawn multiple threads, and the OS would schedule them on different CPU cores. Your job is to implement a thread_create system call which allows a user application to spawn child threads. You can start by spawning threads that only print a few times, just like in P1, and you may reuse the exit function in library/syscall/servers.c when a thread terminates. Make sure that your kernel can schedule multiple threads on different cores.

Next, implement the conditional variable interface functions, cv_wait and cv_signal, for this multithreaded application. This is different from P1 in two ways:
* 

Each conditional variable now needs to associate with a lock.
* 

Threads are scheduled by the kernel, so a conditional variable in a user application can no longer maintain a queue of struct thread.

We leave the design of the conditional variable interface open-ended; feel free to design it yourself. You can refer to the POSIX interface for multithreading and conditional variables (i.e., pthread_cond_t). The goal is to run the producer and consumer functions as two threads within a single multithreaded application.

## A multithreaded web server ​

In P7, we disabled multicore support because handling PLIC interrupts across multiple cores can be tricky. Now is the time to figure out how to manage PLIC on multicore systems. With your P7 code, change the -smp 1 back to -smp 4, and make sure that the kernel can still reliably receive Ethernet packets in the intr_entry function in grass/kernel.c.

As another open-ended part of this project, we ask you to integrate your driver code with the multithreaded producer-consumer application. Intuitively, the Ethernet driver should be a producer that receives network packets from the Ethernet interface and buffers them in memory, with a separate consumer thread deciding how to handle them. This is the typical architecture of a web server application.

The OS concept we would like you to explore here is signal handling. Consider the SIGIO signal widely available on many operating systems. A web server can register a handler for the SIGIO signal via a special system call, which is not available in egos-2000 right now. When the kernel receives a PLIC interrupt from Ethernet, it should send a SIGIO to the web server, so the web server can run the registered handler after the kernel schedules it. This handler should then run the driver logic, decoding the receive descriptors and reading the Ethernet controller's receive buffers.

With signal handling, an OS can avoid putting a large amount of I/O device-driver logic into the kernel. Device drivers run in processes instead.

## Accomplishments ​

You have gained some experience with atomic memory operations, an extension to the RISC-V instruction set for multicore systems. You have seen how multiple cores run code in parallel, resulting in interleaved printing, and how to protect the kernel with a lock. You have tried to find and fix concurrency bugs that may occur infrequently or be nondeterministic.