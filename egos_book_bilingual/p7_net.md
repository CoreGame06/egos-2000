# Ethernet & TCP

> **IP / 以太网与 TCP/IP**

Modern operating systems enable users to connect to computer networks via Ethernet or Wi-Fi, and the most widely used network is the Internet. On the Internet, some computers run web servers that host web pages, while others run web browsers that retrieve and display them. Communication between a web server and web browsers typically relies on TCP/IP. In this project, you will gain hands-on experience with the Intel Gigabit Ethernet Controller and learn how an operating system such as egos-2000 transmits and receives network packets. With Ethernet working, you will learn how to communicate using UDP, a component of TCP/IP. Lastly, we outline some open-ended possibilities for implementing a web server that uses the full TCP/IP protocol.

现代操作系统使用户能够通过以太网（Ethernet）或 Wi-Fi 连接到计算机网络，而应用最广泛的网络就是互联网（Internet）。在互联网上，某些计算机运行 Web 服务器来托管网页，而另一些计算机运行 Web 浏览器来获取并展示这些网页。Web 服务器与 Web 浏览器之间的通信通常依赖于 TCP/IP 协议栈。在本项目中，你将亲自动手实践 Intel 千兆以太网控制器（Intel Gigabit Ethernet Controller），了解像 egos-2000 这样的操作系统是如何收发网络数据包的。在以太网正常工作后，你将学习如何使用 UDP（TCP/IP 协议族的重要组成部分）进行通信。最后，我们将介绍实现支持完整 TCP/IP 协议的 Web 服务器的一些开放性拓展方向。

## Creating a virtual network

> **创建虚拟网络**

This project uses a simple network topology of two machines connected to Ethernet. The Mac/Linux box refers to your work machine on which you run QEMU. The EGOS box refers to the virt RISC-V machine emulated in QEMU that runs egos-2000. Your first task is to create an Ethernet network connecting the two. Instead of using Ethernet cables and routers, you will create an emulated network with QEMU-based software tools (i.e., a virtual network). After you finish this project, you can port your code to the Arty A7 boards with an actual physical Ethernet controller and an Ethernet slot (i.e., a LAN port).

本项目采用由两台连接到以太网的机器构成的简单网络拓扑。Mac/Linux 宿主机指运行 QEMU 的开发工作站；EGOS 虚拟机指在 QEMU 中模拟运行 egos-2000 的 virt RISC-V 虚拟机。你的首要任务是建立连接这两台机器的以太网网络。你无需使用真实的网线和路由器，而是通过基于 QEMU 的软件工具来构建一个仿真网络（即虚拟网络）。完成本项目后，你还可以将代码移植到带有真实物理以太网控制器和以太网接口（即 LAN 口）的 Arty A7 开发板上。

Creating a virtual network involves different steps on macOS and Linux. We will explain the steps separately and cover the concepts of MAC and IP addresses.

在 macOS 和 Linux 上创建虚拟网络的步骤有所不同。我们将分别讲解这些步骤，并阐述 MAC 地址和 IP 地址的基本概念。

# MacOS

> **MacOS**

All you need to do is change one line in the `Makefile` while `QEMU_ETHERNET_MACOS` has been defined for you.

你只需修改 `Makefile` 中的一行代码即可，因为 `QEMU_ETHERNET_MACOS` 已提前为你定义完成。

```diff
- $(QEMU) $(QEMU_MACHINE) $(QEMU_GRAPHIC) $(QEMU_FLASH_1) $(QEMU_SD_CARD)
+ sudo $(QEMU) $(QEMU_MACHINE) $(QEMU_GRAPHIC) $(QEMU_FLASH_1) $(QEMU_SD_CARD) $(QEMU_ETHERNET_MACOS)
```

MacOS provides the ifconfig command, which you can use to inspect the virtual network controller created by QEMU. Specifically, after you run make qemu and enter your sudo password, run the following command in a separate terminal.

macOS 提供了 `ifconfig` 命令，你可以用它来检查 QEMU 创建的虚拟网络控制器。具体而言，在运行 `make qemu` 并输入 sudo 密码后，在另一个终端窗口中执行以下命令：

