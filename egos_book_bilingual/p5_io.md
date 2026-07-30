# I

> **O Device Driver / I/O 设备驱动**

At this point, you have finished reading all the kernel code of egos-2000 under the grass directory. You have also finished `cpu_intr.c` and `cpu_mmu.c` under the earth directory, which handle interrupts, exceptions, and virtual memory.
> 至此，你已经阅读完了 grass 目录下 egos-2000 的所有内核代码，也完成了 earth 目录下负责处理中断、异常和虚拟内存的 `cpu_intr.c` 与 `cpu_mmu.c` 的阅读。

In this project, you will read `dev_tty.c` and `dev_disk.c` under the earth directory. They contain the driver code for the terminal (keyboard input & screen output) and disk devices. Together, the two files have fewer than 170 lines of code, but they provide good examples of an important concept to learn in this project: memory-mapped I/O.
> 在本实验项目中，你将阅读 earth 目录下的 `dev_tty.c` 和 `dev_disk.c`。它们包含了终端（键盘输入与屏幕输出）及磁盘设备的驱动程序代码。这两个文件加起来的代码量不足 170 行，但它们为理解本实验项目中的一个重要概念——内存映射 I/O（memory-mapped I/O）——提供了极佳的示例。

## I

> **O bus and device / I/O 总线与设备**

An I/O bus connects various devices to the CPU. There are different types of I/O buses, and this project introduces a few buses connecting to the terminal and disk devices. In general, computers need to read keyboard input and print characters on a screen. There used to be a separate terminal device, separate from the computer's main body, that handled these functions. This is a photo of the VT100 terminal taken at the Computer History Museum.
> I/O 总线将各种设备连接到 CPU。I/O 总线有不同的类型，本实验项目将介绍连接终端和磁盘设备的几种总线。通常情况下，计算机需要读取键盘输入并在屏幕上打印字符。过去，曾有独立于计算机主机的终端设备专门用来处理这些功能。这是一张在计算机历史博物馆拍摄的 VT100 终端的照片。

### UART and terminal

> **UART 与终端**

In egos-2000, a terminal is connected to the CPU via a Universal Asynchronous Receiver/Transmitter (UART). UART uses two hardware pins on the CPU—one receiving bytes and the other transmitting (i.e., sending) bytes.
> 在 egos-2000 中，终端通过通用异步收发器（Universal Asynchronous Receiver/Transmitter, UART）连接到 CPU。UART 使用 CPU 上的两个硬件引脚——一个用于接收字节，另一个用于传输（即发送）字节。

When a key is pressed on the keyboard, the terminal sends the corresponding character as a byte over UART to the CPU, and the OS reads this byte from the CPU's Receiver hardware pin. When the OS prints a character, it sends a byte through the CPU's Transmitter pin, and the UART passes the byte to the terminal. Asynchronous in UART means that electrical signals on the two hardware pins do not wait for each other.
> 当按下键盘上的某个键时，终端会将对应的字符作为一个字节通过 UART 发送到 CPU，操作系统从 CPU 的接收（Receiver）硬件引脚读取该字节。当操作系统打印一个字符时，它通过 CPU 的发送（Transmitter）引脚发送一个字节，UART 将该字节传递给终端。UART 中的“异步（Asynchronous）”意味着这两个硬件引脚上的电信号不会相互同步等待。

The code below shows how egos-2000 connects to a terminal device using UART.
> 以下代码展示了 egos-2000 如何使用 UART 连接到终端设备。

```c
#define UART_BASE   0x10000000UL
#define LINE_STATUS 5UL

void uart_getc(char* c) {
    while (!(REGB(UART_BASE, LINE_STATUS) & (1 << 0)));
    *c = REGW(UART_BASE, 0) & 0xFF;
}

void uart_putc(char c) {
    while (!(REGB(UART_BASE, LINE_STATUS) & (1 << 5)));
    REGW(UART_BASE, 0) = c;
}
```

