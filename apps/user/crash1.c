#include "app.h"
#include <stdlib.h>

int main() {
    // 申请 32MiB 超出堆边界，_sbrk 会访问地址 0 触发异常，用于测试进程异常回收。
    char* heap_overflow = malloc(32 * 1024 * 1024);
    return 0;
}
