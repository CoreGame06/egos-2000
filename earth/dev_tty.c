/*
串口字符输入输出
在 QEMU 中，终端输入的字符通过串口读入，程序输出的字符通过串口显示。
CPU 通过读写串口设备的特定地址，查询状态、接收字符、发送字符。
这里负责最底层的字符读写；Shell 的命令解析、终端服务的请求处理在其它文件。
*/

#include "egos.h"

// UART_BASE 是串口设备的起始地址（0x10000000），偏移 5 的寄存器保存收发状态。
// REGB(base, offset) 读取该地址的一个字节；这里访问的是设备，不是普通内存数据。
#define VIRT_LINE_STATUS   5UL

// 判断是否没有输入：状态的最低位 bit0 为 1，表示有字符可读。
// & (1 << 0) 取出这一位，再用 ! 取反，所以返回 1 表示空，0 表示有输入。
// 只检查一次，立即返回；调度器也用它判断是否有待处理的终端输入。
uint uart_rx_empty() {
    return !(REGB(UART_BASE, VIRT_LINE_STATUS) & (1 << 0));
}

// 读取一个字符；c 指向存放结果的位置，例如 char ch; uart_getc(&ch)。
void uart_getc(char* c) {
    // 一直检查设备状态，直到有输入；等待期间 CPU 不断执行这个循环，这叫轮询。
    while (uart_rx_empty());
    // 读取偏移 0 的接收寄存器；REGW 读取 32 位，& 0xFF 只保留低 8 位的字符。
    *c = REGW(UART_BASE, 0) & 0xFF;
}

// 多个核心共用同一个串口，输出时加锁，让它们依次使用发送寄存器。
static int tty_lock = 0;

// 输出一个字符，c 是字符本身，例如 uart_putc('A')。
void uart_putc(char c) {
    acquire(tty_lock);
    // 状态 bit5 为 1，表示发送寄存器空闲，可以写入下一个字符；否则继续等待。
    while (!(REGB(UART_BASE, VIRT_LINE_STATUS) & (1 << 5)));
    // 同样是偏移 0：读取时接收字符，写入时发送字符，具体行为由串口设备决定。
    REGW(UART_BASE, 0) = c;
    release(tty_lock);
    // 锁只保护这一个字符，多核同时打印时，整行文字仍可能相互穿插。
}

// 启动时由 boot() 调用，把上面的实现登记到 Earth 接口。
void tty_init() {
    // 这里只保存函数地址，不执行读写，也不启动终端服务进程。
    // 以后调用 earth->tty_read(&ch)，实际执行的就是 uart_getc(&ch)。
    earth->tty_read        = uart_getc;
    earth->tty_write       = uart_putc;
    earth->tty_input_empty = uart_rx_empty;
}
