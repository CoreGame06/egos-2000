System Call & Protection | The EGOS book               Skip to content 

AppearanceMenuReturn to top























# System Call & Protection ​

You have seen how an operating system manages the life cycles of threads (or processes, since we don't distinguish between them yet) and preemptively schedules multiple threads using timer interrupts. This project now helps you understand two more things.
* How do threads invoke system calls to communicate with each other?
* How does an operating system protect its memory, so malicious threads cannot corrupt the system calls by modifying the code or data of the operating system?

System calls and memory protection are supported by exception handling, which is similar to interrupt handling. To begin, we introduce exception handling.

## Exception handling ​

An exception happens if something goes wrong when the CPU executes an instruction. For example, an exception occurs when an instruction attempts to access memory at an invalid address. Instead of ignoring the problem and proceeding to the next instruction, the CPU automatically jumps to a special function called an exception handler, just as it jumps to the interrupt handler when receiving a timer interrupt. We will use the same function to handle both interrupts and exceptions, and use the mcause CSR to show why the CPU jumped to the handler.

### The mcause CSR ​

Below are screenshots of Tables 22 and 23 from this manual, which describe the mcause CSR. You can also read Chapter 3.1.15 of the RISC-V Reference Manual. When an exception or interrupt occurs, the CPU sets mcause before jumping to the handler.



For example, mcause is set to 0x80000007 when the CPU receives a timer interrupt. Bit#31 of mcause is set to 1 because the timer interrupt is an interrupt. The exception code is set to 0b0000000111 because 7 is the machine timer interrupt code. Similarly, when the CPU encounters an illegal instruction, meaning the 4 bytes pointed to by the program counter cannot be decoded into a CPU instruction, mcause is set to 0x2 before the CPU jumps to the handler.

TIP

Exceptions are different from interrupts. Exceptions are triggered by CPU instructions that cause something wrong. Interrupts are triggered by devices outside of the CPU, such as a timer, a disk, or a network interface controller. The mcause CSR helps the operating system understand which interrupt or exception needs to be handled.

### The ecall exception ​

Most exceptions occur due to errors, but RISC-V provides a special instruction, ecall, which intentionally triggers the so-called environment call exception (see exception #8 and #11 in Table 23). This instruction is for system calls: when a thread invokes ecall, the control flow transfers to the operating system, which serves a system call for that thread.

You can find ecall in library/syscall/syscall.c of egos-2000. After asm("ecall") raises an environment call exception, the CPU jumps to trap_entry, as you saw in P2. trap_entry calls kernel_entry, which calls excp_entry, and excp_entry handles system calls in this condition: (id >= EXCP_ID_ECALL_U && id <= EXCP_ID_ECALL_M). EXCP_ID_ECALL_U and EXCP_ID_ECALL_M are defined as 8 and 11, respectively, according to Table 23. Read trap_entry and kernel_entry yourself and see how mcause is used.

TIP

The U-Mode and M-mode in Table 23 stand for user mode and machine mode. We will cover these privilege modes very soon when we start explaining memory protection later in this project.

### A sketch of the "kernel" ​

We have been using the term kernel since P2, but we have never explained what it is. Now that you know the mcause CSR, we can show you a sketch of the kernel.c
```
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
1234567891011

The code above sketches the core of an OS kernel:
* handle thread scheduling upon a timer interrupt
* handle system calls when a thread invokes ecall
* handle other exceptions such as memory access faults

This is just a sketch, since a complete OS must handle all interrupts and exceptions; these three items are probably the most important. You have seen thread scheduling in P2, and P3 will give you hands-on experience with system calls and memory access faults.

## Inter-process communication ​

There are only 2 types of system calls in egos-2000. They are designed for inter-process communication, meaning sending and receiving messages. Next, we introduce the system call interface for applications, and then explain what happens within the OS kernel.

### Application-side interface ​

The code below is from library/syscall/syscall.h, and it defines the data structures for system calls in egos-2000.c
```
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
123456789101112

The content field holds the message being sent or received. Say process A wants to send a message to process B through SYS_SEND; this system call may not succeed immediately. It succeeds only after process B invokes SYS_RECV with process A as the sender, meaning process B is ready to receive a message from process A. For this reason, before process B invokes SYS_RECV, the SYS_SEND system call made by process A has a status of PENDING rather than DONE.

TIP

In other words, egos-2000 implements a blocking version of inter-process communication, so a system call returns only after a message has been successfully sent or received. It is also possible to implement a non-blocking version in which system calls return immediately. egos-2000 uses the blocking version for code simplicity.

With this struct syscall in mind, the system call interface sys_send and sys_recv in library/syscall/syscall.c should be easy to understand.c
```
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
12345678910111213141516

Again, the ecall highlighted above triggers an environment call exception, and the CPU then jumps to the exception handler, trap_entry, right after the ecall instruction.

### Kernel-side handling ​

The exception handler in egos-2000, trap_entry, calls kernel_entry(), which then calls excp_entry() after an environment call exception is raised by ecall. We now explain the following if-statement for system calls.c
```
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
12345678910111213

First of all, proc_set[curr_proc_idx] is the struct representing the current process that has just invoked the system call.
* 

Note that this process initialized the syscall struct at memory address SYSCALL_ARG. Lines #3 and #4 copy the data structure into the kernel. You can ignore earth->mmu_translate for now; we will explain it in P4. Line #6 sets the system call status to PENDING, and line #8 sets the process status to PROC_PENDING_SYSCALL.
* 

Recall that mepc stands for the program counter when the exception occurs, and the value of mepc is read into proc_set[curr_proc_idx].mepc in function kernel_entry. Therefore, line #9 says that after the system call completes, the kernel should return to the instruction immediately after ecall (i.e., skip the 4-byte ecall instruction).
* 

Line #10 attempts to process the SYS_SEND or SYS_RECV system call for the current process, and line #11 finds the next process to schedule. proc_try_syscall is also called in proc_yield because the scheduler repeatedly attempts to process a pending system call until it succeeds.

Read proc_try_syscall(), proc_try_send() and proc_try_recv(). There are only ~40 lines of code, but they gracefully handle inter-process communication. At a high level, if a process has a pending system call, proc_try_syscall() will retry the system call and, if it succeeds, set the process status to PROC_RUNNABLE.

TIP

At this point, you have finished reading grass/kernel.c, including kernel_entry, intr_entry, excp_entry, proc_yield, proc_try_syscall, proc_try_send, and proc_try_recv.

### Introduce process sleep ​

After getting familiar with the system call control flow, we now ask you to use system calls to introduce process sleep. As shown in library/syscall/servers.h, the GPID_PROCESS process in egos-2000 accepts 3 message types to spawn and terminate processes. Your job is to add a fourth message type, PROC_SLEEP, to the struct proc_request so that the process that sends this message to GPID_PROCESS will sleep for a specified amount of time before being scheduled again. Start from a fresh copy of egos-2000, and add the following code as a new file apps/user/sleep.c.c
```
#include "app.h"

int main() {
  const uint usec_cnt = 5000000;
  printf("Start to sleep for %d microseconds.\n\r", usec_cnt);
  sleep(usec_cnt);
  printf("Woke up again after %d microseconds.\n\r", usec_cnt);
}
```
12345678

Then run make qemu, and sleep will be automatically added as a user command:
```
> make qemu
...
➜ /home/yunhao sleep
Start to sleep for 5000000 microseconds.
Woke up again after 5000000 microseconds.
```
12345

For now, you will see the second line printed immediately after the first because the sleep function in library/syscall/servers.c has not been implemented. You shall see the second line 5 seconds after the first line when you complete the following steps.
* 

Update the struct proc_request and the sleep function mentioned above so that this sleep function sends a PROC_SLEEP message to the GPID_PROCESS process.
* 

In apps/system/sys_proc.c, add a case for PROC_SLEEP and put debug printing there temporarily, so you know that GPID_PROCESS succeeds in receiving the message.
* 

Add the proc_sleep function in grass/process.c to the grass layer interface (struct grass in library/egos.h), and initialize it in grass/init.c, just like proc_alloc.
* 

Invoke grass->proc_sleep in the PROC_SLEEP case you have just added in step 2. Then add debug printing in the proc_sleep function of grass/process.c, so you know that proc_sleep is called by GPID_PROCESS with the correct pid and usec arguments.
* 

Implement this proc_sleep function, which should put the process identified by pid to sleep for usec microseconds. This involves a few modifications to the kernel.
* Add one or more fields to struct process, and initialize them in proc_alloc().
* Modify these fields for process pid in proc_sleep(). In addition to argument usec, you need mtime_get(), which returns the clock time in 10^-7 seconds (on QEMU).
* Modify proc_yield() to schedule a process only if it is not sleeping, using the fields in struct process and the latest clock time from mtime_get().
* 

The kernel may now encounter a situation in which no process can be scheduled. You need to handle this situation by replacing the FATAL in proc_yield() with your code.
* 

Remove the debug printings. Run sleep again in the egos-2000 shell, and you shall see the Woke up ... printing 5 seconds after the first line of printing.

## Protect the OS memory ​

By far, all the code we have seen runs in the so-called machine mode, which means it can freely access memory. However, user applications should not be able to read or write memory freely. Otherwise, a malicious application can corrupt the kernel's memory, causing damage. At a high level, we now ask you to do 3 things:
* Specify the memory regions that code in the user mode is allowed to access.
* Run the code of all user applications in user mode rather than machine mode.
* Terminate a user application if it triggers an exception by reading or writing the memory at an address outside of the allowed regions.

### Set up a PMP region ​

Read through chapter 3.7 of the RISC-V reference manual for Physical Memory Protection (PMP), and then write your code in earth/cpu_mmu.c:c
```
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
1234567891011121314

TIP

Your code should overwrite the two CSRs pmpaddr0 and pmpcfg0, so the 4GB region no longer takes effect and is replaced by the 2MB region [0x80200000, 0x80400000). As a result, you will only be able to choose software TLB when booting egos-2000 in the rest of P3.

Specifically, code running in the user mode cannot access any memory region by default. After you finish the PMP code above, code running in user mode will be able to access two memory regions:
* [0x80200000, 0x80400000)—it contains the code, data, heap, and stack of the current process. Set the permissions to read, write, and execute.
* [0x80102000, 0x80103000)—it contains the shell's current working directory. Set the permissions to read and write, but not execute.

However, PMP won't take any effect if we still run everything in machine mode, so we need to switch privilege modes when switching the CPU context from the kernel back to a user application process.

### Switch privilege modes ​

You need to understand mstatus.MPP and update mstatus.MPP in proc_yield according to the comments there. Recall that mstatus.MPP stands for bit#11 and bit#12 of mstatus:



You will need to set these bits to 11 if the next scheduled process is a kernel process (i.e., pid < GPID_USER_START), or set them to 00 for all other processes. In RISC-V, 0 stands for user mode, and 3 (i.e., 11 in binary) stands for machine mode. To see how it works, we need to explain what happens when entering and exiting the kernel.

Upon an interrupt or exception, the CPU enters the kernel and automatically switches the privilege mode to machine mode before jumping to the trap_entry handler. This allows the kernel to run in machine mode and freely access the memory.

Upon executing mret in grass/kernel.s, the CPU exits the kernel, and mret will switch the privilege mode according to mstatus.MPP. Therefore, if we set mstatus.MPP to 00 in proc_yield, the next scheduled process will run in user mode after the mret instruction.

### Kill malicious applications ​

To test whether you correctly set the PMP region and switched privilege modes, we have provided 2 malicious applications, crash1 and crash2, in the apps/user directory. The malicious applications would halt the whole operating system by corrupting the memory.
```
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
1234567891011

Note that this FATAL happens at the end of function excp_entry. Your final task in P3 is to implement the following part of excp_entry.c
```
static void excp_entry(uint id) {
    ...
    /* Student's code goes here (System Call & Protection | Virtual Memory). */

    /* Kill the current process if curr_pid is a user application. */

    /* Student's code ends here. */
    FATAL("excp_entry: kernel got exception %d", id);
}
```
123456789

After excp_entry gracefully kills the malicious applications, you should see the following.
```
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
123456789

In other words, memory protection should work: malicious applications running in user mode trigger memory exceptions when attempting to corrupt memory. The kernel kills these malicious applications when handling such exceptions.

## Accomplishments ​

In terms of OS concepts, you have learned about exception handling, system calls, privilege modes, and inter-process communication. In terms of code reading, you have completed all the code in grass and library/syscall. The grass layer is the kernel in egos-2000.

You will read earth/cpu_mmu.c and library/elf/* in P4, read earth/dev_disk.c and earth/dev_tty.c in P5, and read library/file/* in P6. Then you will essentially have read all the code for egos-2000. We are halfway there!