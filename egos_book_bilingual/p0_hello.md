# Hello, World!

> **你好，世界！**

This project helps you understand the following C program:
> 本项目旨在帮助你理解以下 C 语言程序：

```c
int main() {
    printf("Hello, World!\n\r");
    return 0;
}
```

Specifically, you will learn about 3 types of CPU instructions (integer computation, control transfer, memory access) and 4 types of memory regions (code, data, heap, stack), which are fundamental to every computer program.
> 具体而言，你将学习 3 种 CPU 指令类型（整数计算、控制转移、内存访问）和 4 种内存区域类型（代码段、数据段、堆、栈），这些是所有计算机程序的基础。

Prerequisites: an entry-level systems course equivalent to Cornell CS3410 or CMU 15-213 is typically required before learning OS. This project aims to revisit the concepts of CPU instructions and memory regions rather than replace an entry-level course.
> 先修要求：在学习操作系统之前，通常需要修读过相当于康奈尔大学 CS3410 或卡耐基梅隆大学 CMU 15-213 的入门级系统课程。本项目的目的是重温 CPU 指令与内存区域的相关概念，而不是替代入门课程。

## Get started

> **快速上手**

Follow the instructions in steps 1 and 2 in `USAGES.md`. After you can compile egos-2000 and run it on QEMU, switch to the hello branch by git checkout hello. Run this branch by make qemu, and then you will see the following:
> 请按照 `USAGES.md` 中步骤 1 和步骤 2 的说明进行操作。在成功编译 egos-2000 并将其在 QEMU 上运行后，通过 `git checkout hello` 切换到 hello 分支。使用 `make qemu` 运行该分支，你将看到如下内容：

```
-------- Compile Hello, World! --------
riscv-none-elf-gcc -march=rv32ima_zicsr -mabi=ilp32 -Wl,--gc-sections -ffunction-sections -fdata-sections -fdiagnostics-show-option -fno-builtin hello.s hello.c -Thello.lds -nostdlib -lc -lgcc -o hello.elf
riscv-none-elf-objdump --source --all-headers --demangle --line-numbers --wide hello.elf > hello.lst
riscv-none-elf-objcopy -O binary hello.elf hello.bin
-------- Run Hello-World on QEMU --------
qemu-system-riscv32 -nographic -machine virt -smp 1 -m 4M -bios hello.bin
Hello, World!
```

The code in this branch prints Hello, World! on the screen and stalls. You can exit QEMU by typing Ctrl+a x, after which you will see QEMU: Terminated.
> 该分支中的代码会在屏幕上打印 Hello, World!，然后陷入死循环/停滞。你可以通过输入 Ctrl+a x 退出 QEMU，随后你将看到 QEMU: Terminated。

Next, we explain the code in this hello branch, which gives good examples of the 3 types of CPU instructions (integer computation, control transfer, memory access) and 4 types of memory regions (code, data, heap, stack). After the explanation, you will extend this starter code by implementing the printf function as an exercise.
> 接下来，我们将讲解这个 hello 分支中的代码，它很好地展示了 3 种 CPU 指令类型（整数计算、控制转移、内存访问）和 4 种内存区域类型（代码段、数据段、堆、栈）。在讲解结束后，你将通过实现 `printf` 函数作为练习来扩展这份初始代码。

## Instructions and memory

> **指令与内存**

### Before `main()`

> **进入 `main()` 之前**

Let's start with the first two lines of `hello.lds`:
> 让我们从 `hello.lds` 的前两行开始：

```c
OUTPUT_ARCH("riscv")
ENTRY(_start)
```

The first line says that the output architecture for this computer program is RISC-V. This book will focus on RISC-V, which is widely used in education, but other important CPU architectures, such as Intel x86 and ARM, also exist. The second line says that the entry point of this program is `_start` in `hello.s`. We will move our attention to `hello.s` and ignore the rest of `hello.lds` for now.
> 第一行说明该计算机程序的输出架构是 RISC-V。本书将重点讨论在教学中广泛使用的 RISC-V，但同时也存在其他重要的 CPU 架构，如 Intel x86 和 ARM。第二行说明该程序的入口点是 `hello.s` 中的 `_start`。我们现在将注意力转移到 `hello.s` 上，暂时忽略 `hello.lds` 的其余部分。

