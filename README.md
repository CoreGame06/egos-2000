# egos-2000: Earth and Grass Operating System

**egos-2000** 是一个代码量约 2000 行的轻量级教学操作系统。它运行在 **RISC-V 32 位（rv32ima）** 架构上，支持 QEMU 4 核模拟（`-M virt -smp 4`）以及真实 RISC-V/FPGA 开发板（如 TangNano 20K、Arty A7 等）。

该项目采用清晰的三层微内核架构，完整实现了操作系统各个核心子系统，包括裸机引导、虚拟内存管理、多核抢占式调度、进程间通信（IPC）、文件系统及常用用户态命令。

---

## 🏛️ 三层架构设计 (Earth, Grass, Applications)

项目名称 "egos" 来源于其独特的 **Earth - Grass - OS (Apps)** 三层分层架构：

```
┌─────────────────────────────────────────────────────────────┐
│  Application Layer (应用层 / apps/)                         │
│  - 系统服务：sys_proc, sys_file, sys_terminal, sys_shell    │
│  - 用户命令：ls, cat, cd, echo, sleep, producer_consumer...  │
│  - 通过 struct grass 接口发起系统调用与 IPC 通信            │
├─────────────────────────────────────────────────────────────┤
│  Grass Layer (微内核层 / grass/)                            │
│  - 硬件无关的内核核心 (kernel.c / kernel.s / process.c)     │
│  - 进程控制块 (PCB) 状态机、多级反馈队列 (MLFQ) 抢占式调度   │
│  - 提供 sys_send / sys_recv 同步消息传递与多核同步          │
├─────────────────────────────────────────────────────────────┤
│  Earth Layer (硬件抽象层 / earth/)                          │
│  - 硬件相关的底层封装 (boot.c, cpu_intr.c, cpu_mmu.c...)    │
│  - CSR 寄存器控制、Sv32 页表与 Soft TLB、定时器与外部中断   │
│  - 串口 (UART)、SD 卡 (SDHCI DMA / SPI)、网卡 (e1000) 驱动  │
└─────────────────────────────────────────────────────────────┘
```

各层之间通过定义在 [`library/egos.h`](library/egos.h) 中的 `struct earth` 和 `struct grass` 函数指针表进行解耦调用，实现了清晰的抽象与隔离。

---

## ✨ 主要功能特性

* **多核裸机引导与同步 (Boot & SMP)**：支持 4 核并行启动，使用原子指令与自旋锁实现多核初始化与任务分发。
* **虚拟内存管理 (Memory Management)**：
  * 支持 **Sv32 两级硬件页表** 与 **Soft TLB (软件换入换出)** 两种地址翻译模式。
  * 提供物理页分配器（`mmu_alloc` / `mmu_free`）与 PMP (物理内存保护) 配置。
* **抢占式调度 (Preemptive Scheduling)**：基于 CLINT 硬件时钟中断驱动，实现 5 级多级反馈队列（MLFQ）算法，支持进程动态降级与周期性防饥饿重置。
* **微内核 IPC 机制**：基于 `SYS_SEND` / `SYS_RECV` 实现轻量级同步消息传递，用户态服务（进程、文件、终端）均通过 IPC 解耦协作。
* **文件系统 (File System)**：FAT 风格的简单文件系统，支持 Inode 管理、目录树遍历与数据块读写，支持元数据缓存优化。
* **外设驱动支持**：包含 UART 串口、SD 卡（支持 DMA 读写）、Intel e1000 以太网卡及 VGA 显示。

---

## 🗺️ 内存布局 (Memory Layout)

系统物理内存共 6MB（起始地址 `0x80000000`），采用统一虚拟地址空间映射：

```
0x80600000 ┌─────────────────────────────────────── RAM_END (6MB 物理内存上限)
           │ VGA 显存 / 硬件 Framebuffer
0x80400000 ├─────────────────────────────────────── APPS_PAGES_BASE / APPS_STACK_TOP
           │ 2MB 物理页池 (512 个 4KB 页，供 mmu_alloc 分配)
           │ 同时为用户进程栈顶 (向下生长)
0x80301000 ├─────────────────────────────────────── SYSCALL_ARG (struct syscall 共享参数页)
0x80300000 ├─────────────────────────────────────── APPS_ARG (argc / argv 参数页)
0x80200000 ├─────────────────────────────────────── APPS_ENTRY / EGOS_STACK_TOP
           │ 1MB 应用程序代码与数据段入口
           │ 同时为内核栈顶 (Trap 时 SP 切换至此)
0x80102000 ├─────────────────────────────────────── SHELL_WORK_DIR (当前工作目录共享页)
0x80101000 ├─────────────────────────────────────── GRASS_STRUCT (struct grass 函数指针)
0x80100000 ├─────────────────────────────────────── EARTH_STRUCT (struct earth 函数指针)
0x80000000 └─────────────────────────────────────── RAM_START (内核代码与数据)
```

---

## 📁 目录结构

```
egos-2000/
├── earth/          # 硬件抽象层：引导、MMU、中断与设备驱动
├── grass/          # 内核层：Trap 汇编、进程调度 (MLFQ)、系统调用分发
├── apps/           # 应用层：包含系统服务 (system/) 与用户程序 (user/)
├── library/        # 共享库：libc、ELF 加载器、文件系统 (file/)、syscall
├── tools/          # 工具集：磁盘镜像生成工具 (mkfs) 与固件
├── notes/          # 项目架构深度剖析与学习笔记
└── Makefile        # 构建脚本与 QEMU 启动配置
```

---

## 🚀 快速开始

### 依赖要求
* **交叉编译器**：`riscv-none-elf-gcc` 或 `riscv32-unknown-elf-gcc`
* **模拟器**：`qemu-system-riscv32`

### 编译与运行

```bash
# 1. 编译内核、用户应用并生成磁盘与 ROM 镜像
make install

# 2. 在 QEMU 中启动 4 核模拟
make qemu

# 3. 清理构建生成文件
make clean
```

### 运行截图示例

```text
-------- Simulate on QEMU-RISCV --------
[INFO] Booting egos-2000 on RISC-V (4 cores)...
[SUCCESS] Initialized Earth Layer.
[INFO] Loading sys_proc...
--------------------------------------------------
Welcome to egos-2000!
--------------------------------------------------
➜ ~ ls
bin/   home/  README
➜ ~ cat README
➜ ~ coresinfo
==============Core ID / Process ID==============
Core #0 is running pid=4
Core #1 is running pid=0
Core #2 is running pid=0
Core #3 is running pid=0
```

---

## 📄 许可证与致谢

* 本项目基于 Cornell University 的 **egos-2000** 教学操作系统构建。
* 遵循 MIT 开源许可证。
