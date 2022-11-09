#ifndef _UNISTD_H
#define _UNISTD_H
// 一些常用的标准库函数

/* ok, this may be a joke, but I'm working on it */
#define _POSIX_VERSION 198808L

#define _POSIX_CHOWN_RESTRICTED	/* only root can do a chown (I think..) */
#define _POSIX_NO_TRUNC		/* no pathname truncation (but see in kernel) 路径名不截断 */
#define _POSIX_VDISABLE '\0'	/* character to disable things like ^C */
/*#define _POSIX_SAVED_IDS */	/* we'll get to this yet */
/*#define _POSIX_JOB_CONTROL */	/* we aren't there quite yet. Soon hopefully */

#define STDIN_FILENO	0		// 标准输入文件句柄号
#define STDOUT_FILENO	1		// 标准输出
#define STDERR_FILENO	2		// 标准错误

#ifndef NULL
#define NULL    ((void *)0)
#endif

/* access */
// 以下定义的符号常数用于 access()函数
#define F_OK	0	// file exist
#define X_OK	1	// excutable
#define W_OK	2	// writable
#define R_OK	4	// readable

/* lseek */
//  文件指针重定位,用于 lseek()和 fcntl()函数 
#define SEEK_SET	0		// 将文件读写指针设置为偏移值
#define SEEK_CUR	1		// 当前值， 文件指针设置为当前值加上偏移值
#define SEEK_END	2		// 末尾值， 文件读写指针设置为文件长度加上偏移值

/* _SC stands for System Configuration. We don't use them much */
/* _SC 表示系统配置。我们很少使用 */ 
 // 下面的符号常数用于 sysconf()函数
#define _SC_ARG_MAX		1			// 最大变量数
#define _SC_CHILD_MAX		2		// 子进程最大数
#define _SC_CLOCKS_PER_SEC	3		// 每秒滴答数
#define _SC_NGROUPS_MAX		4		// 最大组数		
#define _SC_OPEN_MAX		5		// 最大打开文件数
#define _SC_JOB_CONTROL		6		// 作业控制
#define _SC_SAVED_IDS		7		// 保存的标识符
#define _SC_VERSION		8			// 版本

/* more (possibly) configurable things - now pathnames */
/* 更多的（可能的）可配置参数 - 现在用于路径名 */ 
 // 下面的符号常数用于 pathconf()函数
#define _PC_LINK_MAX		1		// 连接最大数
#define _PC_MAX_CANON		2		// 最大常规文件数
#define _PC_MAX_INPUT		3		// 最大输入长度
#define _PC_NAME_MAX		4		// 名称最大长度
#define _PC_PATH_MAX		5		// 路径最大长度
#define _PC_PIPE_BUF		6		// 管道缓冲大小
#define _PC_NO_TRUNC		7		// 文件名不截断
#define _PC_VDISABLE		8
#define _PC_CHOWN_RESTRICTED	9	// 改变宿主受限

#include <sys/stat.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <utime.h>

#ifdef __LIBRARY__

