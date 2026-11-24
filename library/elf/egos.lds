/* 内核链接脚本，将 Earth、Grass 和用到的库代码链接成 egos.elf。 */
OUTPUT_ARCH("riscv")

/* 指定 ELF 的入口符号。启动用的 egos.bin 由 objcopy 从 ELF 导出。 */
ENTRY(boot_loader)

MEMORY
{
    /* 内核代码从 RAM 起点开始，数据紧接着 32 KiB 的代码区。 */
    code (rx) : ORIGIN = 0x80000000, LENGTH = 0x8000
    data (rw) : ORIGIN = 0x80008000, LENGTH = 0xF8000
}

PHDRS
{
    /* 将节分别安排进代码段和数据段。 */
    code PT_LOAD;
    data PT_LOAD;
}

SECTIONS
{
    /* 把 earth/boot.s 的入口指令放在镜像最前面。 */
    .init : ALIGN(8) {
        *(.text.enter)
    } >code :code

    /* 内核的其它指令。 */
    .text : ALIGN(8) {
        *(.text .text.*)
    } >code :code

    /* 常量和字符串。 */
    .rodata : ALIGN(8) {
        *(.rdata)
        *(.rodata .rodata.*)
        . = ALIGN(8);
        *(.srodata .srodata.*)
    } >data :data

    /* 有初始值的全局变量和静态变量。 */
    .data : ALIGN(8) {
        *(.data .data.*)
        . = ALIGN(8);
        *(.sdata .sdata.* .sdata2.*)
    } >data :data

    /* 零初始化的全局变量和静态变量，启动时需要有清零后的内存。 */
    .bss (NOLOAD): ALIGN(8) {
        *(.sbss*)
        *(.bss .bss.*)
        *(COMMON)
    } >data :data

    /* 内核堆从静态数据末尾开始，栈由启动汇编另外设置。 */
    .heap (NOLOAD) : ALIGN(8) {
        PROVIDE( __heap_start = . );
    } >data :data

    /* 堆的上界，后面是 Earth 接口表。 */
    PROVIDE( __heap_end = 0x80100000 );
}