```console
> ifconfig -a
...
bridge100: flags=8a63<UP,BROADCAST,SMART,RUNNING,ALLMULTI,SIMPLEX,MULTICAST> mtu 1500
  options=3<RXCSUM,TXCSUM>
  ether ae:c9:06:72:b2:64
  inet 192.168.18.1 netmask 0xffffff00 broadcast 192.168.18.255
  inet6 fe80::acc9:6ff:fe72:b264%bridge100 prefixlen 64 scopeid 0x16
  inet6 fd64:5a7b:bd1e:741a:97:e7a7:bede:7f67 prefixlen 64 autoconf secured
  Configuration:
    id 0:0:0:0:0:0 priority 0 hellotime 0 fwddelay 0
    maxage 0 holdcnt 0 proto stp maxaddr 100 timeout 1200
    root id 0:0:0:0:0:0 priority 0 ifcost 0 port 0
    ipfilter disabled flags 0x0
  member: vmenet0 flags=3<LEARNING,DISCOVER>
          ifmaxaddr 0 port 21 priority 0 path cost 0
  nd6 options=201<PERFORMNUD,DAD>
  media: autoselect
  status: active
```

This is the virtual network controller device created by QEMU on macOS. If you quit QEMU, this virtual device will disappear. The two lines highlighted show the MAC and IP addresses. The MAC address is 6 bytes long and is shown in hexadecimal as ae:c9:06:72:b2:64. The ae is the lowest byte in this MAC address, and the 64 is the highest. An IP address has 32 bits, represented as 4 numbers in the range [0, 255]: the 192 is the highest byte, and the 1 is the lowest.

这是 QEMU 在 macOS 上创建的虚拟网络控制器设备。退出 QEMU 后，该虚拟设备便会自动消失。高亮显示的两行分别输出了 MAC 地址和 IP 地址。MAC 地址长度为 6 字节，以十六进制形式表示为 ae:c9:06:72:b2:64。在此 MAC 地址中，ae 为最低字节，64 为最高字节。IP 地址长 32 位（4 字节），通常写作 [0, 255] 范围内的 4 个十进制数字：其中 192 为最高字节，1 为最低字节。

# Linux (Ubuntu)

> **Linux (Ubuntu)**

While QEMU creates a virtual network controller (i.e., bridge100) automatically on macOS, you need to create it manually on Linux. The steps below are taken from a student project targeting Ubuntu. You may need to search for more information yourself.

与 QEMU 在 macOS 上会自动创建虚拟网络控制器（即 bridge100）不同，在 Linux 环境下你需要手动对其进行创建。以下步骤摘自针对 Ubuntu 的学生项目实践，你可能需要根据实际情况自行查找补充信息。

* Install qemu-system, which gives you /usr/lib/qemu/qemu-bridge-helper. Note that it is used in the definition of `QEMU_ETHERNET_LINUX` in the `Makefile`. net-tools downloads the ifconfig command on Ubuntu.

* 安装 qemu-system 软件包，以获取 `/usr/lib/qemu/qemu-bridge-helper` 辅助工具。请注意，`Makefile` 中 `QEMU_ETHERNET_LINUX` 的定义便使用了该工具。此外，在 Ubuntu 上安装 net-tools 软件包可提供 `ifconfig` 命令。

```console
> sudo apt-get install qemu-system net-tools
```

* We will use qemu-bridge-helper to communicate between egos-2000 and Ubuntu via the virbr0 bridge. The commands below create virbr0 for the bridge helper.

* 我们将使用 `qemu-bridge-helper` 辅助工具，通过 virbr0 网桥在 egos-2000 与 Ubuntu 之间建立通信。以下命令用于为桥接辅助工具配置并创建 virbr0 网桥。

```console
> sudo chmod u+s /usr/lib/qemu/qemu-bridge-helper
> mkdir /etc/qemu
> sudo chmod 755 /etc/qemu
> touch /etc/qemu/bridge.conf
> sudo chmod 644 /etc/qemu/bridge.conf
> echo "allow virbr0" | sudo tee -a /etc/qemu/bridge.conf
> sudo ip link add virbr0 type bridge
> sudo ip link set dev virbr0 up
> sudo ip addr add 192.168.18.1/24 dev virbr0
```