The CPU uses a special memory region at `0x10000000` to communicate with the terminal. When a byte is received, bit #0 of the line status register at `0x10000005` will be set to 1 by the CPU. After detecting such a byte, `uart_getc` reads it from address `0x10000000`. When sending a byte, `uart_putc` waits for the UART to be idle (i.e., the previous byte has been sent) by checking bit #5 of the line status register. After the UART is idle, `uart_putc` writes the byte to be printed on the terminal screen to address `0x10000000`.
> CPU 使用 `0x10000000` 处的一块特殊内存区域来与终端通信。当接收到一个字节时，CPU 会将位于 `0x10000005` 的线路状态寄存器的第 0 位置 1。在检测到该字节后，`uart_getc` 会从地址 `0x10000000` 读取它。当发送字节时，`uart_putc` 通过检查线路状态寄存器的第 5 位来等待 UART 变为空闲状态（即前一个字节已发送完成）。在 UART 空闲后，`uart_putc` 将要在终端屏幕上打印的字节写入地址 `0x10000000`。

The code looks simple because most of the complexities are hidden by the hardware. For example, when running egos-2000 on a RISC-V board and using the screen command on MacOS as the terminal, the UART/USB bridge chip shown below converts electrical signals between UART and Universal Serial Bus (USB), so egos-2000 does not have to handle the complexity of USB. In real-world operating systems, the USB driver code can be much more complex than the UART code above.
> 代码看起来很简单，是因为绝大多数复杂性都被硬件掩盖了。例如，当在 RISC-V 开发板上运行 egos-2000 并在 macOS 上使用 screen 命令作为终端时，下图所示的 UART/USB 桥接芯片会在 UART 和通用串行总线（Universal Serial Bus, USB）之间转换电信号，因此 egos-2000 无须处理 USB 的复杂性。在现实世界的操作系统中，USB 驱动程序代码要比上面的 UART 代码复杂得多。

This UART driver code also serves as an example of memory-mapped I/O. Specifically, a hardware manufacturer can define special memory regions for controlling I/O devices, and different manufacturers can define different regions. Indeed, egos-2000 can run on both QEMU and RISC-V boards, which use different UART regions. RISC-V boards use the region starting at `0xF0001000` to control the UART, and the driver code for RISC-V boards is also slightly different from the QEMU code in `dev_tty.c`.
> 这段 UART 驱动程序代码也是内存映射 I/O 的一个典型示例。具体而言，硬件厂商可以定义专门的内存区域来控制 I/O 设备，不同厂商可以定义不同的区域。事实上，egos-2000 既可以运行在 QEMU 上，也可以运行在 RISC-V 开发板上，两者使用了不同的 UART 内存区域。RISC-V 开发板使用以 `0xF0001000` 开头的区域来控制 UART，并且 RISC-V 开发板的驱动代码也与 `dev_tty.c` 中用于 QEMU 的代码略有不同。

### SPI and SD card

> **SPI 与 SD 卡**

A computer typically needs a disk to store data when powered off, and egos-2000 uses an SD card as the disk. On a RISC-V board, an SD card is connected to the CPU via the Serial Peripheral Interface (SPI), which uses four hardware pins on the CPU, as illustrated in this Wikipedia image.
> 计算机在关机时通常需要磁盘来存储数据，而 egos-2000 使用 SD 卡作为磁盘。在 RISC-V 开发板上，SD 卡通过串行外设接口（Serial Peripheral Interface, SPI）连接到 CPU，该接口使用了 CPU 上的四个硬件引脚，如维基百科的这幅插图所示。

Consider the CPU as the SPI Main and the SD card device as the SPI Sub. Both sides have four hardware pins, and their functionalities are described below.
> 将 CPU 视为 SPI 主设备（SPI Main），将 SD 卡设备视为 SPI 从设备（SPI Sub）。双方各有四个硬件引脚，其功能描述如下：

* Chip Select (CS) resets the SD card before use.
> * 片选（Chip Select, CS）：在使用前重置 SD 卡。

* Serial Clock (SCLK) provides clock signals from the CPU (e.g., 20MHz).
> * 串行时钟（Serial Clock, SCLK）：提供来自 CPU 的时钟信号（例如 20MHz）。

