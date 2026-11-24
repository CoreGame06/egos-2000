/* 每个新进程第一次运行时，都会从 app.s 的 app_entry 开始，再进入自己的 main()。
    进程的启动入口：取 argc -> 设置栈 -> 调用 main -> 请求退出。
 * 到这里已是用户态，页表也已选好；工作线程直接从自己的线程函数开始执行。 */

    .section .text
    .global app_entry
app_entry:
    /* a0 原来是 argc 的地址，读出参数个数后放回 a0；a1 已经是 argv 的地址。 */
    lw a0, 0(a0)

    /* 设置虚拟栈顶，栈向低地址增长 */
    li sp,0x80400000

    /* 调用 main(argc, argv)。sys_proc 的第二个参数特殊，指向启动锁和核心启动计数。 */
    call main

    /* main 的返回值作为 exit 的参数，请求 sys_proc 回收当前进程。 */
    call exit
