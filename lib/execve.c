/*
 *  linux/lib/execve.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>


/**
 * @brief 加载并执行子进程函数
 * 
 * @param int 系统中断 0x80
 * @param execve 中断处理函数
 * @param file 被执行程序的文件名
 * @param argv 命令行参数指针数组
 * @param envp 环境变量指针数组
 */
_syscall3(int,execve,const char *,file,char **,argv,char **,envp)