// 以下是内核实现的系统调用符号常数，用于作为系统调用函数表中的索引值。( include/linux/sys.h )
#define __NR_setup	0	/* used only by init, to get system going */
#define __NR_exit	1
#define __NR_fork	2
#define __NR_read	3
#define __NR_write	4
#define __NR_open	5
#define __NR_close	6
#define __NR_waitpid	7
#define __NR_creat	8
#define __NR_link	9
#define __NR_unlink	10
#define __NR_execve	11
#define __NR_chdir	12
#define __NR_time	13
#define __NR_mknod	14
#define __NR_chmod	15
#define __NR_chown	16
#define __NR_break	17
#define __NR_stat	18
#define __NR_lseek	19
#define __NR_getpid	20
#define __NR_mount	21
#define __NR_umount	22
#define __NR_setuid	23
#define __NR_getuid	24
#define __NR_stime	25
#define __NR_ptrace	26
#define __NR_alarm	27
#define __NR_fstat	28
#define __NR_pause	29
#define __NR_utime	30
#define __NR_stty	31
#define __NR_gtty	32
#define __NR_access	33
#define __NR_nice	34
#define __NR_ftime	35
#define __NR_sync	36
#define __NR_kill	37
#define __NR_rename	38
#define __NR_mkdir	39
#define __NR_rmdir	40
#define __NR_dup	41
#define __NR_pipe	42
#define __NR_times	43
#define __NR_prof	44
#define __NR_brk	45
#define __NR_setgid	46
#define __NR_getgid	47
#define __NR_signal	48
#define __NR_geteuid	49
#define __NR_getegid	50
#define __NR_acct	51
#define __NR_phys	52
#define __NR_lock	53
#define __NR_ioctl	54
#define __NR_fcntl	55
#define __NR_mpx	56
#define __NR_setpgid	57
#define __NR_ulimit	58
#define __NR_uname	59
#define __NR_umask	60
#define __NR_chroot	61
#define __NR_ustat	62
#define __NR_dup2	63
#define __NR_getppid	64
#define __NR_getpgrp	65
#define __NR_setsid	66
#define __NR_sigaction	67
#define __NR_sgetmask	68
#define __NR_ssetmask	69
#define __NR_setreuid	70
#define __NR_setregid	71

// 以下定义系统调用嵌入式汇编宏函数。 
 // 不带参数的系统调用宏函数。type name(void)。 
 // %0 - eax(__res)，%1 - eax(__NR_##name)。其中 name 是系统调用的名称，与 __NR_ 组合形成上面 
 // 的系统调用符号常数，从而用来对系统调用表中函数指针寻址。 
 // 返回：如果返回值大于等于 0，则返回该值，否则置出错号 errno，并返回-1。
#define _syscall0(type,name) \
type name(void) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
	: "=a" (__res) \
	: "0" (__NR_##name)); \
if (__res >= 0) \
	return (type) __res; \
errno = -__res; \
return -1; \
}

#define _syscall1(type,name,atype,a) \
type name(atype a) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
	: "=a" (__res) \
	: "0" (__NR_##name),"b" ((long)(a))); \
if (__res >= 0) \
	return (type) __res; \
errno = -__res; \
return -1; \
}

#define _syscall2(type,name,atype,a,btype,b) \
type name(atype a,btype b) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
	: "=a" (__res) \
	: "0" (__NR_##name),"b" ((long)(a)),"c" ((long)(b))); \
if (__res >= 0) \
	return (type) __res; \
errno = -__res; \
return -1; \
}

#define _syscall3(type,name,atype,a,btype,b,ctype,c) \
type name(atype a,btype b,ctype c) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
	: "=a" (__res) \
	: "0" (__NR_##name),"b" ((long)(a)),"c" ((long)(b)),"d" ((long)(c))); \
if (__res>=0) \
	return (type) __res; \
errno=-__res; \
return -1; \
}

#endif /* __LIBRARY__ */

extern int errno;    // 出错号，全局变量

// 对应各系统调用的函数原型定义
// 检查调用进程是否可以对指定的文件执行某种操作
	// if(access("test.txt", R_OK)==0)  printf("READ OK\n");
	// if(access("test.txt", W_OK)==0)  printf("WRITE OK\n");
	// if(access("test.txt", X_OK)==0)  printf("EXEC OK\n");
	// if(access("test.txt", F_OK)==0)  printf("File exist\n");
