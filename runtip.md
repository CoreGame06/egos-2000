export PATH=$PATH:/home/shenmi/xpack-riscv-none-elf-gcc-14.2.0-3/bin:/home/shenmi/xpack-qemu-riscv-8.2.2-1/bin
cd /home/shenmi/egos-2000 && make qemu

# EGOS-2000 以太网/UDP 演示运行指南

由于 Linux 系统的虚拟网络配置在重启后可能会重置，为了保证下次能够一次性顺畅演示，请按照以下步骤执行：

如果你现在不需要演示网络部分，只需要把 Makefile 中启动 QEMU 的参数还原为普通模式即可。

 Makefile 第 74 行：

  + sudo $(QEMU) $(QEMU_MACHINE) $(QEMU_GRAPHIC) $(QEMU_FLASH_1) $(QEMU_SD_CARD) $(QEMU_ETHERNET_LINUX)
  - $(QEMU) $(QEMU_MACHINE) $(QEMU_GRAPHIC) $(QEMU_FLASH_1) $(QEMU_SD_CARD)

第61行
  - QEMU_MACHINE = -M virt -smp 4 -m 8M -bios tools/egos.bin
  + QEMU_MACHINE = -M virt -smp 4 -m 8M -bios tools/egos.bin


## 第一步：环境初始化与 ARP 绑定（宿主机终端）

每次电脑重启或者网卡异常后，**必须在宿主机终端**直接复制并运行以下整段脚本。它会自动唤醒虚拟网卡 `virbr0`，并强制绑定静态 ARP（防止 Linux 找不到虚拟机 MAC 地址而发送 `0x0806` 请求包）：

```bash
# 1. 确保 QEMU 桥接权限和配置
sudo chmod u+s /usr/lib/qemu/qemu-bridge-helper 2>/dev/null
sudo mkdir -p /etc/qemu
echo "allow virbr0" | sudo tee /etc/qemu/bridge.conf > /dev/null

# 2. 唤醒/创建网桥 virbr0
sudo ip link add virbr0 type bridge 2>/dev/null || true
sudo ip link set dev virbr0 up
sudo ip addr add 192.168.18.1/24 dev virbr0 2>/dev/null || true

# 3. 清除旧记录并强行绑定静态 ARP (极度重要!)
sudo arp -d 192.168.18.2 -i virbr0 2>/dev/null || true
sudo arp -s 192.168.18.2 52:54:00:12:34:56 -i virbr0
```

*(验证方法：运行 `arp -a -i virbr0`，只要看到 `192.168.18.2` 对应 `52:54:00:12:34:56` 并且有 `PERM` 标志即可。)*

---

## 第二步：启动操作系统（终端 1）

新开终端时记得先配置好 GCC 和 QEMU 的环境变量（即本文件开头的那两行命令），然后在 `egos-2000` 项目根目录下编译并启动系统：

```bash
make qemu
```
等待系统启动完毕，看到 `➜ /home/yunhao` 提示符即可。

---

## 第三步：发送 UDP 数据包（终端 2）

新开一个宿主机终端，运行 `nc` (Netcat) 命令连接到虚拟机的 8002 端口（UDP模式）：

```bash
nc -u 192.168.18.2 8002
```

进入等待输入状态后，直接敲入你想发送的任意字符（例如 `Hello,egos2000`），然后按下**回车键**。

---

## 预期结果
你将在【终端 1】（EGOS-2000 系统界面）看到极其干净精准的接收日志：
```text
[INFO] Got pkt: rxdesc[0], len=60, ethertype=0x800, proto=17
[SUCCESS] Get 60 bytes from rxdesc[0]: Hello,egos2000
```
*(注：如果消息长度很短，`len` 依然显示 60 字节，那是因为以太网底层硬性规定了数据帧最小必须通过 Padding 补齐到 60 字节。)*