* Inspect the virtual Ethernet controller virbr0 with ifconfig.

* 使用 `ifconfig` 检查虚拟以太网控制器 virbr0 的配置状态。

```console
> ifconfig -a
...
virbr0: flags=4099<UP,BROADCAST,MULTICAST>  mtu 1500
        inet 192.168.18.1  netmask 255.255.255.0  broadcast 0.0.0.0
        ether 52:54:00:9d:14:bd  txqueuelen 1000  (以太网)
        RX packets 9  bytes 468 (468.0 B)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 12  bytes 622 (622.0 B)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
```

* Update the `Makefile`.

* 更新 `Makefile`。

```diff
- $(QEMU) $(QEMU_MACHINE) $(QEMU_GRAPHIC) $(QEMU_FLASH_1) $(QEMU_SD_CARD)
+ sudo $(QEMU) $(QEMU_MACHINE) $(QEMU_GRAPHIC) $(QEMU_FLASH_1) $(QEMU_SD_CARD) $(QEMU_ETHERNET_LINUX)
```

The br=virbr0 in `QEMU_ETHERNET_LINUX` means QEMU will use this manually created virtual Ethernet controller, virbr0.

`QEMU_ETHERNET_LINUX` 中的 `br=virbr0` 表示 QEMU 将使用这一手动创建的虚拟以太网控制器 virbr0。

## Mapping IP to MAC address

> **将 IP 地址映射到 MAC 地址**

Take a look at the struct `ethernet_header` in apps/user/`udp_demo.c`.

查看 `apps/user/udp_demo.c` 中定义的 `struct ethernet_header`：

```c
struct ethernet_header {
    uchar destmac[6];
    uchar srcmac[6];
    ushort ethertype;
} __attribute__((packed));
```

Suppose egos-2000 with MAC address 52:54:00:12:34:56 sends a 64-byte message to your macOS with MAC address ae:c9:06:72:b2:64. Here is what happens.

假设 MAC 地址为 52:54:00:12:34:56 的 egos-2000 向 MAC 地址为 ae:c9:06:72:b2:64 的 macOS 发送一条 64 字节的消息。具体交互过程如下：

* egos-2000 puts the 14-byte struct `ethernet_header` before the 64-byte message, and sets the destmac and srcmac as ae:c9:06:72:b2:64 and 52:54:00:12:34:56.

* egos-2000 在 64 字节消息的前面拼接 14 字节的 `struct ethernet_header` 以太网帧头，并将 `destmac`（目标 MAC）与 `srcmac`（源 MAC）分别设为 ae:c9:06:72:b2:64 和 52:54:00:12:34:56。

* The 14+64=78 bytes are passed to the Ethernet controller (i.e., the E1000 network controller emulated by QEMU), which then broadcasts them over Ethernet.

* 包含了 14 + 64 = 78 字节的数据被递交给以太网控制器（即 QEMU 模拟的 E1000 网卡），随后由其通过以太网广播出去。

* The Ethernet controller on macOS named bridge100 receives 78-byte data and checks that the destination MAC address (destmac) matches ae:c9:06:72:b2:64. The macOS Ethernet controller thus knows that it is the destination for this 64-byte message.

* macOS 上名为 bridge100 的虚拟以太网控制器接收到这 78 字节的数据帧，并检查目标 MAC 地址（`destmac`）是否与 ae:c9:06:72:b2:64 匹配。由此，macOS 以太网控制器确认自己即为该 64 字节消息的目标接收方。

If all we need is to communicate in the local Ethernet network, the steps above are enough. However, the Internet uses IP addresses to identify senders and receivers, where IP stands for the Internet Protocol. Many network tools rely on IP addresses, so macOS or Linux has to know how to map a machine's IP address to its MAC address.

如果只需要在局域以太网内部进行通信，上述步骤就已经足够。然而，互联网使用 IP 地址来标识发送方和接收方，其中 IP 代表网际协议（Internet Protocol）。由于许多网络工具依赖 IP 地址进行通信，因此 macOS 或 Linux 必须掌握如何将目标机器的 IP 地址映射到其 MAC 地址。