According to `_start` in `hello.s`, the first two CPU instructions in this program are:
> 根据 `hello.s` 中的 `_start`，该程序中的前两条 CPU 指令为：

```assembly
li sp, 0x80400000
call main
```

The `li` instruction loads the 4-byte integer `0x80400000` to the stack pointer register, `sp`. `li` is an example of an integer computation instruction because it modifies a register with an integer value. Other examples of integer computation include addition, multiplication, division, bitwise shift operations, and bitwise AND/OR/XOR operations.
> `li` 指令将 4 字节整数 `0x80400000` 加载到栈指针寄存器 `sp` 中。`li` 是整数计算指令的一个示例，因为它用一个整数值更新了寄存器的内容。整数计算的其他示例包括加法、乘法、除法、位移操作以及按位与/或/异或运算。

After setting `sp`, the call instruction transfers control to `main()` in `hello.c`. The call instruction is an example of a control transfer instruction, and it does two things: (1) set the program counter to the first instruction of `main()`; (2) set the `ra` register to the address of call `_end` in `hello.s`, which is the instruction next to call main. Here, `sp` specifies the stack memory that `main()` will use, and `ra` means return address, i.e., the program counter after `main()` returns. We now move our attention to the code of `main()`.
> 在设置好 `sp` 之后，`call` 指令将控制权转移到 `hello.c` 中的 `main()`。`call` 指令是控制转移指令的一个示例，它完成了两件事：（1）将程序计数器（PC）设置为 `main()` 第一条指令的地址；（2）将 `ra` 寄存器设置为 `hello.s` 中 `call _end` 的地址，即紧跟在 `call main` 之后的下一条指令。在此，`sp` 指定了 `main()` 将要使用的栈内存，而 `ra` 表示返回地址（return address），即 `main()` 返回后恢复执行的程序计数器地址。现在我们将注意力转向 `main()` 的代码。

### In `main()`

> **在 `main()` 内部**

The C code of `main()` in `hello.c` is simple:
> `hello.c` 中 `main()` 的 C 语言代码非常简单：

```c
int main() {
    char* msg = "Hello, World!\n\r";
    terminal_write(msg, 15);
    return 0;
}
```

After make, you can find the CPU instructions for `main()` in `hello.lst`:
> 在运行 `make` 之后，你可以在 `hello.lst` 中找到 `main()` 的 CPU 指令：

```text
80000078 <main>:
main():
80000078:   fe010113            addi    sp,sp,-32
8000007c:   00112e23            sw  ra,28(sp)
80000080:   00812c23            sw  s0,24(sp)
80000084:   02010413            addi    s0,sp,32
80000088:   800007b7            lui a5,0x80000
8000008c:   0b878793            addi    a5,a5,184 # 800000b8 <main+0x40>
80000090:   fef42623            sw  a5,-20(s0)
80000094:   00f00593            li  a1,15
80000098:   fec42503            lw  a0,-20(s0)
8000009c:   f75ff0ef            jal 80000010 <terminal_write>
800000a0:   00000793            li  a5,0
800000a4:   00078513            mv  a0,a5
800000a8:   01c12083            lw  ra,28(sp)
800000ac:   01812403            lw  s0,24(sp)
800000b0:   02010113            addi    sp,sp,32
800000b4:   00008067            ret
```

There are 16 instructions for `main()` starting at address `0x80000078`, each occupying four bytes. Therefore, the call main in `hello.s` sets the program counter to `0x80000078`, which points to an `addi` instruction. The binary form of this instruction is `0xfe010113`, and such binary encoding is explained in the RISC-V Instruction Set Manual. For our purpose, we ignore the binary encoding rules and focus on the instructions themselves. Let's start by reading the 7 instructions highlighted above.
> `main()` 函数从地址 `0x80000078` 开始，共有 16 条指令，每条指令占用 4 个字节。因此，`hello.s` 中的 `call main` 会将程序计数器设置为 `0x80000078`，该地址指向一条 `addi` 指令。该指令的二进制形式为 `0xfe010113`，这种二进制编码在《RISC-V 指令集手册》中有详细说明。就我们的学习目的而言，我们可以暂时忽略二进制编码规则，将重点放在指令本身。让我们首先阅读上面突出标出的 7 条指令。

