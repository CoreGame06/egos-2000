#include "app.h"

/* UDP 发包演示：填写报文头和消息 -> 计算校验和 -> 准备 DMA 发送 -> 通知网卡。
 * 直接操作 QEMU 的 e1000 网卡，本地/目标 MAC、IP、端口需按实际网络环境配置。
 * 当前使用固定地址，没有 ARP 地址解析或 socket 接口。 */
#define HELLO_MSG           "Hello from egos-2000!\n\r"
#define IPTOINT(a, b, c, d) ((a << 24) | (b << 16) | (c << 8) | d)

// 目标和本地的 MAC、IP、UDP 端口。
#define DEST_MAC  {0xe2, 0x8c, 0xf4, 0xbc, 0x59, 0x7b}
#define LOCAL_MAC {0x52, 0x54, 0x00, 0x12, 0x34, 0x56}
uint dest_ip = IPTOINT(192, 168, 18, 1), dest_udp_port = 8002;
uint local_ip = IPTOINT(192, 168, 18, 2), local_udp_port = 8001;

// 以太网头：目标 MAC、源 MAC、协议类型，共 14 字节。
// packed 去掉结构体中的填充字节，使内存布局与报文格式一致。
struct ethernet_header {
    uchar destmac[6];
    uchar srcmac[6];
    ushort ethertype;
} __attribute__((packed));

// IPv4 头共 20 字节，记录源/目标 IP 等信息；total_length 包含 IP 头和后面的全部内容。
struct ip_header {
    uchar version;
    uchar diff_services;
    ushort total_length;
    ushort identification;
    ushort fragment_offset;
    uchar ttl;
    uchar proto;
    ushort checksum;
    uint src_ip;
    uint dst_ip;
} __attribute__((packed));

// UDP 头共 8 字节，length 是 UDP 头加消息的长度。
struct udp_header {
    ushort src_port;
    ushort dst_port;
    ushort length;
    ushort checksum;
} __attribute__((packed));

// 完整报文：以太网头 + IP 头 + UDP 头 + 消息内容。
struct ethernet_frame {
    struct ethernet_header eth;
    struct ip_header ip;
    struct udp_header udp;
    char payload[sizeof(HELLO_MSG)];
} __attribute__((packed));

// UDP 校验和还需要源/目标 IP、协议号和 UDP 长度，临时拼成“伪首部”。
// 这部分只参与校验，不额外发送。
struct checksum_fields {
    uint src_ip;
    uint dst_ip;
    uchar zero;
    uchar proto;
    ushort length;
} __attribute__((packed));

// 从已有的和 r 开始，每两个字节作为一个数累加，超出 16 位的进位加回低位。
// complete=0 返回中间结果，=1 再取反得到校验和；当前未处理奇数长度末尾的单字节。
static ushort checksum(uint r, char* ptr, uint length, int complete) {
    for (int i = 0; i < length / 2; i++)
        r += ((uint)(ptr[2 * i]) << 8) | (uint)(ptr[2 * i + 1]);

    // 把高位进位加回低 16 位，直到没有进位。
    while (r >> 16) r = (r & 0xffff) + (r >> 16);
    if (complete && (r = (~r) & 0xffff) == 0) r = 0xffff;
    return r;
}

