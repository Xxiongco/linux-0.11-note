/*
 *  linux/lib/open.c
 *
 *  (C) 1991  Linus Torvalds
 */

// open()系统调用用于将一个文件名转换成一个文件描述符。当调用成功时，返回的文件描述符将是进
// 程没有打开的最小数值的描述符。该调用创建一个新的打开文件，并不与任何其它进程共享。在执行 exec
// 函数时，该新的文件描述符将始终保持着打开状态。文件的读写指针被设置在文件开始位置。

#define __LIBRARY__
#include <unistd.h>
#include <stdarg.h>

// 打开文件
int open(const char * filename, int flag, ...)
{
	register int res;
	va_list arg;

	va_start(arg,flag);
	__asm__("int $0x80"
		:"=a" (res)
		:"0" (__NR_open),"b" (filename),"c" (flag),
		"d" (va_arg(arg,int)));
	if (res>=0)
		return res;
	errno = -res;
	return -1;
}