### Prologue and epilogue

> **序言与结语**

The two instructions at addresses `0x80000078` and `0x800000b0` add the integers -32 and 32 to the `sp` register, meaning that the 32-byte region `[0x80400000 - 32, 0x80400000)` is used as the stack of `main()`. The `ret` will cause a control transfer to the instruction pointed to by `ra`, i.e., call `_end` in `hello.s` as we have explained earlier. Such usage of `sp` and `ra` is the common structure of every function in C.
> 位于地址 `0x80000078` 和 `0x800000b0` 的两条指令分别将整数 -32 和 32 加到 `sp` 寄存器中，这意味着 32 字节的区域 `[0x80400000 - 32, 0x80400000)` 被用作 `main()` 函数的栈帧。`ret` 指令会导致控制权转移到 `ra` 所指向的指令，即前文解释过的 `hello.s` 中的 `call _end`。对 `sp` 和 `ra` 的这种配合使用是 C 语言中每个函数的典型结构。

Note that `ra` should still hold the address of call `_end` when executing `ret`, which is ensured by these two instructions:

```text
8000007c:   00112e23            sw  ra,28(sp)
...
800000a8:   01c12083            lw  ra,28(sp)
```

28(`sp`) means adding 28 to the `sp` register to form the address. Since the `sp` register equals `0x80400000` - 32, the `sw` instruction stores the 4 bytes of `ra` to the memory region `[0x80400000 - 32 + 28, 0x80400000)`, and the `lw` loads 4 bytes from the region back into `ra`. As long as this 4-byte region is unmodified, `ra` can be freely modified during the execution of `main()`.
> `28(sp)` 表示将 `sp` 寄存器的值加上 28 来计算目标地址。由于 `sp` 寄存器的值为 `0x80400000` - 32，因此 `sw` 指令将 `ra` 的 4 字节数据存入内存区域 `[0x80400000 - 32 + 28, 0x80400000)`，而 `lw` 指令则将 4 个字节从该区域重新加载回 `ra`。只要这 4 字节的内存区域未被修改，在 `main()` 的执行过程中就可以自由修改 `ra`。

These are examples of memory access instructions. Other examples include lb/sb, which load/store one byte from/to memory, while lw/sw load/store 4 bytes at a time.
> 这些是内存访问指令的示例。其他示例包括 `lb`/`sb`（分别从/向内存加载/存储一个字节），而 `lw`/`sw` 则一次加载/存储 4 个字节。

> [!TIP]
> > **TIP / 提示**

> Up to this point, we have seen examples of all 3 types of CPU instructions (integer computation, control transfer, memory access) that constitute the unprivileged instructions of RISC-V. P0 and P1 will only use unprivileged instructions. Starting from P2, you will see the privileged instructions and registers that are needed for operating systems.
> > 到目前为止，我们已经看到了构成 RISC-V 非特权指令的所有 3 种 CPU 指令类型（整数计算、控制转移、内存访问）的示例。P0 和 P1 只会使用非特权指令。从 P2 开始，你将看到操作系统所需的特权指令和特权寄存器。

### First line of `main()`

> **`main()` 的第一行**

```c
    char* msg = "Hello, World!\n\r";
```

is compiled to the following 4 instructions.
> 被编译为以下 4 条指令：

```text
80000084:   02010413            addi    s0,sp,32
80000088:   800007b7            lui a5,0x80000
8000008c:   0b878793            addi    a5,a5,184 # 800000b8 <main+0x40>
80000090:   fef42623            sw  a5,-20(s0)
```

`lui` stands for Load-Upper-Immediate, which means that the upper 20 bits of `a5` are set to `0x80000` while the lower 12 bits are set to 0. Therefore, these 4 instructions (1) set `s0` to `0x80400000`; (2) set `a5` to `0x800000b8`; (3) store the 4 bytes of integer `0x800000b8` at memory address `0x80400000` - 20. This means two things:

