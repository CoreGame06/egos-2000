/* 应用和系统服务的链接脚本，定义程序在进程虚拟地址空间中的布局。 */
OUTPUT_ARCH("riscv")

/* 从 apps/app.s 的 app_entry 开始执行。 */
ENTRY(app_entry)

MEMORY
{
    /* 两个区域的起点和容量。实际页权限由页表决定，当前统一使用 USER_RWX。 */
    code (rx) : ORIGIN = 0x80200000, LENGTH = 0x8000
    data (rw) : ORIGIN = 0x80208000, LENGTH = 0xF8000
}

PHDRS
{
    /* 程序头表中列出两个要加载的段。下面用 :code 和 :data 指定每个节归哪个段。 */
    code PT_LOAD;
    data PT_LOAD;
}

SECTIONS
{
    /* 指令放在代码区。 */
    .text : ALIGN(8) {
        *(.text .text.*)
    } >code :code

    /* 字符串和其它常量。 */
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

    /* 零初始化的变量在 ELF 里只记录空间大小，elf_load 装入时清零。 */
    .bss (NOLOAD): ALIGN(8) {
        *(.sbss*)
        *(.bss .bss.*)
        *(COMMON)
    } >data :data

    /* 堆从静态数据末尾开始。实际能用的空间取决于页表映射。 */
    .heap (NOLOAD) : ALIGN(8) {
        PROVIDE( __heap_start = . );
    } >data :data

    /* 堆的上界，后面就是 APPS_ARG 参数页。 */
    PROVIDE( __heap_end = 0x80300000 );
}
