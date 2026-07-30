Hello, World! | The EGOS book              Skip to content 

AppearanceMenuReturn to top























# Hello, World! ​

This project helps you understand the following C program.c
```
int main() {
    printf("Hello, World!\n\r");
    return 0;
}
```
1234

Specifically, you will learn about 3 types of CPU instructions (integer computation, control transfer, memory access) and 4 types of memory regions (code, data, heap, stack), which are fundamental to every computer program.

Prerequisites: an entry-level systems course equivalent to Cornell CS3410 or CMU 15-213 is typically required before learning OS. This project aims to revisit the concepts of CPU instructions and memory regions rather than replace an entry-level course.

## Get started ​

Follow the instructions in steps 1 and 2 in USAGES.md. After you can compile egos-2000 and run it on QEMU, switch to the hello branch by git checkout hello. Run this branch by make qemu, and then you will see the following:
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

Next, we explain the code in this hello branch, which gives good examples of the 3 types of CPU instructions (integer computation, control transfer, memory access) and 4 types of memory regions (code, data, heap, stack). After the explanation, you will extend this starter code by implementing the printf function as an exercise.

## Instructions and memory ​

### Before main() ​

Let's start with the first two lines of hello.lds.lds
```
OUTPUT_ARCH("riscv")
ENTRY(_start)
```
12

The first line says that the output architecture for this computer program is RISC-V. This book will focus on RISC-V, which is widely used in education, but other important CPU architectures, such as Intel x86 and ARM, also exist. The second line says that the entry point of this program is _start in hello.s. We will move our attention to hello.s and ignore the rest of hello.lds for now.

According to _start in hello.s, the first two CPU instructions in this program areasm
```
li sp, 0x80400000
call main
```
12

The li instruction loads the 4-byte integer 0x80400000 to the stack pointer register, sp. li is an example of an integer computation instruction because it modifies a register with an integer value. Other examples of integer computation include addition, multiplication, division, bitwise shift operations, and bitwise AND/OR/XOR operations.

After setting sp, the call instruction transfers control to main() in hello.c. The call instruction is an example of a control transfer instruction, and it does two things: (1) set the program counter to the first instruction of main(); (2) set the ra register to the address of call _end in hello.s, which is the instruction next to call main. Here, sp specifies the stack memory that main() will use, and ra means return address, i.e., the program counter after main() returns. We now move our attention to the code of main().

### In main() ​

The C code of main() in hello.c is simple:c
```
int main() {
    char* msg = "Hello, World!\n\r";
    terminal_write(msg, 15);
    return 0;
}
```
12345

After make, you can find the CPU instructions for main() in hello.lst:lst
```
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
123456789101112131415161718

There are 16 instructions for main() starting at address 0x80000078, each occupying four bytes. Therefore, the call main in hello.s sets the program counter to 0x80000078, which points to an addi instruction. The binary form of this instruction is 0xfe010113, and such binary encoding is explained in the RISC-V Instruction Set Manual. For our purpose, we ignore the binary encoding rules and focus on the instructions themselves. Let's start by reading the 7 instructions highlighted above.

# Prologue and epilogue ​

The two instructions at addresses 0x80000078 and 0x800000b0 add the integers -32 and 32 to the sp register, meaning that the 32-byte region [0x80400000 - 32, 0x80400000) is used as the stack of main(). The ret will cause a control transfer to the instruction pointed to by ra, i.e., call _end in hello.s as we have explained earlier. Such usage of sp and ra is the common structure of every function in C.

Note that ra should still hold the address of call _end when executing ret, which is ensured by these two instructions:lst
```
8000007c:   00112e23            sw  ra,28(sp)
...
800000a8:   01c12083            lw  ra,28(sp)
```


28(sp) means adding 28 to the sp register to form the address. Since the sp register equals 0x80400000 - 32, the sw instruction stores the 4 bytes of ra to the memory region [0x80400000 - 32 + 28, 0x80400000), and the lw loads 4 bytes from the region back into ra. As long as this 4-byte region is unmodified, ra can be freely modified during the execution of main().

These are examples of memory access instructions. Other examples include lb/sb, which load/store one byte from/to memory, while lw/sw load/store 4 bytes at a time.

TIP

Up to this point, we have seen examples of all 3 types of CPU instructions (integer computation, control transfer, memory access) that constitute the unprivileged instructions of RISC-V. P0 and P1 will only use unprivileged instructions. Starting from P2, you will see the privileged instructions and registers that are needed for operating systems.

# First line of main() ​c
```
    char* msg = "Hello, World!\n\r";