* The compiler puts the "Hello, World!\n\r" string at memory address `0x800000b8`.
> 编译器将字符串 `"Hello, World!\n\r"` 放置在内存地址 `0x800000b8` 处。

* The compiler determines that msg, a local variable of `main()`, occupies 4 bytes of memory starting at `0x80400000` - 20 on the 32-byte `main()` stack.
> 编译器将 `main()` 的局部变量 `msg` 分配在 32 字节的 `main()` 栈空间内，占用从 `0x80400000` - 20 开始的 4 字节内存。

> [!TIP]
> > **TIP / 提示**

> This is a good point to revise the concept of pointers. In C, a char is simply one byte storing the ASCII code of a character. For example, the ASCII codes for H and e are 72 and 101, so memory addresses `0x800000b8` and `0x800000b9` hold 72 and 101, respectively. The type of msg, namely char*, is called a char pointer, meaning that variable msg holds the memory address of a char. This book targets 32-bit CPUs so that every memory address is represented by 4 bytes (i.e., all C pointers hold 4 bytes). One can dereference the msg pointer with syntax *msg. For example, *msg == 'H' and *(msg + 1) == 'e'.
> > 这是重温指针概念的好时机。在 C 语言中，`char` 只是存储字符 ASCII 码的一个字节。例如，'H' 和 'e' 的 ASCII 码分别是 72 和 101，因此内存地址 `0x800000b8` 和 `0x800000b9` 分别保存 72 和 101。`msg` 的类型（即 `char*`）被称为字符指针，意味着变量 `msg` 存放了一个 `char` 的内存地址。本书面向 32 位 CPU，因此每个内存地址都用 4 字节表示（即所有 C 指针都占用 4 个字节）。可以通过语法 `*msg` 来解引用 `msg` 指针。例如，`*msg == 'H'` 以及 `*(msg + 1) == 'e'`。

### Second and third line

> **第二行和第三行**

```c
    terminal_write(msg, 15);
    return 0;
```

are compiled to the following 5 instructions.
> 被编译为以下 5 条指令：

```text
80000094:   00f00593            li  a1,15
80000098:   fec42503            lw  a0,-20(s0)
8000009c:   f75ff0ef            jal 80000010 <terminal_write>
800000a0:   00000793            li  a5,0
800000a4:   00078513            mv  a0,a5
```

Registers `a0` and `a1` hold the first two arguments when calling a function, while `a0` also holds the return value. Therefore, `a1` is set to 15, and `a0` is set to `0x800000b8` by loading 4 bytes from address `0x80400000` - 20. `jal` is an alias of call, and the first instruction of `terminal_write` is at address `0x80000010`. After `terminal_write` returns, `a0` is set to 0 according to the last line in `main()`. It is possible to do `li` `a0`,0 without using `a5` if the compiler does more optimizations.
> 寄存器 `a0` 和 `a1` 在调用函数时用于传递前两个参数，同时 `a0` 也用于保存函数返回值。因此，`a1` 被设置为 15，而 `a0` 则通过从地址 `0x80400000` - 20 处加载 4 字节而被设置为 `0x800000b8`。`jal` 指令是 `call` 的别名，`terminal_write` 的第一条指令位于地址 `0x80000010`。在 `terminal_write` 返回后，根据 `main()` 中的最后一行代码，`a0` 被置 0。如果编译器开启更多优化，完全可以在不借助于 `a5` 的情况下直接执行 `li a0,0`。

### In `terminal_write()`

> **在 `terminal_write()` 内部**

For `terminal_write()`, we only explain the C code, and you should read all its instructions in `hello.lst` as an exercise.
> 对于 `terminal_write()`，我们只解释其 C 语言代码，你应该阅读 `hello.lst` 中它的所有指令作为练习。

```c
void terminal_write(const char *str, int len) {
    for (int i = 0; i < len; i++) {
        *(char*)(0x10000000) = str[i];
    }
}
```

For P0, it is enough to know that writing a byte to this special address `0x10000000` will print a character on the screen. P5 will introduce the details of I/O devices.
> 对于 P0，只需知道向这个特殊地址 `0x10000000` 写入一个字节就会在屏幕上打印一个字符即可。P5 将介绍 I/O 设备的详细信息。