* Main Out Sub In (MOSI) is used by the CPU to send bytes to the SD card.
> * 主出从入（Main Out Sub In, MOSI）：CPU 用来向 SD 卡发送字节。

* Main In Sub Out (MISO) is the SD card's interface for sending bytes to the CPU.
> * 主入从出（Main In Sub Out, MISO）：SD 卡向 CPU 发送字节的接口。

As with UART, the CPU provides memory-mapped I/O regions for communicating with the SD card via SPI. Unlike UART, the SPI Main and Sub exchange bytes synchronously during communication. The code below explains how it works.
> 与 UART 一样，CPU 提供了内存映射 I/O 区域，用于通过 SPI 与 SD 卡通信。与 UART 不同的是，SPI 主设备和从设备在通信过程中是同步交换字节的。以下代码解释了其工作原理。

```c
static char spi_exchange(char byte) {
    /* The "exchange" here means sending a byte and then receiving a byte. */
    REGW(SDSPI_BASE, LITEX_SPI_MOSI)    = byte;
    REGW(SDSPI_BASE, LITEX_SPI_CONTROL) = (8 * (1 << 8) | (1));

while ((REGW(SDSPI_BASE, LITEX_SPI_STATUS) & 1) != 1);
    return (char)(REGW(SDSPI_BASE, LITEX_SPI_MISO) & 0xFF);
}
```

First, the byte is transmitted via the MOSI pin. Ignore `LITEX_SPI_CONTROL`, which is hardware-specific. After sending the byte, SPI immediately receives a byte from the MISO pin (i.e., from the SD card) as the return value of `spi_exchange`. The while loop waits for the arrival of a byte, just like the loop in `uart_getc`. SPI always receives a byte after sending one (i.e., synchronous), whereas UART is asynchronous.
> 首先，字节通过 MOSI 引脚发送出去。忽略与特定硬件相关的 `LITEX_SPI_CONTROL`。发送完字节后，SPI 会立即从 MISO 引脚（即从 SD 卡）接收一个字节作为 `spi_exchange` 的返回值。while 循环等待字节到达，就像 `uart_getc` 中的循环一样。SPI 总是发送一个字节后再接收一个字节（即同步传输），而 UART 则是异步传输。

While `spi_exchange` uses the MOSI and MISO SPI pins, the code below controls the clock signals sent out to the SCLK pin from the CPU.
> 在 `spi_exchange` 使用 MOSI 和 MISO 两个 SPI 引脚的同时，以下代码控制从 CPU 发送到 SCLK 引脚的时钟信号。

```c
#define CPU_CLOCK_RATE 100000000 /* 100MHz */
INFO("Set the SPI clock to 20MHz for the SD card");
REGW(SDSPI_BASE, LITEX_SPI_CLKDIV) = CPU_CLOCK_RATE / 20000000 + 1;
```

> [!TIP]
> As an exercise, read the `sdspi_init` function in `dev_disk.c` from which you will see how SCLK is controlled during the initialization of an SD card.
> 作为练习，请阅读 `dev_disk.c` 中的 `sdspi_init` 函数，你将在其中看到在 SD 卡初始化过程中是如何控制 SCLK 的。

An SD card command is 6 bytes, so the operating system can ask the SD card to perform a specific task by sending the corresponding 6-byte command via `spi_exchange`. Given an SD card command, the `sdspi_exec_cmd` function sends the 6 bytes and then waits for the SD card's reply until a timeout occurs.
> 一条 SD 卡命令长度为 6 个字节，因此操作系统可以通过 `spi_exchange` 发送相应的 6 字节命令来让 SD 卡执行特定任务。给定一条 SD 卡命令，`sdspi_exec_cmd` 函数发送这 6 个字节，然后等待 SD 卡的回复，直到发生超时。

```c
static char sdspi_exec_cmd(char* cmd) {
    /* Send a 6-byte SD card command through the SPI bus. */
    for (uint i = 0; i < 6; i++) spi_exchange(cmd[i]);
    #define TIME_OUT 8000
    for (uint reply, i = 0; i < TIME_OUT; i++)
        if ((reply = spi_exchange(0xFF)) != 0xFF) return reply;

return 0xFF;
}
```

