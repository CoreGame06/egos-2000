#include "app.h"

// 输出命令名之后的参数，例如 echo hello 输出 hello。
int main(int argc, char** argv) {
    for (uint i = 1; i < argc; i++) printf("%s ", argv[i]);
    printf("\n\r");
    return 0;
}