*(char*)(`0x10000000`) converts `0x10000000` into a char pointer and then dereferences it, so it stands for the byte at memory address `0x10000000` which is written by char str[i].
> `*(char*)(0x10000000)` 将 `0x10000000` 转换为字符指针 `char*` 并对其解引用，因此它代表通过赋值 `str[i]` 所写入的内存地址 `0x10000000` 处的字节。

### Code, data and stack

> **代码段、数据段与栈**

After reading `main()` and `terminal_write()`, let's review the memory regions touched by these two functions, starting with the following part of `hello.lst`:

```text
Sections:
Idx Name              Size      VMA       LMA       File off  Algn  Flags
  0 .init             0000000c  80000000  80000000  00001000  2**3  CONTENTS, ALLOC, LOAD, READONLY, CODE
  1 .text             000000a8  80000010  80000010  00001010  2**3  CONTENTS, ALLOC, LOAD, READONLY, CODE
  2 .rodata           00000010  800000b8  800000b8  000010b8  2**3  CONTENTS, ALLOC, LOAD, READONLY, DATA
```

A section refers to a memory region specified by a starting address (VMA/LMA) and a size.
> 一个节（section）指的是由起始地址（VMA/LMA）和字节大小所限定的内存区域。

The .init section starts at `0x80000000` and holds the three instructions in `hello.s`. By default, `0x80000000` is the initial program counter. Section .text holds the instructions for `terminal_write()` and `main()` starting at `0x80000010` and ending at `0x800000b8`. Sections .init and .text combined form the code region of this program.
> `.init` 节从 `0x80000000` 开始，保存 `hello.s` 中的三条指令。默认情况下，`0x80000000` 是初始程序计数器。`.text` 节保存 `terminal_write()` 和 `main()` 的指令，起始于 `0x80000010`，终止于 `0x800000b8`。`.init` 节和 `.text` 节组合在一起构成了该程序的代码段（code region）。

The 16 bytes starting at `0x800000b8` form the data region of this program. The data region would typically contain 3 sections:
> 从 `0x800000b8` 开始的 16 个字节构成了该程序的数据段（data region）。数据段通常包含 3 个节：

* .rodata holds read-only data such as the "Hello, World!\n\r" string.
> `.rodata` 保存只读数据，例如 `"Hello, World!\n\r"` 字符串。

* .bss holds global variables with zero initial values.
> `.bss` 保存初始值为零的全局变量。

* .data holds global variables with non-zero initial values.
> `.data` 保存初始值非零的全局变量。

This program has no global variables, so we only see .rodata. The end of .rodata is `0x800000c8`, which is the start of the heap region. You will learn about the heap region in the dynamic memory allocation part of this project.
> 由于本程序没有全局变量，因此我们只看到了 `.rodata`。`.rodata` 节的末尾是 `0x800000c8`，这也正是堆区（heap region）的起始位置。你将在本项目的“动态内存分配”一节中学习堆区。

Lastly, the stack region starts at `0x80400000` and grows down. The 32-byte `main()` stack is `[0x80400000 - 32, 0x80400000)`, and the stack of `terminal_write()` is the 48-byte region `[0x80400000 - 32 - 48, 0x80400000 - 32)`. The choice of `0x80400000` as the top of the stack is not mandatory; it is possible to load a different value into `sp` in `hello.s`.
> 最后，栈区（stack region）从 `0x80400000` 开始并向下增长。32 字节的 `main()` 栈帧空间为 `[0x80400000 - 32, 0x80400000)`，而 `terminal_write()` 的栈帧则是 48 字节区域 `[0x80400000 - 32 - 48, 0x80400000 - 32)`。选择 `0x80400000` 作为栈顶并不是强制性的；完全可以在 `hello.s` 中向 `sp` 寄存器加载其他不同的值。

## Formatted output

> **格式化输出**

The `terminal_write` function takes a string and prints it on the screen, but we may further wish to print integers, pointers in hexadecimal format, etc. We thus introduce the printf function, which converts a format into a string and prints it on the screen.
> `terminal_write` 函数接收一个字符串并在屏幕上输出，但我们可能还希望打印整数、十六进制格式的指针等。因此我们引入了 `printf` 函数，它能将格式化字符串转换并打印到屏幕上。