### SD card read via SPI

> **通过 SPI 读取 SD 卡**

The SD card command #17 is defined for reading a block. An SD card block is typically 512 bytes—when reading or writing an SD card, the OS reads or writes a 512-byte block. This is different from the terminal, which reads or writes in the granularity of a single byte.
> SD 卡命令 #17 被用于读取单个数据块（block）。一个 SD 卡数据块通常为 512 字节——在对 SD 卡进行读写时，操作系统以 512 字节的数据块为单位进行读写。这与终端不同，终端是以单个字节为粒度进行读写的。

Given `sdspi_exec_cmd`, the `sdspi_read` function below reads a block from the SD card to the memory address dst, and the offset argument decides which block should be read. For example, if offset is 0, `sdspi_read` will read the very first block on the SD card.
> 在 `sdspi_exec_cmd` 的基础上，下面的 `sdspi_read` 函数从 SD 卡读取一个数据块到内存地址 dst，其中 offset 参数决定读取哪一个数据块。例如，如果 offset 为 0，`sdspi_read` 将读取 SD 卡上的第一个数据块。

```c
static void sdspi_read(uint offset, char* dst) {
    /* Wait until the SD card is ready for a new command. */
    while (spi_exchange(0xFF) != 0xFF);

/* Send a read request with command #17. */
    char* arg = (void*)&offset;
    char reply, cmd17[] = {17 | (1 << 6), arg[3], arg[2], arg[1], arg[0], 0xFF};
    if (reply = sdspi_exec_cmd(cmd17))
        FATAL("cmd17 returns status 0x%.2x", reply);

/* Wait for the data packet and ignore the 2-byte checksum. */
    while (spi_exchange(0xFF) != 0xFE);
    for (uint i = 0; i < BLOCK_SIZE; i++) dst[i] = spi_exchange(0xFF);
    spi_exchange(0xFF);
    spi_exchange(0xFF);
}
```

At a high level, the code above proceeds as follows.
> 从宏观上看，上述代码的执行步骤如下：

* Wait for the SD card device to be ready for the next command.
> * 等待 SD 卡设备就绪以接收下一个命令。

* Send command #17 to the SD card. Out of the 6 bytes, the 4 bytes in the middle encode a block number (i.e., offset) indicating which block should be read.
> * 向 SD 卡发送命令 #17。这 6 个字节中，中间的 4 个字节对块号（即 offset）进行编码，指示应该读取哪一个数据块。

* Wait for the SD card device to be ready to send back the 512-byte block data.
> * 等待 SD 卡设备准备好发送回 512 字节的数据块。

* Receive 512 bytes of block data from the SD card, along with a 2-byte checksum.
> * 从 SD 卡接收 512 字节的数据块以及 2 字节的校验和。

We have shown a concrete example of controlling an SD card device using SPI, along with a good amount of hardware details about SPI. Next, we focus solely on the memory-mapped I/O interface because the PCIe hardware pins and signals are complex.
> 我们展示了一个使用 SPI 控制 SD 卡设备的具体示例，以及大量关于 SPI 的硬件细节。接下来，我们只关注内存映射 I/O 接口，因为 PCIe 的硬件引脚和信号非常复杂。

### Plug-and-play via PCIe

> **通过 PCIe 实现即插即用**

While SPI provides a simple way of using an SD card, there are two constraints.
> 虽然 SPI 提供了一种使用 SD 卡的简单方法，但存在两个限制：

* SPI devices typically need manual setup and cannot be automatically detected.
> * SPI 设备通常需要手动设置，无法被自动检测到。

* Reading the 512-byte block, byte-by-byte, through `spi_exchange` is very slow.
> * 通过 `spi_exchange` 逐字节读取 512 字节的数据块速度非常慢。