```
2

is compiled to the following 4 instructions.lst
```
80000084:   02010413            addi    s0,sp,32
80000088:   800007b7            lui a5,0x80000
8000008c:   0b878793            addi    a5,a5,184 # 800000b8 <main+0x40>
80000090:   fef42623            sw  a5,-20(s0)
```
6789

lui stands for Load-Upper-Immediate, which means that the upper 20 bits of a5 are set to 0x80000 while the lower 12 bits are set to 0. Therefore, these 4 instructions (1) set s0 to 0x80400000; (2) set a5 to 0x800000b8; (3) store the 4 bytes of integer 0x800000b8 at memory address 0x80400000 - 20. This means two things:
* 

The compiler puts the "Hello, World!\n\r" string at memory address 0x800000b8.
* 

The compiler determines that msg, a local variable of main(), occupies 4 bytes of memory starting at 0x80400000 - 20 on the 32-byte main() stack.

TIP

This is a good point to revise the concept of pointers. In C, a char is simply one byte storing the ASCII code of a character. For example, the ASCII codes for H and e are 72 and 101, so memory addresses 0x800000b8 and 0x800000b9 hold 72 and 101, respectively. The type of msg, namely char*, is called a char pointer, meaning that variable msg holds the memory address of a char. This book targets 32-bit CPUs so that every memory address is represented by 4 bytes (i.e., all C pointers hold 4 bytes). One can dereference the msg pointer with syntax *msg. For example, *msg == 'H' and *(msg + 1) == 'e'.

# Second and third line ​c
```
    terminal_write(msg, 15);
    return 0;
```
34

are compiled to the following 5 instructions.lst
```
80000094:   00f00593            li  a1,15
80000098:   fec42503            lw  a0,-20(s0)
8000009c:   f75ff0ef            jal 80000010 <terminal_write>
800000a0:   00000793            li  a5,0
800000a4:   00078513            mv  a0,a5
```
1011121314

Registers a0 and a1 hold the first two arguments when calling a function, while a0 also holds the return value. Therefore, a1 is set to 15, and a0 is set to 0x800000b8 by loading 4 bytes from address 0x80400000 - 20. jal is an alias of call, and the first instruction of terminal_write is at address 0x80000010. After terminal_write returns, a0 is set to 0 according to the last line in main(). It is possible to do li a0,0 without using a5 if the compiler does more optimizations.

### In terminal_write() ​

For terminal_write(), we only explain the C code, and you should read all its instructions in hello.lst as an exercise.c
```
void terminal_write(const char *str, int len) {
    for (int i = 0; i < len; i++) {
        *(char*)(0x10000000) = str[i];
    }
}
```
12345

For P0, it is enough to know that writing a byte to this special address 0x10000000 will print a character on the screen. P5 will introduce the details of I/O devices.

*(char*)(0x10000000) converts 0x10000000 into a char pointer and then dereferences it, so it stands for the byte at memory address 0x10000000 which is written by char str[i].

### Code, data and stack ​

After reading main() and terminal_write(), let's review the memory regions touched by these two functions, starting with the following part of hello.lst:lst
```
Sections:
Idx Name              Size      VMA       LMA       File off  Algn  Flags
  0 .init             0000000c  80000000  80000000  00001000  2**3  CONTENTS, ALLOC, LOAD, READONLY, CODE
  1 .text             000000a8  80000010  80000010  00001010  2**3  CONTENTS, ALLOC, LOAD, READONLY, CODE
  2 .rodata           00000010  800000b8  800000b8  000010b8  2**3  CONTENTS, ALLOC, LOAD, READONLY, DATA
