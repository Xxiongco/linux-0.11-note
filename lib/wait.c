/*
 *  linux/lib/wait.c
 *
 *  (C) 1991  Linus Torvalds
 */

// wait()将挂起当前进程，直到其子进程之一退出（终止），或者收到要求终止该进程的信号，或者是
// 需要调用一个信号句柄（信号处理程序）
// waitpid()挂起当前进程，直到 pid 指定的子进程退出（终止）或者收到要求终止该进程的信号，或
// 者是需要调用一个信号句柄（信号处理程序）。


#define __LIBRARY__
#include <unistd.h>
#include <sys/wait.h>

_syscall3(pid_t,waitpid,pid_t,pid,int *,wait_stat,int,options)

pid_t wait(int * wait_stat)
{
	return waitpid(-1,wait_stat,0);
}