int access(const char * filename, mode_t mode);
// acct系统调用可用于启动/禁止进程信息记录功能； 未实现
int acct(const char * filename);
// 设置定时器(闹钟)。在指定seconds后，内核会给当前进程发送14）SIGALRM信号
int alarm(int sec);
// 把内存末尾指针设置为addr.
int brk(void * end_data_segment);
// 把内存的末尾指针移动increment个字节
void * sbrk(ptrdiff_t increment);
// 改变当前工作目录
int chdir(const char * filename);
// 改变文件访问方式
int chmod(const char * filename, mode_t mode);
// 改变文件所有者
int chown(const char * filename, uid_t owner, gid_t group);
// 改变根目录
int chroot(const char * filename);
// 关闭文件
int close(int fildes);
// 以给定的文件访问方式创建文件
int creat(const char * filename, mode_t mode);
// 复制文件描述符
int dup(int fildes);
// 运行程序
int execve(const char * filename, char ** argv, char ** envp);
int execv(const char * pathname, char ** argv);
int execvp(const char * file, char ** argv);
int execl(const char * pathname, char * arg0, ...);
int execlp(const char * file, char * arg0, ...);
int execle(const char * pathname, char * arg0, ...);
// 退出
volatile void exit(int status);
volatile void _exit(int status);
// 操作文件描述符属性
int fcntl(int fildes, int cmd, ...);
// 复制进程
int fork(void);
// 取得id
int getpid(void);
int getuid(void);
int geteuid(void);
int getgid(void);
int getegid(void);
// 设备控制， 实现设备打开、关闭、读、写等功能
int ioctl(int fildes, int cmd, ...);
// 发送信号
int kill(pid_t pid, int signal);
// 添加链接
int link(const char * filename1, const char * filename2);
// 移动文件访问指针
int lseek(int fildes, off_t offset, int origin);
// 创建文件， 第一个参数表示你要创建的文件的名称，第二个参数表示文件类型，第三个参数表示该文件对应的设备文件的设备号
int mknod(const char * filename, mode_t mode, dev_t dev);
// 挂载目录
int mount(const char * specialfile, const char * dir, int rwflag);
// 将当前进程运行的优先级增加指定值
int nice(int val);
// 打开文件
int open(const char * filename, int flag, ...);
// 暂停，等待信号唤醒
int pause(void);
// 创建管道
int pipe(int * fildes);
// 读文件
int read(int fildes, char * buf, off_t count);
// 设置id
int setpgrp(void);
int setpgid(pid_t pid,pid_t pgid);
int setuid(uid_t uid);
int setgid(gid_t gid);
// 设置信号
void (*signal(int sig, void (*fn)(int)))(int);
// 返回与此命名文件有关的信息结构
int stat(const char * filename, struct stat * stat_buf);
// 获取已在描述符fields上打开文件的有关信息
int fstat(int fildes, struct stat * stat_buf);
// 精确到秒
int stime(time_t * tptr);
// 将所有修改过的块缓冲区排入写队列，然后就返回，它并不等待实际写磁盘操作结束
int sync(void);
// 得到当前日历时间或者设置日历时间，参数一般为NULL
time_t time(time_t * tloc);
// 获取进程时间
time_t times(struct tms * tbuf);
// ulimit -c unlimited   设置coredump文件大小
// ulimint -a 用来显示当前的各种用户进程限制
// 数据段长度：ulimit -d unlimited
// 最大内存大小：ulimit -m unlimited
// 堆栈大小：ulimit -s unlimited
int ulimit(int cmd, long limit);    //设置进程结构相关信息
// 在创建文件时设置或者屏蔽掉文件的一些权限。一般与open()函数配合使用
mode_t umask(mode_t mask);
// 取消挂载
int umount(const char * specialfile);
// 获取当前内核名称和其它信息
int uname(struct utsname * name);
// 取消链接
int unlink(const char * filename);
// 获取文件状态
int ustat(dev_t dev, struct ustat * ubuf);
// 修改文件的存取时间和更改时间
int utime(const char * filename, struct utimbuf * times);
// waitpid()会暂时停止目前进程的执行, 直到有信号来到或子进程结束
// waitpid函数提供了wait函数没有提供的三个功能：
// 1、waitpid等待一个特定的进程，而wait则返回任一终止子进程的状态 。
// 2、waitpid提供了一个 wait的非阻塞版本，有时希望取得一个子进程的状态， 但不想进程阻塞。
// 3、waitpid支持作业控制。
pid_t waitpid(pid_t pid,int * wait_stat,int options);
// 暂时停止目前进程的执行, 直到有信号来到或子进程结束
pid_t wait(int * wait_stat);
// 写文件
int write(int fildes, const char * buf, off_t count);
// 复制文件句柄
int dup2(int oldfd, int newfd);
// 设id
int getppid(void);
pid_t getpgrp(void);
pid_t setsid(void);

#endif