```console
# On macOS
> sudo arp -d -i bridge100 -a

# On Linux

> sudo arp -d -i virbr0 192.168.18.1
> > sudo arp -d -i virbr0 192.168.18.2

# On both, and use virbr0 instead of bridge100 on Linux

> sudo arp -s 192.168.18.1 ae:c9:06:72:b2:64 -i bridge100
> sudo arp -s 192.168.18.2 52:54:00:12:34:56 -i bridge100
> sudo arp -a -i bridge100
? (192.168.18.1) at ae:c9:6:72:b2:64 on bridge100 ifscope permanent [bridge]
? (192.168.18.2) at 52:54:0:12:34:56 on bridge100 permanent [bridge]
```

The arp command is built in for this purpose. The commands above tell your macOS or Linux system that, for messages sent to the IP address 192.168.18.2, 52:54:0:12:34:56 should be the destination MAC address in the Ethernet header. Similarly, the machine at IP address 192.168.18.1 has MAC address ae:c9:6:72:b2:64.

系统内置的 `arp` 命令即用于管理该映射关系。上述命令告知你的 macOS 或 Linux 系统：对于发送至 IP 地址 192.168.18.2 的数据包，以太网帧首部中的目标 MAC 地址应当设置为 52:54:0:12:34:56。同理，IP 地址 192.168.18.1 对应的机器 MAC 地址为 ae:c9:6:72:b2:64。

TIP

Make sure to carefully examine the output of sudo arp -a -i bridge100. If the mappings from IP to MAC addresses are incorrect or incomplete, network communication will likely fail.

提示

请务必仔细核查 `sudo arp -a -i bridge100` 命令的输出。若 IP 地址到 MAC 地址的映射关系不正确或缺失，网络通信将无法正常建立。

## Running the UDP demo app

> **运行 UDP 示例应用程序**

Now is a good time to read the apps/user/`udp_demo.c` file. For example, you can find that struct `ip_header` holds `src_ip` and `dst_ip`, which are assigned by 192.168.18.2 and 192.168.18.1 in the main function. This demo app uses User Datagram Protocol (UDP) to send the string Hello from egos-2000!\n\r from egos-2000 (i.e., 192.168.18.2) to Linux or macOS (i.e., 192.168.18.1).

现在是阅读 `apps/user/udp_demo.c` 源码的最佳时机。你可以看到，`struct ip_header` 中包含了 `src_ip` 与 `dst_ip` 字段，它们在 `main` 函数中分别被赋值为 192.168.18.2 和 192.168.18.1。该示例程序使用用户数据报协议（UDP），将字符串 `Hello from egos-2000!\n\r` 从 egos-2000（IP: 192.168.18.2）发送到宿主 Linux 或 macOS（IP: 192.168.18.1）。

After creating a virtual network, your second task in this project is to modify the source and destination addresses at the start of this demo app, so it works on your machine. Again, you can use the ifconfig command to inspect the destination IP and MAC addresses. Press Ctrl+a c in QEMU to enter the QEMU shell, then type info network. The MAC address of the E1000 network controller is shown below.

建立虚拟网络后，本项目的第二个任务是修改该示例程序开头的源地址和目标地址配置，使其适应你的实际开发环境。同样，你可以使用 `ifconfig` 命令查看目标的 IP 与 MAC 地址。在 QEMU 中按下 `Ctrl+a c` 组合键进入 QEMU monitor，然后输入 `info network`，即可查看 E1000 网卡的 MAC 地址：

```console
> make qemu
...
[CRITICAL] Welcome to the egos-2000 shell!
➜ /home/yunhao QEMU 8.2.6 monitor - type 'help' for more information
(qemu) info network
e1000.0: index=0,type=nic,model=e1000,macaddr=52:54:00:12:34:56
 \ E1000: index=0,type=vmnet-host,
(qemu)
```

After setting the addresses correctly in `udp_demo.c`, open two terminals and use the nc network tool to receive the UDP message on your macOS or Linux.