```c
printf("%d is a number\n\r", 1234);
printf("%s is a string\n\r", "abcd");
printf("%s-%d is awesome!\n\r", "egos", 2000);
```

These examples show that printf takes a string as its first argument, with special placeholders: %d for integers and %s for strings. For example, the last line would print "egos-2000 is awesome!\n\r" according to the 3 arguments.
> 这些示例表明 `printf` 接收一个字符串作为其首个参数，并带有特殊的占位符：`%d` 用于整数，`%s` 用于字符串。例如，最后一行代码将根据传入的 3 个参数输出 `"egos-2000 is awesome!\n\r"`。

Uncomment the printf from `main()`, and uncomment functions `format_to_str()` and `printf()` in `hello.c`. Do make qemu again, and you will see the following:

```
Hello, World!
egos-2000 is awesome!
```

Let's start by reading the first 3 lines of this code block:
> 让我们从阅读该代码块的前 3 行开始：

```c
#include <stdlib.h>  // for itoa() and utoa()
#include <string.h>  // for strlen() and strcat()
#include <stdarg.h>  // for va_start(), va_end(), va_arg() and va_copy()
```

Intuitively, the code includes part of the C standard library. For example, strlen takes a string and returns its length, and the code of strlen is provided by the `string.h` library in the C compiler, so you don't have to write it yourself. Similarly, strcat in the `string.h` library concatenates two strings into a single one. itoa in `stdlib.h` converts an integer to a string in ASCII encoding (i.e., Integer-to-ASCII).
> 直观来看，这段代码包含并使用了 C 标准库的一部分。例如，`strlen` 接收一个字符串并返回其长度，`strlen` 的实现由 C 编译器的 `string.h` 库提供，因此你无需自行编写。类似地，`string.h` 中的 `strcat` 可以将两个字符串拼接为一个。`stdlib.h` 中的 `itoa` 则将整数转换为 ASCII 编码的字符串（即 Integer-to-ASCII）。

Lastly, the `stdarg.h` library provides `va_start`, `va_end`, and `va_arg`, where va stands for variable argument (i.e., the argument list of `printf()` does not have a fixed length, so different invocations of `printf()` can take different numbers of arguments). In `format_to_str()`, `va_arg`(args, int) reads an int-type argument from the argument list, and itoa converts the integer to a string, which is appended to the output string.
> 最后，`stdarg.h` 库提供了 `va_start`、`va_end` 和 `va_arg`，其中 `va` 代表可变参数（variable argument，即 `printf()` 的参数列表长度不固定，不同调用的 `printf()` 可以传入不同数量的参数）。在 `format_to_str()` 中，`va_arg(args, int)` 从参数列表中读取一个 `int` 类型参数，`itoa` 将该整数转换为字符串并追加到输出字符串的末尾。

> [!TIP]
> > **TIP / 提示**

> Read the assembly code of `format_to_str()` and `printf()` in `hello.lst`. You can skip library functions like strlen or strcat, and focus on instructions within these two functions. You will get a precise understanding by connecting the C code with the CPU instructions, which a verbal explanation cannot provide. For example, you will see how `va_arg` is compiled to a few load and store instructions without doing any function calls.
> > 阅读 `hello.lst` 中 `format_to_str()` 和 `printf()` 的汇编代码。你可以跳过像 `strlen` 或 `strcat` 这样的库函数，将注意力集中在这两个函数内部的指令上。将 C 代码与 CPU 指令结合对照分析，能带给你仅凭文字解释无法企及的精确理解。例如，你将看到 `va_arg` 是如何被编译成几条简单的加载与存储指令，而无需发起任何函数调用的。

After gaining a better understanding of `format_to_str()` and `printf()`, your job in this project is to modify `format_to_str()` so that the code below functions correctly. For %u, a placeholder for a 4-byte unsigned integer, you can use the utoa function in `stdlib.h`. For %llu, a placeholder for an 8-byte unsigned integer, you will need to implement a ulltoa function that converts the 8-byte unsigned integer into a string (i.e., an array of ASCII characters).
> 在深入理解 `format_to_str()` 和 `printf()` 之后，你在本项目中的任务是修改 `format_to_str()`，使下面的代码能够正确运行。对于 `%u`（4 字节无符号整数的占位符），你可以使用 `stdlib.h` 中的 `utoa` 函数。对于 `%llu`（8 字节无符号整数的占位符），你需要实现一个 `ulltoa` 函数，将 8 字节无符号整数转换为字符串（即 ASCII 字符数组）。

