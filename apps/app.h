/* 应用共用的接口指针和当前工作目录。 */

#pragma once

#include "egos.h"
#include "syscall.h" // 消息收发、线程创建等函数的声明。

//将 Earth 和 Grass 保存为指针，以后可以通过指针调用
struct grass* grass = (void*)GRASS_STRUCT;
struct earth* earth = (void*)EARTH_STRUCT;

// Shell 和 cd 共享当前工作目录，cd 修改后 Shell 也能读到。

// 当前目录的 inode 号，存放在共享内存的前 4 字节。
#define workdir_ino (*(int*)(SHELL_WORK_DIR))
// 当前目录的路径字符串，紧接在 inode 号后面。
#define workdir     ((char*)(SHELL_WORK_DIR + sizeof(int)))