在 `udp_demo.c` 中正确配置好相关地址后，打开两个终端窗口，并在宿主 macOS 或 Linux 上使用 `nc` 网络工具来监听并接收 UDP 消息。

```console
# In terminal #1
> make qemu
...
[CRITICAL] Welcome to the egos-2000 shell!
➜ /home/yunhao udp_demo

# In terminal #2

> nc -u -l 192.168.18.1 8002
Hello from egos-2000!
```

In the nc command, -u means using UDP, and -l means listening. You can also use the tcpdump command. For example, right before running `udp_demo` in egos-2000, run sudo tcpdump -e -i bridge100 in another terminal. You should see the printing below with the addresses and the UDP, length 24.

在 `nc` 命令选项中，`-u` 表示使用 UDP 协议，`-l` 表示启动监听（listening）。你也可以借助 `tcpdump` 抓包工具进行验证。例如，在 egos-2000 中运行 `udp_demo` 之前，先在另一个终端中执行 `sudo tcpdump -e -i bridge100`。你应该能观察到如下包含相关地址以及 `UDP, length 24` 的抓包输出信息：

```console
> sudo tcpdump -e -i bridge100
14:57:27.269551 52:54:00:12:34:56 (oui Unknown) > ae:c9:06:72:b2:64 (oui Unknown), ethertype IPv4 (0x0800), length 66: 192.168.18.2.vcom-tunnel > 192.168.18.1.teradataordbms: UDP, length 24
```

TIP

The bridge100 in the above commands refers to the virtual network controller interface shown in the output of ifconfig -a on macOS. Replace it with virbr0 when running on Linux.

提示

上述命令中的 bridge100 对应 macOS 上 `ifconfig -a` 输出的虚拟网卡接口名称。在 Linux 环境下测试时，请将其替换为 virbr0。

## Receiving data from Ethernet

> **从以太网接收数据**

After learning how to send a UDP message from egos-2000 through Ethernet, you will write the driver code for receiving messages. Specifically, when nc sends a UDP message from Linux or macOS to egos-2000, the Intel Gigabit Ethernet Controller will send an I/O interrupt to the Platform-Level Interrupt Controller (PLIC) component of the RISC-V CPU emulated in QEMU. Your driver code needs to enable interrupts from the Ethernet controller and handle them in the `intr_entry` function in grass/`kernel.c`, where timer interrupts are currently handled. When handling an I/O interrupt from Ethernet, you will print the metadata and data of the received Ethernet frame, in particular, the UDP message.

在掌握了如何通过以太网从 egos-2000 发送 UDP 消息之后，接下来你需要编写接收消息的驱动程序代码。具体来说，当 `nc` 工具从宿主 Linux 或 macOS 向 egos-2000 发送 UDP 数据报时，Intel 千兆以太网控制器会向 QEMU 模拟的 RISC-V CPU 中的平台级中断控制器（PLIC）触发一个 I/O 中断（外部中断）。你的驱动代码需要使能以太网控制器的中断，并在 `grass/kernel.c` 中的 `intr_entry` 函数中对该中断进行处理（目前该函数已包含了时钟中断的处理逻辑）。在处理以太网 I/O 中断时，你需要打印出接收到的以太网帧的元数据及有效载荷，尤其是其中封装的 UDP 消息内容。

### Enabling PLIC interrupts

> **启用 PLIC 中断**

Enabling I/O interrupts for the Intel Gigabit Ethernet Controller involves two steps. The first step is simply to set the MEIE bit of the mie CSR, just like setting mie.MTIE enables timer interrupts according to chapter 3.1.9 of the RISC-V Reference Manual. Just like the handling of timer interrupts, it allows the control flow to enter `intr_entry` with id==11 in the kernel upon an I/O interrupt (aka. external interrupt). However, this does not tell the kernel which device sends the I/O interrupt, and we need to learn that through PLIC and the concept of Interrupt Request (IRQ). Consider an IRQ as a number in the range [0, 1023], and PLIC associates a unique IRQ with each device.

