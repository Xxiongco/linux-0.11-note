/*
 *  linux/lib/close.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

// 关闭文件函数
_syscall1(int,close,int,fd)
