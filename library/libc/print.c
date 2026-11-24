/* 项目的 printf 和日志函数。先把参数拼成字符串，再交给终端接口输出。 */

#include "egos.h"
#include "servers.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

// 把无符号数 v 按 base 进制写到 p，返回下一个可写的位置。
static char* fmt_u64(char* p, ulonglong v, uint base) {
    char tmp[24]; int n = 0;
    // 取余先得到最低位。例如 123 按十进制依次取到 3、2、1，再反过来写。
    do tmp[n++] = "0123456789abcdef"[v % base]; while (v /= base);
    while (n) *p++ = tmp[--n];
    return p;
}

// 将 fmt 和参数列表 args 拼成字符串，写入 out。例如 "PID=%d" 和 5 得到 "PID=5"。
// 当前只处理下列格式，调用者负责提供足够大的缓冲区。
void format_to_str(char* out, const char* fmt, va_list args) {
    char* p = out;
    // 普通字符直接复制。遇到 %，按后面的格式用 va_arg 取出下一个参数。
    for (; *fmt; fmt++) {
        if (*fmt != '%') { *p++ = *fmt; continue; }
        fmt++;
        uint lcount = 0;
        while (*fmt == 'l') { lcount++; fmt++; }   // 两个 l（如 %lld）时取 64 位整数。
        switch (*fmt) {
        case 's': { for (const char* s = va_arg(args, char*); *s;) *p++ = *s++; break; }
        case 'c': *p++ = (char)va_arg(args, int); break; // char 传给变参函数时会提升为 int。
        case 'd': {
            long long v = (lcount >= 2) ? va_arg(args, long long)
                                        : (long long)va_arg(args, int);
            if (v < 0) { *p++ = '-'; v = -v; } // 先写负号，再转换数字。
            p = fmt_u64(p, (ulonglong)v, 10); break;
        }
        case 'u': p = fmt_u64(p, (lcount >= 2) ? va_arg(args, ulonglong)
                                               : (ulonglong)va_arg(args, uint), 10); break;
        case 'x': p = fmt_u64(p, (lcount >= 2) ? va_arg(args, ulonglong)
                                               : (ulonglong)va_arg(args, uint), 16); break;
        case 'p': *p++ = '0'; *p++ = 'x';
                  p = fmt_u64(p, (ulonglong)(uint)va_arg(args, void*), 16); break;
        }
    }
    *p = 0; // 整条字符串拼完后再补结束符。
}

// 在 512 字节缓冲区中依次拼接前缀、正文和后缀，再交给 term_write 输出。
// 串口直写和消息输出的区别在 servers.c 里处理。
#define LOG(prefix, suffix)                                                    \
    char buf[512];                                                             \
    strcpy(buf, prefix);                                                       \
    va_list args;                                                              \
    va_start(args, format);                                                    \
    format_to_str(buf + strlen(prefix), format, args);                         \
    va_end(args);                                                              \
    strcat(buf, suffix);                                                       \
    term_write(buf, strlen(buf));

// egos.h 将 printf 定义为 my_printf，普通打印的前缀和后缀都为空。
int my_printf(const char* format, ...) { LOG("", ""); }

// 日志会加上类型标签。\x1B[...m 设置终端颜色，0m 恢复默认颜色。
int INFO(const char* format, ...) { LOG("[INFO] ", "\n\r") }

int FATAL(const char* format, ...) {
    LOG("\x1B[1;31m[FATAL] ", "\x1B[1;0m\n\r") // 红色。
    while (1); // 报错后停在这里。
}

int SUCCESS(const char* format, ...) {
    LOG("\x1B[1;32m[SUCCESS] ", "\x1B[1;0m\n\r") // 绿色。
}

int CRITICAL(const char* format, ...) {
    LOG("\x1B[1;33m[CRITICAL] ", "\x1B[1;0m\n\r") // 黄色。
}