为 Intel 千兆以太网控制器使能 I/O 中断分为两个步骤。第一步是配置 CPU 侧的中断使能：只需将 `mie` CSR 寄存器的 `MEIE` 位置 1 即可，这与《RISC-V 参考手册》第 3.1.9 章中通过设置 `mie.MTIE` 启用时钟中断的原理一致。与时钟中断的处理流程相似，当发生 I/O 中断（即外部中断，external interrupt）时，控制流会跳转进入内核的 `intr_entry` 函数（此时中断标识号 `id == 11`）。然而，这仅仅说明发生了外部中断，并不能直接告知内核是哪一个外设触发了该中断；因此，我们需要借助 PLIC 以及中断请求号（IRQ）的概念来识别具体的设备。你可以将 IRQ 理解为 [0, 1023] 范围内的数字编号，PLIC 会为每个外设分配一个唯一的 IRQ 号。

The second step is to learn the IRQ number of the Ethernet controller and enable interrupts from this IRQ in PLIC. It is your job to figure out the details, but here are some hints.

第二步是查明以太网控制器的具体 IRQ 号，并在 PLIC 中使能该 IRQ 的中断响应。探究具体细节是本任务的要求，以下提供一些参考提示：

```c
#define PLIC_PRIORITY(irq)     (PLIC_BASE + 0x0000 + 4 * (irq))
#define PLIC_ENABLE_BASE(core) (PLIC_BASE + 0x2000 + 0x80 * (core))
#define PLIC_CAUSE(core)       (PLIC_BASE + 0x200004 + 0x1000 * (core))
```

The first macro sets the priority of an IRQ, and the second specifies the base address of a bitmap that controls whether an IRQ is enabled for a CPU core. Modify the `QEMU_MACHINE` in the `Makefile` and use -smp 1 instead of -smp 4 so that you only need to enable the interrupt for core #0. The third macro provides the device's IRQ number that triggered the interrupt, and you need it in `intr_entry`. In particular, make sure to read the IRQ number before handling an I/O interrupt from Ethernet and write it back after handling it. This allows PLIC to fire the next interrupt. Read chapter 10 of this CPU manual for more about PLIC.

第一个宏用于设置指定 IRQ 的优先级；第二个宏指定控制某 CPU 核心是否使能该 IRQ 的使能位图基地址。建议修改 `Makefile` 中的 `QEMU_MACHINE` 参数，使用 `-smp 1` 替代 `-smp 4`，这样你只需为 0 号 CPU 核心使能中断即可。第三个宏用于获取触发本次中断的设备 IRQ 号（即进行 Claim 响应），你需要将其用于 `intr_entry` 中。需要特别注意的是：在处理来自以太网的 I/O 中断之前，必须先读取该 IRQ 号（Claim 步骤）；在处理完成后，必须将该 IRQ 号重新写回（Complete 步骤，标识中断处理完成）。只有这样，PLIC 才能继续向 CPU 触发下一次中断。如需深入了解 PLIC 机制，请参阅该 CPU 手册的第 10 章。

TIP

Enabling and handling I/O interrupts for multiple CPU cores could be tricky. Focus on getting your Ethernet driver code working, and then you can work on supporting multicore in the next project.

提示

为多个 CPU 核心启用和处理 I/O 中断可能会比较复杂。建议先专注于让你的以太网驱动程序正常工作，在接下来的项目中再去支持多核。

### Initializing the E1000 NIC

> **初始化 E1000 网卡**

The key concepts in Intel's E1000 Ethernet controller are the receive descriptor and receive buffer. When E1000 receives an Ethernet frame, it writes the frame to a receive buffer and records its metadata (e.g., length) in the corresponding receive descriptor. On the operating system side, egos-2000 reads the descriptors and buffers, and sets the descriptors' status field to unused so they can receive future Ethernet frames.

Intel E1000 以太网控制器硬件的核心概念是接收描述符（receive descriptor）与接收缓冲区（receive buffer）。当 E1000 网卡接收到一个以太网帧时，硬件 DMA 会将该数据帧写入某个接收缓冲区，并在对应的接收描述符中填入元数据（如数据帧长度、状态标志等）。在操作系统层面，egos-2000 负责读取描述符与缓冲区中的数据，并将描述符的状态字段重新标记为未用/就绪状态，从而使其能够继续接收后续的以太网帧。