To address such problems, we introduce Plug-and-Play and Direct Memory Access (DMA). Plug-and-Play allows the OS to detect a new device connected to the CPU and dynamically configure memory-mapped I/O regions for it. Direct Memory Access allows the devices to read and write memory directly, so an SD card can write block data to memory without the CPU being involved (e.g., by running the loop in `spi_exchange`).
> 为了解决这些问题，我们引入了即插即用（Plug-and-Play）与直接内存访问（Direct Memory Access, DMA）。即插即用允许操作系统检测连接到 CPU 的新设备并为其动态配置内存映射 I/O 区域。直接内存访问允许设备直接读写内存，从而使 SD 卡可以在没有 CPU 参与（例如运行 `spi_exchange` 中的循环）的情况下将块数据写入内存。

Plug-and-Play and DMA are enabled by the Peripheral Component Interconnect (PCI) or its successor, PCIe. In egos-2000, the memory region starting at `0x30000000` controls PCIe, which connects to multiple devices. The OS controls device #i on PCIe by reading or writing the memory region `[0x30000000+0x8000*i, 0x30000000+0x8000*(i+1)`). For example, the addr=`0x1` in the definition of `QEMU_SD_CARD` in the `Makefile` indicates that an SD card is inserted as device #1 on the PCIe bus. Therefore, the memory at `0x30008000` is used to control this SD card (see `SDHCI_PCI_ECAM` in library/`egos.h`). Here is a table from a PCI wiki page that shows how to interpret the memory region at `0x30008000`.
> 即插即用和 DMA 是由外设部件互连（Peripheral Component Interconnect, PCI）或其继承者 PCIe 支持的。在 egos-2000 中，起始于 `0x30000000` 的内存区域用于控制 PCIe，PCIe 连接了多个设备。操作系统通过读写内存区域 `[0x30000000+0x8000*i, 0x30000000+0x8000*(i+1)`) 来控制 PCIe 上的第 i 号设备（device #i）。例如，`Makefile` 中 `QEMU_SD_CARD` 定义中的 addr=`0x1` 表明 SD 卡作为 PCIe 总线上的 1 号设备接入。因此，`0x30008000` 处的内存被用于控制该 SD 卡（参见 library/`egos.h` 中的 `SDHCI_PCI_ECAM`）。以下是来自 PCI 维基百科页面的一张表格，展示了如何解读 `0x30008000` 处的内存区域。

The first 4 bytes at `0x30008000` provide the Vendor ID and Device ID for device #1 on PCIe. Based on the two ID numbers, an operating system learns that device #1 is an SD card. This is essentially how Plug-and-Play works: the operating system periodically reads the 4 bytes at `0x30000000` (device #0), `0x30008000` (device #1), `0x30010000` (device #2), and so on. Using the two ID numbers, the operating system determines whether a device is plugged in or unplugged at a PCIe address. Later in this project, you will use the `QEMU_GRAPHIC` macro in the `Makefile` to plug in a VGA device as device #2 on the PCIe bus.
> `0x30008000` 处的头 4 个字节提供了 PCIe 上 1 号设备的厂商标识符（Vendor ID）和设备标识符（Device ID）。基于这两个 ID 号，操作系统便得知 1 号设备是一张 SD 卡。这本质上就是即插即用的工作方式：操作系统通过读取 `0x30000000`（0 号设备）、`0x30008000`（1 号设备）、`0x30010000`（2 号设备）等地址处的 4 个字节，利用这两个 ID 号就能判断某个 PCIe 地址处是否接入或拔出了设备。在本实验项目的后半部分，你将使用 `Makefile` 中的 `QEMU_GRAPHIC` 宏将 VGA 设备作为 2 号设备接入到 PCIe 总线上。

> [!TIP]
> As an exercise, read the first two lines of the `sdhci_init` function in `dev_disk.c`. They modify Command (`0x4`) and Base address #0 (`0x10`) according to the table above. Understand these two lines of code, especially the Memory Space and Bus Master bits, by reading the PCI wiki page.
> 作为练习，请阅读 `dev_disk.c` 中 `sdhci_init` 函数的前两行。它们根据上表修改了 Command (`0x4`) 和 Base address #0 (`0x10`)。通过阅读 PCI 维基百科页面来理解这两行代码，特别是 Memory Space（内存空间）和 Bus Master（总线主控）位。