```c
printf("%c is character $", '$');
printf("%c is character 0", (char)48);
printf("%x is integer 1234 in hexadecimal", 1234);
printf("%u is the maximum of unsigned int", (unsigned int)0xFFFFFFFF);
printf("%p is the hexadecimal address of the hello-world string", msg);
printf("%llu is the maximum of unsigned long long", 0xFFFFFFFFFFFFFFFFULL);
```

The expected output is shown below, and the hexadecimal address of the hello-world string is likely different from the `0x800029f8` printed by our solution code.
> 预期输出如下所示，并且 hello-world 字符串的十六进制地址可能与我们参考代码打印的 `0x800029f8` 不同。

```
$ is character $
0 is character 0
4d2 is integer 1234 in hexadecimal
4294967295 is the maximum of unsigned int
0x800029f8 is the hexadecimal address of the hello-world string
18446744073709551615 is the maximum of unsigned long long
```

> [!TIP]
> > **TIP / 提示**

> Can you explain why your printing is different from `0x800029f8`? Similarly, can you explain why the address of the hello-world string used to be `0x800000b8` earlier in this project?
> > 你能解释为什么你打印出的地址可能与 `0x800029f8` 不同吗？类似地，你能解释为什么在本项目的早些时候 hello-world 字符串的地址曾经是 `0x800000b8` 吗？

## Dynamic memory allocation

> **动态内存分配**

The char buf[512] in `printf()` allocates 512 bytes on the `printf()` stack, which holds the output string generated by `format_to_str()`. However, the output string can be longer than 512, and its length is typically unknown at compile time. This leads to the need for dynamic memory allocation:
> `printf()` 中的 `char buf[512]` 在 `printf()` 的栈帧上分配了 512 字节的内存空间，用于保存由 `format_to_str()` 生成的输出字符串。然而，输出字符串的长度可能会超过 512 字节，且通常在编译期无法预知。这就产生了对动态内存分配的需求：

```c
int printf(const char* format, ...) {
    va_list args;
    va_start(args, format);

va_list args_copy;
    va_copy(args_copy, args);
    unsigned int len = format_to_str_len(format, args_copy);
    char *buf = malloc(len);

format_to_str(buf, format, args);
    va_end(args);
    terminal_write(buf, strlen(buf));

va_end(args_copy);
> free(buf);

return 0;
}
```

In this last part of P0, you will implement the `format_to_str_len` function, which calculates the length of the output string based on format and args. Calling `va_copy()` avoids any interference between `format_to_str_len()` and `format_to_str()`. After malloc(len), pointer buf holds an address such that the len bytes starting at buf can be freely used. After free(buf), this len-byte region can no longer be used.
> 在 P0 的最后一部分中，你将实现 `format_to_str_len` 函数，该函数根据 `format` 和 `args` 计算输出字符串的长度。调用 `va_copy()` 可以避免 `format_to_str_len()` 与 `format_to_str()` 之间产生相互干扰。在执行 `malloc(len)` 之后，指针 `buf` 保存了一个起始地址，使得从 `buf` 开始的 `len` 字节内存可以被自由使用。而在执行 `free(buf)` 之后，这 `len` 字节的内存区域将不再允许使用。

### Heap

> **堆**

Before explaining more about malloc and free, we first explain the heap memory region using the code below, which you need to uncomment in `hello.c`.
> 在进一步讲解 `malloc` 和 `free` 之前，我们首先借助下面的代码来解释堆内存区域（heap region），你需要取消 `hello.c` 中这部分代码的注释。

```c
extern char __heap_start, __heap_end;
static char* brk = &__heap_start;
char* _sbrk(int size) {
    if (brk + size > (char*)&__heap_end) {
        terminal_write("_sbrk: heap grows too large\r\n", 29);
        return NULL;
    }

char* old_brk = brk;
    brk += size;
    return old_brk;
}
```

