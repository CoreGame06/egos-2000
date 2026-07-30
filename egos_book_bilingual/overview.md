# Core projects

> **核心项目**

There are 6 core projects that cover key OS concepts for an undergraduate OS course, such as Cornell CS4411. The projects introduce the architectural support for OS one by one, which students have found very helpful. For example, students will play with the timer and scheduler without any knowledge of privilege modes or page tables.

共有 6 个核心项目，涵盖了本科生操作系统课程（如康奈尔大学 CS4411）的核心概念。这些项目循序渐进地引入操作系统所需的硬件架构支持，学生们反馈这种方式非常有帮助。例如，学生可以在完全不需要了解特权模式或页表的前提下，动手实践定时器与调度器。

| Project / 项目 | OS concepts / 操作系统概念 | Architectural support / 硬件架构支持 |
> | --- | --- | --- |

| P0: Hello, World! | code, data, heap, stack`<br>`代码段、数据段、堆、栈 | user-level ISA`<br>`用户级 ISA |
> | P1: Cooperative Threads / 协作式线程 | thread, yield, context switch`<br>`线程、让出 (yield)、上下文切换 | |

| P2: Preemptive Scheduling / 抢占式调度 | interrupt and exception handling`<br>`中断与异常处理 | control register, timer`<br>`控制寄存器、定时器 |
> | P3: System Call & Protection / 系统调用与保护 | inter-process communication`<br>`进程间通信 | privilege mode`<br>`特权模式 |

| P4: Virtual Memory / 虚拟内存 | process, virtual address space`<br>`进程、虚拟地址空间 | page table translation`<br>`页表转换 |
> | P6: File System / 文件系统 | block, inode, file, directory`<br>`数据块、inode、文件、目录 | |

### Advanced projects

> **进阶项目**

There are 3 advanced projects for a graduate-level OS course, such as Cornell CS5411 or Northeastern CS6640.

共有 3 个进阶项目，适用于研究生阶段的操作系统课程（如康奈尔大学 CS5411 或东北大学 CS6640）。

| Project / 项目 | OS concepts / 操作系统概念 | Architectural support / 硬件架构支持 |
| --- | --- | --- |
| P5: I/O Device Driver / I/O 设备驱动 | memory-mapped I/O`<br>`内存映射 I/O | I/O bus, direct memory access`<br>`I/O 总线、直接内存访问 (DMA) |
| P7: Ethernet & TCP/IP / 以太网与 TCP/IP | I/O interrupt, IRQ, UDP`<br>`I/O 中断、IRQ、UDP | platform-level interrupt controller`<br>`平台级中断控制器 (PLIC) |
| P8: Multicore & Locks / 多核与锁 | boot loader, mutual exclusion`<br>`引导加载程序 (boot loader)、互斥 | atomic memory operation`<br>`原子内存操作 |

All the projects can run on the QEMU emulator as well as real RISC-V hardware. Now, let's get our hands dirty and have fun with OS!

所有项目均可在 QEMU 模拟器以及真实 RISC-V 硬件上运行。现在，让我们动手实践，感受开发操作系统的乐趣吧！
