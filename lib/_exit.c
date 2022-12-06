/*
 *  linux/lib/_exit.c
 *
 *  (C) 1991  Linus Torvalds
 */

#define __LIBRARY__
#include <unistd.h>

/**
 * @brief 系统退出函数，调用中断 int 0x80
 * 
 * @param exit_code 退出码
 * @return volatile 
 */
volatile void _exit(int exit_code)
{
	__asm__("int $0x80"::"a" (__NR_exit),"b" (exit_code));
}