```


A section refers to a memory region specified by a starting address (VMA/LMA) and a size.

The .init section starts at 0x80000000 and holds the three instructions in hello.s. By default, 0x80000000 is the initial program counter. Section .text holds the instructions for terminal_write() and main() starting at 0x80000010 and ending at 0x800000b8. Sections .init and .text combined form the code region of this program.

The 16 bytes starting at 0x800000b8 form the data region of this program. The data region would typically contain 3 sections:
* .rodata holds read-only data such as the "Hello, World!\n\r" string.
* .bss holds global variables with zero initial values.
* .data holds global variables with non-zero initial values.

This program has no global variables, so we only see .rodata. The end of .rodata is 0x800000c8, which is the start of the heap region. You will learn about the heap region in the dynamic memory allocation part of this project.

Lastly, the stack region starts at 0x80400000 and grows down. The 32-byte main() stack is [0x80400000 - 32, 0x80400000), and the stack of terminal_write() is the 48-byte region [0x80400000 - 32 - 48, 0x80400000 - 32). The choice of 0x80400000 as the top of the stack is not mandatory; it is possible to load a different value into sp in hello.s.

## Formatted output ​

The terminal_write function takes a string and prints it on the screen, but we may further wish to print integers, pointers in hexadecimal format, etc. We thus introduce the printf function, which converts a format into a string and prints it on the screen.c
```
printf("%d is a number\n\r", 1234);
printf("%s is a string\n\r", "abcd");
printf("%s-%d is awesome!\n\r", "egos", 2000);
```
123

These examples show that printf takes a string as its first argument, with special placeholders: %d for integers and %s for strings. For example, the last line would print "egos-2000 is awesome!\n\r" according to the 3 arguments.

Uncomment the printf from main(), and uncomment functions format_to_str() and printf() in hello.c. Do make qemu again, and you will see the following:
```
Hello, World!
egos-2000 is awesome!
```


Let's start by reading the first 3 lines of this code block:c
```
#include <stdlib.h>  // for itoa() and utoa()
#include <string.h>  // for strlen() and strcat()
#include <stdarg.h>  // for va_start(), va_end(), va_arg() and va_copy()
```


Intuitively, the code includes part of the C standard library. For example, strlen takes a string and returns its length, and the code of strlen is provided by the string.h library in the C compiler, so you don't have to write it yourself. Similarly, strcat in the string.h library concatenates two strings into a single one. itoa in stdlib.h converts an integer to a string in ASCII encoding (i.e., Integer-to-ASCII).

Lastly, the stdarg.h library provides va_start, va_end, and va_arg, where va stands for variable argument (i.e., the argument list of printf() does not have a fixed length, so different invocations of printf() can take different numbers of arguments). In format_to_str(), va_arg(args, int) reads an int-type argument from the argument list, and itoa converts the integer to a string, which is appended to the output string.

TIP

Read the assembly code of format_to_str() and printf() in hello.lst. You can skip library functions like strlen or strcat, and focus on instructions within these two functions. You will get a precise understanding by connecting the C code with the CPU instructions, which a verbal explanation cannot provide. For example, you will see how va_arg is compiled to a few load and store instructions without doing any function calls.

After gaining a better understanding of format_to_str() and printf(), your job in this project is to modify format_to_str() so that the code below functions correctly. For %u, a placeholder for a 4-byte unsigned integer, you can use the utoa function in stdlib.h. For %llu, a placeholder for an 8-byte unsigned integer, you will need to implement a ulltoa function that converts the 8-byte unsigned integer into a string (i.e., an array of ASCII characters).c
```
printf("%c is character $", '$');
printf("%c is character 0", (char)48);
printf("%x is integer 1234 in hexadecimal", 1234);
printf("%u is the maximum of unsigned int", (unsigned int)0xFFFFFFFF);
printf("%p is the hexadecimal address of the hello-world string", msg);
printf("%llu is the maximum of unsigned long long", 0xFFFFFFFFFFFFFFFFULL);
```
123456

The expected output is shown below, and the hexadecimal address of the hello-world string is likely different from the 0x800029f8 printed by our solution code.
```
$ is character $
0 is character 0
4d2 is integer 1234 in hexadecimal
4294967295 is the maximum of unsigned int
0x800029f8 is the hexadecimal address of the hello-world string
18446744073709551615 is the maximum of unsigned long long
```


TIP

Can you explain why your printing is different from 0x800029f8? Similarly, can you explain why the address of the hello-world string used to be 0x800000b8 earlier in this project?

## Dynamic memory allocation ​

The char buf[512] in printf() allocates 512 bytes on the printf() stack, which holds the output string generated by format_to_str(). However, the output string can be longer than 512, and its length is typically unknown at compile time. This leads to the need for dynamic memory allocation:c
```
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
    free(buf);

    return 0;
}
```
123456789101112131415161718

In this last part of P0, you will implement the format_to_str_len function, which calculates the length of the output string based on format and args. Calling va_copy() avoids any interference between format_to_str_len() and format_to_str(). After malloc(len), pointer buf holds an address such that the len bytes starting at buf can be freely used. After free(buf), this len-byte region can no longer be used.

### Heap ​

Before explaining more about malloc and free, we first explain the heap memory region using the code below, which you need to uncomment in hello.c.c
```
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
123456789101112

__heap_start and __heap_end are defined in hello.lds: __heap_start is also the end of the data region (i.e., the end of the .bss section), while __heap_end is hard-coded to 0x80200000, just like the initial stack pointer is hard-coded to 0x80400000.

The goal is to ensure that the heap and stack do not overlap as the heap grows up and the stack grows down, which introduces the concept of a break pointer (i.e., the brk above, pointing to the end of the heap). The stack and the heap do not overlap as long as the break pointer is lower than the stack pointer. We simply assume that the stack pointer is never lower than 0x80200000 (i.e., the stack size is never larger than 2MB), and the brk variable never grows greater than 0x80200000.

### Malloc and free ​

The initial heap size is 0 since brk is initialized as __heap_start, and the heap region is defined as [__heap_start, brk). Say malloc(512) is the first time invoking malloc(). Three things will happen within malloc:
* Invoke _sbrk() with an argument greater than 512, say 4096 (i.e., 4KB).
* With the return value of _sbrk(), initialize some data structures in the heap.
* Using the data structures, find 512 bytes in this heap region and return their address.

Say malloc(32) is invoked next. It is likely that malloc() can find 32 bytes using the data structures without having to call _sbrk() again to increase the heap. When free() is invoked, the data structures are updated so that the freed memory can be reused later.

The data structures for malloc() and free() are implemented in the stdlib.h library from the C compiler so that you don't have to know the details—just like you don't have to know the details of itoa or strcat. However, you are more than welcome to design your own data structures and implement your own malloc and free functions.

## Accomplishments ​

You have finished reading library/libc/* in egos-2000. You also get the intuition behind library/elf/*, which initializes the 4 memory regions for a program, given its executable binary in ELF format.