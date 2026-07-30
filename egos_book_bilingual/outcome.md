# The Learning Outcomes

> **学习成果**

| Project | Pieces of egos-2000 |
| --- | --- |
| P0: Hello, World! | `library/libc` |
| P1: Cooperative Threads | `grass/process.h`, `grass/process.c` |
| P2: Preemptive Scheduling | `earth/cpu_intr.c`, `grass/kernel.s`, part of `grass/kernel.c` |
| P3: System Call & Protection | `grass/kernel.c`, `library/syscall`, `apps/system/sys_proc.c` |
| P4: Virtual Memory | `earth/cpu_mmu.c`, `library/elf` |
| P5: I/O Device Driver | `earth/dev_{tty, disk}.c`, `apps/user/video_demo.c` |
| P6: File System | `library/file`, `tools/mkfs.c`, `apps/system/sys_file.c` |
| P7: Ethernet & TCP/IP | `apps/user/udp_demo.c`, `apps/user/tcp_demo.c` |
| P8: Multicore & Locks | `earth/boot.{s,c}`, `grass/init.c`, `apps/system/sys_shell.c` |

| 项目 | egos-2000 的组成部分 |
| --- | --- |
| P0: Hello, World! | `library/libc` |
| P1: 协作式线程 | `grass/process.h`, `grass/process.c` |
| P2: 抢占式调度 | `earth/cpu_intr.c`, `grass/kernel.s`, `grass/kernel.c` 的一部分 |
| P3: 系统调用与保护 | `grass/kernel.c`, `library/syscall`, `apps/system/sys_proc.c` |
| P4: 虚拟内存 | `earth/cpu_mmu.c`, `library/elf` |
| P5: I/O 设备驱动 | `earth/dev_{tty, disk}.c`, `apps/user/video_demo.c` |
| P6: 文件系统 | `library/file`, `tools/mkfs.c`, `apps/system/sys_file.c` |
| P7: 以太网与 TCP/IP | `apps/user/udp_demo.c`, `apps/user/tcp_demo.c` |
| P8: 多核与锁 | `earth/boot.{s,c}`, `grass/init.c`, `apps/system/sys_shell.c` |

As we have stated at the start, the goal of this little book is to help students gain experience and confidence in creating their own operating system or similar computer systems in their future careers. As shown in the table above, each project helps students understand how to create one piece of egos-2000. Except for a few applications under apps, students finish reading all the code of egos-2000.

正如我们在开篇所指出的，这本小书的目标是帮助学生在未来的职业生涯中为构建自己的操作系统或类似计算机系统积累经验与信心。如上表所示，每个项目都帮助学生理解如何构建 egos-2000 的各个组成部分。除了 `apps` 目录下的少数几个应用程序外，学生将通读 egos-2000 的全部代码。