## SD host controller interface

> **SD 主机控制器接口**

The PCIe bus provides an advanced memory-mapped I/O interface for SD cards called the Secure Digital Host Controller Interface (SDHCI). An example of using SDHCI in egos-2000 is the `sdhci_read` function in `dev_disk.c`, which reads a block via SD card command #17, just like `sdspi_read`, but uses direct memory access.
> PCIe 总线为 SD 卡提供了一个高级的内存映射 I/O 接口，称为安全数字主机控制器接口（Secure Digital Host Controller Interface, SDHCI）。在 egos-2000 中使用 SDHCI 的一个例子是 `dev_disk.c` 中的 `sdhci_read` 函数，它像 `sdspi_read` 一样通过 SD 卡命令 #17 读取一个块，但它使用的是直接内存访问（DMA）。

### SDHCI specification

> **SDHCI 规范**

The SD Association maintains the official SDHCI specification, just as RISC-V International maintains the ratified RISC-V ISA specifications. In this project, you will need Chapters 2.1 and 2.2 of the SD Host Controller Simplified Specification. Table 2-1 in Chapter 2.1.1 shows the SDHCI register map, and the screenshot below shows part of this table, which is just enough to understand the `sdhci_read` function.
> SD 协会负责维护官方 SDHCI 规范，正如 RISC-V 国际组织负责维护已批准的 RISC-V ISA 规范一样。在本实验项目中，你需要阅读《SD Host Controller Simplified Specification》的第 2.1 和 2.2 章。第 2.1.1 章中的表 2-1 展示了 SDHCI 寄存器映射，下图展示了该表的一部分，这刚好足够用来理解 `sdhci_read` 函数。

In this screenshot, 002h means `0x002`—it is just a different notation for hexadecimal.
> 在这张截图中，002h 表示 `0x002`——这只是十六进制的另一种表示法。

### Direct memory access

> **直接内存访问**

The first few lines of `sdhci_read` prepare a direct memory access.
> `sdhci_read` 的前几行代码为直接内存访问做准备。

```c
/* Prepare DMA (SDMA mode of SDHCI). */
static __attribute__((aligned(BLOCK_SIZE))) char aligned_buf[BLOCK_SIZE];
REGW(SDHCI_BASE, 0x0) = (uint)aligned_buf;
REGW(SDHCI_BASE, 0x4) = (1 << 16) | BLOCK_SIZE;
```

According to the table above, the address of a 512-byte buffer `aligned_buf` is written to the SDMA System Address register at 000h. The Block Size register at 004h is written by `BLOCK_SIZE`, and the 16-bit Block Count register at 006h is written by 1. Together, these values indicate that `sdhci_read` asks SDHCI to write one `BLOCK_SIZE`-byte block to the `aligned_buf` buffer when executing the upcoming SD card read command.
> 根据上表，512 字节缓冲区 `aligned_buf` 的地址被写入到 000h 处的 SDMA 系统地址寄存器。004h 处的块大小寄存器写入 `BLOCK_SIZE`，而 006h 处的 16 位块计数寄存器写入 1。这些设置表明，在执行随后的 SD 卡读取命令时，`sdhci_read` 要求 SDHCI 将一个大小为 `BLOCK_SIZE` 字节的数据块写入到 `aligned_buf` 缓冲区中。

> [!TIP]
> Read the rest of `sdhci_read` and the `sdhci_exec_cmd` function by referencing the table above. The code shows how to issue SD card command #17 by writing to the Argument register at 008h, the Transfer Mode register at 00Ch, and the Command register at 00Eh. It further shows how to wait for the SD card command #17 to finish, after which the 512-byte block of data should be ready in `aligned_buf`, given how we have prepared the DMA.
> 结合上表阅读 `sdhci_read` 的其余部分以及 `sdhci_exec_cmd` 函数。代码展示了如何通过写入 008h 处的参数寄存器、00Ch 处的传输模式寄存器和 00Eh 处的命令寄存器来发出 SD 卡命令 #17。它还进一步展示了如何等待 SD 卡命令 #17 完成；而在完成之后，按照我们此前对 DMA 的设置，512 字节的数据块就已经在 `aligned_buf` 缓冲区中准备就绪了。

