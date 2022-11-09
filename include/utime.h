#ifndef _UTIME_H
#define _UTIME_H
// 该文件定义了文件访问和修改时间结构 utimbuf{}以及 utime()函数原型

#include <sys/types.h>	/* I know - shouldn't do this, but .. */

struct utimbuf {
	time_t actime;    // 文件访问时间。从 1970.1.1:0:0:0 开始的秒数
	time_t modtime;   // 文件修改时间。从 1970.1.1:0:0:0 开始的秒数
};

// 设置文件访问和修改时间函数
extern int utime(const char *filename, struct utimbuf *times);

#endif