`__heap_start` and `__heap_end` are defined in `hello.lds`: `__heap_start` is also the end of the data region (i.e., the end of the .bss section), while `__heap_end` is hard-coded to `0x80200000`, just like the initial stack pointer is hard-coded to `0x80400000`.
> `__heap_start` 和 `__heap_end` 在 `hello.lds` 中定义：`__heap_start` 同时也是数据段的末尾（即 `.bss` 节的结尾），而 `__heap_end` 则被硬编码为 `0x80200000`，正如初始栈指针被硬编码为 `0x80400000` 一样。

The goal is to ensure that the heap and stack do not overlap as the heap grows up and the stack grows down, which introduces the concept of a break pointer (i.e., the brk above, pointing to the end of the heap). The stack and the heap do not overlap as long as the break pointer is lower than the stack pointer. We simply assume that the stack pointer is never lower than `0x80200000` (i.e., the stack size is never larger than 2MB), and the brk variable never grows greater than `0x80200000`.
> 我们的目标是确保当堆向上增长、栈向下增长时，堆与栈不会发生碰撞重叠。为此，我们引入了 break 指针（即代码中的 `brk`，指向堆顶/堆末尾）的概念。只要 break 指针保持在栈指针下方，栈与堆就不会相撞。我们简单地假设栈指针永远不会低于 `0x80200000`（即栈的空间大小永远不会超过 2MB），同时 `brk` 变量也永远不会增长超过 `0x80200000`。

### Malloc and free

> **Malloc 与 free**

The initial heap size is 0 since brk is initialized as `__heap_start`, and the heap region is defined as [`__heap_start`, brk). Say malloc(512) is the first time invoking `malloc()`. Three things will happen within malloc:
> 由于 `brk` 被初始化为 `__heap_start`，因此初始堆大小为 0，堆区域被定义为 [`__heap_start`, brk)。假设 `malloc(512)` 是第一次调用 `malloc()`，`malloc` 内部将会执行三步操作：

* Invoke `_sbrk`() with an argument greater than 512, say 4096 (i.e., 4KB).
> 传入大于 512 的参数（例如 4096，即 4KB）来调用 `_sbrk()`。

* With the return value of `_sbrk`(), initialize some data structures in the heap.
> 利用 `_sbrk()` 返回的地址，在堆中初始化管理数据结构。

* Using the data structures, find 512 bytes in this heap region and return their address.
> 利用这些数据结构，在该堆内存区域中划分出 512 字节并返回其首地址。

Say malloc(32) is invoked next. It is likely that `malloc()` can find 32 bytes using the data structures without having to call `_sbrk`() again to increase the heap. When `free()` is invoked, the data structures are updated so that the freed memory can be reused later.
> 假设随后再次调用了 `malloc(32)`。此时 `malloc()` 很可能可以直接利用现有数据结构在堆中找到 32 字节的空间，而无需再次调用 `_sbrk()` 来扩充堆内存。当调用 `free()` 时，这些数据结构会被更新，以便被释放的内存随后可以被重新分配使用。

The data structures for `malloc()` and `free()` are implemented in the `stdlib.h` library from the C compiler so that you don't have to know the details—just like you don't have to know the details of itoa or strcat. However, you are more than welcome to design your own data structures and implement your own malloc and free functions.
> 用于管理 `malloc()` 与 `free()` 的数据结构由 C 编译器的 `stdlib.h` 库提供并实现，因此你无需了解其底层细节——就像你不需要关注 `itoa` 或 `strcat` 的具体实现一样。当然，非常欢迎你自主设计数据结构，实现一套属于你自己的 `malloc` 和 `free` 函数。

## Accomplishments

> **学习产出与成就**

You have finished reading library/libc/* in egos-2000. You also get the intuition behind library/elf/*, which initializes the 4 memory regions for a program, given its executable binary in ELF format.
> 至此，你已经研读完了 egos-2000 中 `library/libc/*` 的全部内容。对于给定的 ELF 格式可执行二进制程序，你也直观地理解了 `library/elf/*` 是如何为其初始化 4 个内存区域的背后原理。