int main() {
    // 填写各层报文头。网络字段按高字节在前保存，用 bswap16/32 调整字节顺序。
    /* clang-format off */
    struct ethernet_frame eth_frame = {
        .eth = {
            .srcmac          = LOCAL_MAC,
            .destmac         = DEST_MAC,
            .ethertype       = __builtin_bswap16(0x0800) // IPv4。
        },
        .ip = {
            .version         = 0x45, // IPv4，头长 5×4=20 字节。
            .diff_services   = 0,
            .total_length    = __builtin_bswap16(sizeof(struct ip_header) + sizeof(struct udp_header) + sizeof(HELLO_MSG)),
            .identification  = __builtin_bswap16(0),
            .fragment_offset = __builtin_bswap16(0x4000), // 不分片。
            .ttl             = 64,
            .proto           = 0x11, // 协议号 17，表示 UDP。
            .src_ip          = __builtin_bswap32(local_ip),
            .dst_ip          = __builtin_bswap32(dest_ip)
        },
        .udp = {
            .src_port        = __builtin_bswap16(local_udp_port),
            .dst_port        = __builtin_bswap16(dest_udp_port),
            .length          = __builtin_bswap16(sizeof(struct udp_header) + sizeof(HELLO_MSG)),
        }
    };
    /* clang-format on */

    // 复制消息；sizeof 包含末尾 '\0'，本例消息总长度为偶数。
    memcpy(eth_frame.payload, HELLO_MSG, sizeof(HELLO_MSG));

    // IP 校验和只计算 IP 头。
    eth_frame.ip.checksum = __builtin_bswap16(
        checksum(0, (void*)&eth_frame.ip, sizeof(struct ip_header), 1));

    // UDP 校验和分两次累加：先算伪首部，再加上 UDP 头和消息，最后取反。
    struct checksum_fields check = {.src_ip = eth_frame.ip.src_ip,
                                    .dst_ip = eth_frame.ip.dst_ip,
                                    .zero   = 0,
                                    .proto  = eth_frame.ip.proto,
                                    .length = eth_frame.udp.length};
    uint r = checksum(0, (void*)&check, sizeof(struct checksum_fields), 0);
    eth_frame.udp.checksum = __builtin_bswap16(
        checksum(r, (void*)&eth_frame.udp,
                 sizeof(struct udp_header) + sizeof(HELLO_MSG), 1));

    // 允许访问网卡寄存器和使用 DMA，指定寄存器地址。
    REGW(ETH_PCI_ECAM, 0x4)  = 6;
    REGW(ETH_PCI_ECAM, 0x10) = ETH_CTL_BASE;

    // DMA 需要物理地址，不能直接使用任意用户指针。
    // 共享目录页采用恒等映射，虚拟地址与物理地址相同，因此借用其空余位置存放报文。
    // 这些位置是共享的，当前演示没有并发发送保护。
    char* txbuffer = (char*)(SHELL_WORK_DIR + 1024);
    memcpy(txbuffer, &eth_frame, sizeof(struct ethernet_frame));

    // 描述符告诉网卡去哪里取数据、发送多少字节、如何报告完成。
    // 一项占 16 字节：0~7 为物理地址，8~9 为长度，11 为命令，12 为状态。
    char* txdesc = (char*)(SHELL_WORK_DIR + 2048);
    memset(txdesc, 0, 16);
    REGW(txdesc, 0)  = (uint)txbuffer; // 报文所在的物理地址。

    // bit0 表示本包结束，bit3 要求网卡回写完成状态。
    REGB(txdesc, 11) = 9;

    REGB(txdesc, 8)  = sizeof(struct ethernet_frame); // 本例报文较短，只填写长度低字节。

    // 告诉网卡描述符的物理地址，并初始化发送环和发送控制。
    REGW(ETH_CTL_BASE, 0x3800) = (uint)txdesc;
    // 发送环长度按控制器要求设为 128 字节（8 项），这里仅提交第 0 项。
    REGW(ETH_CTL_BASE, 0x3808) = 128;
    REGW(ETH_CTL_BASE, 0x3810) = 0;        // 设备从第 0 项开始处理。
    REGW(ETH_CTL_BASE, 0x3818) = 0;        // 暂时没有提交待发送项。
    REGW(ETH_CTL_BASE, 0x410)  = 0x60100A; // 包间发送间隔。
    REGW(ETH_CTL_BASE, 0x400) |= 0x4010A;  // 开启发送并设置发送参数。

    // 尾位置从 0 改为 1，提交第 0 项，网卡开始通过 DMA 读取报文并发送。
    REGW(ETH_CTL_BASE, 0x3818) = 1;

    // 轮询状态 bit0，等网卡回写“已完成”后退出；DMA 搬数据，CPU 在这里等待。
    while (!(REGB(txdesc, 12) & 1));

    return 0;
}