Chapter 2.2 of the specification details all the registers in Table 2-1. You are encouraged to study the difference between Single-Operation DMA (SDMA) and Advanced DMA (ADMA) by reading more of the specification or searching online. For example, you will learn that a constraint of SDMA is that the DMA buffer cannot span across multiple memory pages. By making the DMA buffer aligned to `BLOCK_SIZE`, the code guarantees that the 512 bytes of `aligned_buf` are on the same 4KB memory page. With this constraint in mind, we now ask you to implement a better version of the SD card driver.
> 规范的第 2.2 章详细介绍了表 2-1 中的所有寄存器。鼓励你通过阅读更多规范或在网上搜索来学习单次操作 DMA（Single-Operation DMA, SDMA）与高级 DMA（Advanced DMA, ADMA）之间的区别。例如，你将了解到 SDMA 的一个限制是 DMA 缓冲区不能跨越物理页边界（即跨越多个内存页）。通过将 DMA 缓冲区按 `BLOCK_SIZE` 对齐，代码保证了 `aligned_buf` 的 512 个字节都位于同一个 4KB 内存页内。考虑到这一限制，现在我们要求你实现一个改进版的 SD 卡驱动程序。

### Multi-block read and write

> **多块读写**

Our driver code uses the SD card command #17 to read a single block, and the `disk_read` function simply calls `sdhci_read` or `sdspi_read` repeatedly in a loop.
> 我们的驱动程序代码使用 SD 卡命令 #17 来读取单个块，而 `disk_read` 函数只是在循环中重复调用 `sdhci_read` 或 `sdspi_read`。

The SD card standard provides commands #18 and #25 for reading and writing consecutive blocks altogether. Your job is to replace the loop in `disk_read` and complete `disk_write`, using SD commands #18 and #25. This blog describes these commands, and you can find other materials or ask AI/LLMs.
> SD 卡标准提供了命令 #18 和 #25，用于连续读写多个数据块。你的任务是使用 SD 命令 #18 和 #25 来替换 `disk_read` 中的循环，并实现 `disk_write`。这篇博客文章描述了这些命令，你也可以查阅其他资料或咨询 AI/大语言模型。

After modifying `disk_read` and `disk_write`, egos-2000 should still boot up normally. It is very useful and important to test your driver code. As explained in earth/`dev_disk.c`, you need to add a testing function, `disk_test`, and call it at the end of `disk_init`. This test code should write to and read from the SD card several times and verify that the behavior is as expected. Remember to revert any changes to the disk before `disk_test` returns.
> 在修改 `disk_read` 和 `disk_write` 之后，egos-2000 应该仍能正常启动。对驱动程序代码进行测试是非常重要且有益的。正如 earth/`dev_disk.c` 中所解释的，你需要添加一个测试函数 `disk_test`，并在 `disk_init` 的末尾调用它。该测试代码应该多次读写 SD 卡，并验证其行为是否符合预期。请记得在 `disk_test` 返回前恢复对磁盘数据所做的修改。

If you have a RISC-V board, you could further implement a new driver for the SPI bus using commands #18 and #25. It is similar to the driver code on QEMU, and the main difference is that you will use SPI to transfer block data instead of DMA.
> 如果你有 RISC-V 开发板，还可以进一步使用命令 #18 和 #25 为 SPI 总线实现一个新驱动程序。它与 QEMU 上的驱动代码类似，主要区别在于你需要通过 SPI 传输数据块而非使用 DMA。

## VGA via PCIe's plug-and-play

> **通过 PCIe 的即插即用实现 VGA**

