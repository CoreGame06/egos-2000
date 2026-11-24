# Syscall 目录说明

`syscall.*` 负责把请求交给内核，`servers.*` 负责把系统服务封装成应用可以直接调用的函数。

| 文件 | 作用 |
| --- | --- |
| [syscall.h](syscall.h) | 定义 `struct syscall` 和内核处理的三类请求，也就是发送消息、接收消息和创建线程。 |
| [syscall.c](syscall.c) | 实现 `sys_send()`、`sys_recv()` 和 `thread_create()`。先填写参数页，再执行 `ecall` 进入内核。 |
| [servers.h](servers.h) | 定义各系统服务的 PID，以及进程、终端和文件服务的请求与回复格式。 |
| [servers.c](servers.c) | 实现 `exit()`、`sleep()`、`file_read()` 和终端读写等接口。例如 `file_read()` 给 `sys_file` 发送 `FILE_READ`，等它回复后把读到的数据复制给调用者。 |

`syscall.c.orig` 是旧版备份，`syscall.c.rej` 保存未应用的补丁，当前源码见 `syscall.c`。

## 参数页和进程间通信

`sc` 指向系统调用参数页，虚拟地址固定为 `SYSCALL_ARG`（`0x80301000`）。每个进程都用这个地址填写请求，各自的页表把它映射到各自的物理页。同一进程的多个线程共享页表，也就共享这一页。

以 A 给 B 发消息为例，内核复制消息的路径是：

```text
A 的参数页 → A 的 PCB.syscall → B 的 PCB.syscall → B 的参数页
```

PCB 是内核在 `proc_set[]` 中保存的进程或线程记录。进入内核后，`excp_entry()` 根据当前 PID 查页表，把参数页里的请求复制到 PCB 的 `syscall` 字段。B 还没请求接收时，消息先留在 A 的 PCB 里。匹配后，内核复制消息，B 的 `sys_recv()` 再从自己的参数页取出实际发送者和内容。相关代码在 [kernel.c](../../grass/kernel.c)。

上面是通过内核复制消息实现的 IPC。对照真正的**共享内存**，Shell 和 `cd` 把工作目录映射到同一个物理页，`cd` 写完后，Shell 直接读那一页就能看到变化。见 [app.h](../../apps/app.h)。

## 创建线程和线程栈

`thread_create(entry, arg)` 把入口函数、参数和栈顶交给内核。内核为新线程找一个空闲 PCB，沿用原进程的 PID 和页表，并设置新线程的 PC、参数寄存器和栈指针。

当前用户库每个进程只预留一块 `thread_stack[4096]`。每次创建都传入同一个栈顶，所以两个工作线程同时使用这块内存会覆盖彼此的栈数据。生产者消费者示例只创建一个工作线程，主线程用自己的栈。见 [producer_consumer.c](../../apps/user/producer_consumer.c)。

Linux 上调用 `pthread_create()` 时，glibc 会为新线程准备独立的栈区域，再通过 `clone` 一类的系统调用创建线程。各线程共享进程地址空间，同时有自己的栈和线程 ID。

## 终端服务和串口

QEMU 通过模拟串口连接 egos 和宿主机终端，用来输入命令、输出文字。

- 普通应用通过消息请求 `sys_terminal` 读写串口。例如 `printf()` 把文字发给它，由它写串口。
- Shell 和其它系统服务编译时定义了 `KERNEL`，所以直接调用 Earth 的串口接口。它们仍在用户态运行。
