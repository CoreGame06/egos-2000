/* malloc 和 free 由工具链的 C 库提供，egos 在这里实现它们使用的堆增长接口。 */

#include "egos.h"

// 链接脚本 app.lds 和 egos.lds 定义堆的起点和上界。
extern char __heap_start, __heap_end;
// brk 是当前堆的末尾，初始指向堆起点。
static char* brk = &__heap_start;

// malloc 发现现有堆空间不够时，会请求把堆末尾向后移 size 字节。
// _sbrk 返回旧末尾，也就是这次新增空间的起点。
// 这里只移动指针，堆空间能否访问取决于已有的页表映射。
char* _sbrk(int size) {
    if (brk + size > (char*)&__heap_end) {
        printf("_sbrk: heap grows too large\n\r");
        *(int*)(0) = 1; // 超出堆上界时，故意写地址 0 触发内存异常。
    }

    char* old_brk = brk;
    brk += size;
    return old_brk;
}
