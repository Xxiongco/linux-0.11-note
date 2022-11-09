#ifndef _FCNTL_H
#define _FCNTL_H
//file descriptor control?

// fcntl函数有5种功能：
// 　1.复制一个现有的描述符（cmd=F_DUPFD）.
// 　2.获得／设置文件描述符标记(cmd=F_GETFD或F_SETFD).
//   3.获得／设置文件状态标记(cmd=F_GETFL或F_SETFL).
//   4.获得／设置异步I/O所有权(cmd=F_GETOWN或F_SETOWN).
//   5.获得／设置记录锁(cmd=F_GETLK,F_SETLK或F_SETLKW).


#include <sys/types.h>

/* open/fcntl - NOCTTY, NDELAY isn't implemented yet */
#define O_ACCMODE	00003                        // 文件访问模式屏蔽码 access
// 打开文件 open()和文件控制 fcntl()函数使用的  文件访问模式  。同时只能使用三者之一
#define O_RDONLY	   00                      // 以只读方式打开文件
#define O_WRONLY	   01                      // 只写
#define O_RDWR		   02                      // 读写
// 下面是  文件创建标志，用于 open()。可与上面访问模式用'位或'的方式一起使用
#define O_CREAT		00100	/* not fcntl */   // 如果文件不存在就创建
#define O_EXCL		00200	/* not fcntl */   // 独占使用
#define O_NOCTTY	00400	/* not fcntl */   // 不分配控制终端， no contrl tty
#define O_TRUNC		01000	/* not fcntl */   // 若文件已经存在且是写操作，则长度截为0
#define O_APPEND	02000                     // 以添加方式打开，文件指针置为文件尾
#define O_NONBLOCK	04000	/* not fcntl */   // 非阻塞方式打开和操作文件
#define O_NDELAY	O_NONBLOCK                // 非阻塞方式打开和操作文件

/* Defines for fcntl-commands. Note that currently
 * locking isn't supported, and other things aren't really
 * tested.
 */
#define F_DUPFD		0	/* dup 拷贝*/ 
#define F_GETFD		1	/* get f_flags 取得文件句柄标志*/
#define F_SETFD		2	/* set f_flags 设置文件句柄标志*/
#define F_GETFL		3	/* more flags (cloexec) 取文件状态和访问模式*/
#define F_SETFL		4                           // 设置文件状态标志和访问模式
// 下面是文件锁定命令。fcntl()的第三个参数 lock 是指向 flock 结构的指针。
#define F_GETLK		5	/* not implemented */  // 返回阻止锁定的 flock 结构
#define F_SETLK		6                          // 设置锁
#define F_SETLKW	7                          // 等待设置或清除锁定 set lock wait

/* for F_[GET|SET]FL */
/* 用于 F_GETFL 或 F_SETFL */ 
 // 在执行 exec()簇函数时关闭文件句柄。(执行时关闭 - Close On EXECution)
#define FD_CLOEXEC	1	/* actually anything with low bit set goes */

/* Ok, these are locking features, and aren't implemented at any
 * level. POSIX wants them.
 */
#define F_RDLCK		0   // 共享或读文件锁定
#define F_WRLCK		1   // 独占或写文件锁定
#define F_UNLCK		2   // 文件解锁

/* Once again - not implemented, but ... */
struct flock {
	short l_type;           // 锁定类型（F_RDLCK，F_WRLCK，F_UNLCK）
	short l_whence;         // 开始偏移(SEEK_SET，SEEK_CUR 或 SEEK_END)
	off_t l_start;          // 阻塞锁定的开始处。相对偏移（字节数）
	off_t l_len;            // 阻塞锁定的大小；如果是 0 则为到文件末尾
	pid_t l_pid;            // 加锁的进程 id
};  


// 以下是使用上述标志或命令的函数原型。 
 // 创建新文件或重写一个已存在文件。 
 // 参数 filename 是欲创建文件的文件名，mode 是创建文件的属性（参见 include/sys/stat.h）
extern int creat(const char * filename,mode_t mode);
// 文件句柄操作，会影响文件的打开。 
 // 参数 fildes 是文件句柄，cmd 是操作命令，见上面 23-30 行
extern int fcntl(int fildes,int cmd, ...);
// 打开文件。在文件与文件句柄之间建立联系。 
 // 参数 filename 是欲打开文件的文件名，flags 是上面 7-17 行上的标志的组合
extern int open(const char * filename, int flags, ...);

#endif
