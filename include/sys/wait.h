#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

// 该头文件描述了进程等待时信息。包括一些符号常数和 wait()、waitpid()函数原型申明

#include <sys/types.h>

#define _LOW(v)		( (v) & 0377)           // 取低字节（8 进制表示） 0377 = 0xff
#define _HIGH(v)	( ((v) >> 8) & 0377)    // 取高字节

/* options for waitpid, WUNTRACED not supported */
#define WNOHANG		1       // 如果没有状态也不要挂起，并立刻返回
#define WUNTRACED	2       // 报告停止执行的子进程状态

#define WIFEXITED(s)	(!((s)&0xFF)                                // 如果子进程正常退出，则为真
#define WIFSTOPPED(s)	(((s)&0xFF)==0x7F)                          // 如果子进程正停止着，则为 true
#define WEXITSTATUS(s)	(((s)>>8)&0xFF)                             // 返回退出状态
#define WTERMSIG(s)	((s)&0x7F)                                      // 返回导致进程终止的信号值（信号量）
#define WSTOPSIG(s)	(((s)>>8)&0xFF)                                 // 返回导致进程停止的信号值
#define WIFSIGNALED(s)	(((unsigned int)(s)-1 & 0xFFFF) < 0xFF)     // 如果由于未捕捉到信号而导致子进程退出则为真

// 等待子进程的状态变化
pid_t wait(int *stat_loc);
// 等待特定子进程的状态变化
pid_t waitpid(pid_t pid, int *stat_loc, int options);

#endif
