#include "app.h"
#include <stdlib.h>

int main(int argc, char** argv) {
    // 用于观察抢占和调度：loop 一直运行；loop 3 运行 3 轮并输出。
    // loop 3 silent 不输出，便于观察时钟抢占；代码只检查参数个数，不检查 silent 的内容。
    for (uint i = 0; argc == 1 ? 1 : (i < atoi(argv[1])); i++) {
        for (uint j = 0; j < 1000000; j++);
        if (argc != 1 && argc != 3) printf("loop #%d\n\r", i);
    }
    return 0;
}