To learn more about device drivers, you will now plug in a VGA device on the PCIe bus and write the driver code to initialize it. The goal is to ensure that apps/user/`video_demo.c` can run on QEMU, as shown in this screenshot.
> 为了进一步了解设备驱动程序，你现在将在 PCIe 总线上插入一个 VGA 设备，并编写驱动代码来对其进行初始化。目标是确保 apps/user/`video_demo.c` 可以在 QEMU 上运行，如这张截图所示。

To start with, update `QEMU_GRAPHIC` in the `Makefile` to -device VGA,addr=`0x2` -serial mon:stdio, which plugs in a VGA device as device #2 on the PCIe bus. The rest of your code should be in the `boot()` function in earth/`boot.c`. In QEMU's documentation for this VGA device, you can find the "PCI spec" as follows.
> 首先，将 `Makefile` 中的 `QEMU_GRAPHIC` 更新为 -device VGA,addr=`0x2` -serial mon:stdio，这会将 VGA 设备作为 2 号设备插入到 PCIe 总线上。其余代码应位于 earth/`boot.c` 中的 `boot()` 函数中。在 QEMU 关于该 VGA 设备的文档中，你可以找到如下的“PCI 规范”：

* PCI Region 0: Framebuffer memory, 16 MB in size (by default).
> * PCI 区域 0：帧缓冲区内存，大小为 16 MB（默认值）。

* PCI Region 2: MMIO bar, 4096 bytes in size (QEMU 1.3+)
> * PCI 区域 2：MMIO bar，大小为 4096 字节（QEMU 1.3+）。

This means you need to initialize both the base address #0 and base address #2 in device #2's PCI configuration. The first address is defined as `VIDEO_FRAME_BASE` in library/`egos.h`, and it is used by `video_demo` to set the RGB values of each pixel.
> 这意味着你需要初始化 2 号设备的 PCI 配置空间中的基地址 #0（Base Address #0）和基地址 #2（Base Address #2）。第一个基地址在 library/`egos.h` 中被定义为 `VIDEO_FRAME_BASE`，`video_demo` 会用它来设置每个像素的 RGB 值。

Your job is to pick an address for base address #2 and initialize the memory-mapped I/O region according to the "MMIO area spec" section in QEMU's documentation. The "VGA IOports" and "bochs dispi interface registers" in this section are not really well explained, so search for more information yourself. The comments in the `boot()` function provide some hints and guidance.
> 你的任务是为基地址 #2 选择一个内存地址，并根据 QEMU 文档中的“MMIO area spec”小节初始化该内存映射 I/O 区域。该文档节中的“VGA IOports”和“bochs dispi interface registers”说明较为简略，因此请自行查阅资料以获取更多信息。`boot()` 函数中的注释也提供了一些提示与引导。

Your driver code only runs on QEMU because the VGA/HDMI device on the RISC-V boards does not require any driver code to run `video_demo`. If you have a board such as the $35 Tang Nano 20K and are more interested in designing a Graphical User Interface (GUI) for egos-2000, you can work on your GUI directly on the board without worrying about the VGA driver at all. You simply need a monitor and a VGA/HDMI cable or adaptor.
> 你的驱动程序代码仅在 QEMU 上运行，因为 RISC-V 开发板上的 VGA/HDMI 设备不需要编写驱动代码即可运行 `video_demo`。如果你手头有类似于售价 35 美元的 Tang Nano 20K 开发板，并且更想为 egos-2000 设计图形用户界面（GUI），你可以直接在开发板上进行开发，完全无需关注 VGA 驱动程序。你只需要准备一台显示器以及一条 VGA/HDMI 线缆或转接头即可。

## Accomplishments

> **本章成果**

You have learned memory-mapped I/O and three types of I/O buses by reading the device driver code in egos-2000. The SDHCI code provides examples of plug-and-play and direct memory access. You have also written your own driver for an SD card and a VGA display.
> 通过阅读 egos-2000 中的设备驱动程序代码，你已经掌握了内存映射 I/O 以及三种不同类型的 I/O 总线。SDHCI 代码展示了即插即用和直接内存访问（DMA）的具体应用。你还亲手为 SD 卡和 VGA 显示器编写了设备驱动程序。