In addition to the receive descriptors and buffers, you need to initialize a few other things for E1000, such as the MAC address and interrupt mask. Below is a list of the E1000 control registers you need to know; you can find more details in Intel's E1000 manual.

除了配置接收描述符和接收缓冲区之外，你还需要为 E1000 网卡初始化其他若干关键寄存器，例如 MAC 地址以及中断掩码寄存器。下表列出了你需要用到的 E1000 控制寄存器；更详细的寄存器说明参见 Intel E1000 官方手册。

* `0xD0`: Interrupt Mask Set/Read; `0x100`: Receive Control
* `0x2808`: Receive Descriptor Length
* `0x2810`: Receive Descriptor Head; `0x2818`: Receive Descriptor Tail
* `0x2800`: Receive Descriptor Base Low; `0x2804`: Receive Descriptor Base High
* `0x5400`: Receive Address (MAC) Low; `0x5404`: Receive Address (MAC) High

* `0xD0`: 中断掩码设置/读取（Interrupt Mask Set/Read）；`0x100`: 接收控制（Receive Control）
* `0x2808`: 接收描述符长度（Receive Descriptor Length）
* `0x2810`: 接收描述符头（Receive Descriptor Head）；`0x2818`: 接收描述符尾（Receive Descriptor Tail）
* `0x2800`: 接收描述符基址低位（Receive Descriptor Base Low）；`0x2804`: 接收描述符基址高位（Receive Descriptor Base High）
* `0x5400`: 接收地址 (MAC) 低位（Receive Address (MAC) Low）；`0x5404`: 接收地址 (MAC) 高位（Receive Address (MAC) High）

Add your code to initialize the E1000 device in earth/`boot.c`, and put the common data structures, such as the receive descriptor, in library/`egos.h`. Define an array of receive descriptors and an array of receive buffers as global variables in earth/`boot.c`. Declare them in library/`egos.h`, so the kernel can access them in `intr_entry` when printing out the UDP messages within the Ethernet frames received.

请在 `earth/boot.c` 中添加代码以初始化 E1000 设备，并将通用数据结构（如接收描述符结构体）定义在 `library/egos.h` 中。在 `earth/boot.c` 中定义接收描述符数组和接收缓冲区数组作为全局变量，并在 `library/egos.h` 中使用 `extern` 对其进行声明，以便内核在收到以太网帧时能在 `intr_entry` 中访问这些描述符与缓冲区，并打印出接收到的 UDP 消息。

### Printing the data received

> **打印接收到的数据**

