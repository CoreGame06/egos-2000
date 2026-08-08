/*
 * (C) 2026, Cornell University
 * All rights reserved.
 *
 * Description: formatted printing
 * format_to_str() converts a format into a C string:
 * e.g., converts ("%s-%d", "egos", 2000) to "egos-2000".
 * term_write() prints the converted C string to the screen.
 */

#include "egos.h"
#include "servers.h"
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

static char* fmt_u64(char* p, ulonglong v, uint base) {
    char tmp[24]; int n = 0;
    do tmp[n++] = "0123456789abcdef"[v % base]; while (v /= base);
    while (n) *p++ = tmp[--n];
    return p;
}

void format_to_str(char* out, const char* fmt, va_list args) {
    char* p = out;
    for (; *fmt; fmt++) {
        if (*fmt != '%') { *p++ = *fmt; continue; }
        fmt++;
        uint lcount = 0;
        while (*fmt == 'l') { lcount++; fmt++; }   /* %ld 按 32 位，%lld 按 64 位 */
        switch (*fmt) {
        case 's': { for (const char* s = va_arg(args, char*); *s;) *p++ = *s++; break; }
        case 'c': *p++ = (char)va_arg(args, int); break;
        case 'd': {
            long long v = (lcount >= 2) ? va_arg(args, long long)
                                        : (long long)va_arg(args, int);
            if (v < 0) { *p++ = '-'; v = -v; }
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
    *p = 0;
}

#define LOG(prefix, suffix)                                                    \
    char buf[512];                                                             \
    strcpy(buf, prefix);                                                       \
    va_list args;                                                              \
    va_start(args, format);                                                    \
    format_to_str(buf + strlen(prefix), format, args);                         \
    va_end(args);                                                              \
    strcat(buf, suffix);                                                       \
    term_write(buf, strlen(buf));

int my_printf(const char* format, ...) { LOG("", ""); }

int INFO(const char* format, ...) { LOG("[INFO] ", "\n\r") }

int FATAL(const char* format, ...) {
    LOG("\x1B[1;31m[FATAL] ", "\x1B[1;0m\n\r") /* \x1B[1;31m means red. */
    while (1);
}

int SUCCESS(const char* format, ...) {
    LOG("\x1B[1;32m[SUCCESS] ", "\x1B[1;0m\n\r") /* \x1B[1;32m means green. */
}

int CRITICAL(const char* format, ...) {
    LOG("\x1B[1;33m[CRITICAL] ", "\x1B[1;0m\n\r") /* \x1B[1;33m means yellow. */
}
