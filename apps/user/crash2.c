#include "app.h"
#include "egos.h"

int main() {
    // 把物理页池地址当作用户虚拟地址访问，当前页表未映射该范围，用于测试访问异常。
    memset((void*)APPS_PAGES_BASE, 0, RAM_END - APPS_PAGES_BASE);
    return 0;
}