Your last task in this project is to print the UDP messages received from Ethernet. Inspect the descriptor right after the Receive Descriptor Tail (i.e., `0x2818`), and it should indicate that a frame has been received (i.e., bit #0 of the descriptor's status field is 1). Read the data from the corresponding receive buffer, and print out the buffer content at offset 42 (i.e., the size of the headers: 42==sizeof(struct `ethernet_header` eth)+sizeof(struct `ip_header` ip)+sizeof(struct `udp_header` udp) according to apps/user/`udp_demo.c`). Lastly, set the descriptor status field as unused, and advance the Receive Descriptor Tail.

你在本项目中的最后一项任务是打印从以太网接收到的 UDP 消息。检查紧跟在接收描述符尾寄存器（RDT，即 `0x2818`）当前所指索引之后的描述符，该描述符的状态应表明已成功接收到一个数据帧（即描述符 status 字段的 bit #0 为 1）。从对应的接收缓冲区中读取数据，并打印出缓冲区偏移量 42 处的字节内容（该偏移量等于各协议头长度之和：根据 `apps/user/udp_demo.c`，42 == sizeof(struct `ethernet_header` eth) + sizeof(struct `ip_header` ip) + sizeof(struct `udp_header` udp)）。最后，将该描述符的 status 字段清零（标记为未用），并递增/推进接收描述符尾指针（RDT）。

Again, do not forget to write the IRQ back to PLIC, so PLIC can fire the next I/O interrupt. Do not forget to check your ARP records again, which should map IP address 192.168.18.2 to MAC address ae:c9:6:72:b2:64. After you finish, you should be able to do the following.

再次提醒：切记将 IRQ 写回 PLIC（完成 Complete 响应），否则 PLIC 将无法触发后续的 I/O 中断。此外，也别忘了再次核验你的 ARP 表项映射，确保 IP 地址 192.168.18.2 准确映射至 MAC 地址 ae:c9:6:72:b2:64。全部完成后，你应该能够执行如下测试：

```console
# In terminal #1
# You may need to add sudo before this nc command.
> nc -u 192.168.18.2 8002
Hello egos-2000!
This is Mac/Linux!

# In terminal #2

> make qemu
...
[CRITICAL] Welcome to the egos-2000 shell!
➜ /home/yunhao
[SUCCESS] Get 59 bytes from rxdesc[0]: Hello egos-2000!

[SUCCESS] Get 61 bytes from rxdesc[1]: This is Mac/Linux!
```

The SUCCESS logs above show the Ethernet frame length, the receive descriptor index, and the UDP message at offset 42 of the receive buffer.

上述 SUCCESS 日志分别展示了收到的以太网帧长度、接收描述符索引，以及位于接收缓冲区偏移量 42 处的 UDP 消息文本。

## Write a web server with TCP

> **IP / 使用 TCP/IP 编写 Web 服务器**

After seeing how to send and receive Ethernet frames using UDP, you can start to research how to introduce more of TCP/IP. With TCP, you can write a simple web server listening on TCP port 80 and handling HTTP requests for HTML web pages. One way to do this is to integrate your Ethernet driver with the so-called uIP library, as explored by Professor Cheng Tan in his CS6640 at Northeastern University. This CS6640 student project is an open-source reference for this idea.

在掌握了如何利用 UDP 收发以太网帧之后，你可以开始进一步探索如何引入完整的 TCP/IP 协议栈。借助 TCP 协议，你可以编写一个监听在 TCP 80 端口上的简易 Web 服务器，用来处理获取 HTML 网页的 HTTP 请求。实现该方案的一种方式是将你的以太网驱动程序与轻量级协议栈 uIP 库进行集成，这正如美国东北大学（Northeastern University）谭程（Cheng Tan）教授在其 CS6640 课程中所探索的那样。这份 CS6640 学生项目为该设计构想提供了一个开源参考实现。

Another possibility is to run TCP over Wi-Fi, as demonstrated in the apps/user/`tcp_demo.c` file of egos-2000. It requires the Arty A7 board and the ESP32 Pmod extension. You can read the comments in apps/user/`tcp_demo.c` for more details. Note that the TCP/IP protocols are implemented in the ESP32 hardware, so you do not need a software driver such as the uIP library. However, this idea does not work on QEMU.

另一种可选思路是在 Wi-Fi 链路上运行 TCP，这在 egos-2000 的 `apps/user/tcp_demo.c` 文件中有具体示例。该方案需要借助 Arty A7 开发板和 ESP32 Pmod 扩展模块。你可以阅读 `apps/user/tcp_demo.c` 中的注释获取详细说明。需要注意的是，TCP/IP 协议栈已由 ESP32 硬件板载实现，因此无需像 uIP 那样的软件协议栈驱动。不过，该方案无法在 QEMU 模拟器中运行。

## Accomplishments

> **总结与成就**

You have gained experience with I/O interrupts, platform-level interrupt controller, and the IRQ numbers. You have also gained experience with the Intel Gigabit Ethernet Controller by reading Intel's manual and implementing the receive functionality. This concludes our introduction to device drivers in operating systems. We encourage you to research how to implement a TCP/IP web server on top of this project.

在本项目中，你积累了关于 I/O 中断、平台级中断控制器（PLIC）以及 IRQ 中断号配置的实战经验；同时通过研读 Intel 官方手册并亲自实现数据接收逻辑，深入掌握了 Intel 千兆以太网控制器的驱动开发。至此，操作系统课程中关于设备驱动程序的内容就告一段落了。我们鼓励你在本项目的成果之上，继续探索并实现一个支持完整 TCP/IP 协议栈的 Web 服务器！
