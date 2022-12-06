/*
 *  linux/lib/setsid.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

// 创建一个会话并设置进程组号
_syscall0(pid_t,setsid)
