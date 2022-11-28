/*
 *  linux/fs/fcntl.c
 *
 *  (C) 1991  Linus Torvalds
 */
// 文件控控制

#include <string.h>
#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <fcntl.h>
#include <sys/stat.h>

extern int sys_close(int fd);

/**
 * @brief 复制文件句柄
 * 
 * @param fd 源句柄
 * @param arg 新文件句柄的最小数值
 * @return int 新文件句柄或出错码
 */
static int dupfd(unsigned int fd, unsigned int arg)
{
	if (fd >= NR_OPEN || !current->filp[fd])
		return -EBADF;
	if (arg >= NR_OPEN)
		return -EINVAL;
	while (arg < NR_OPEN)
		if (current->filp[arg])
			arg++;
		else
			break;
	if (arg >= NR_OPEN)
		return -EMFILE;
	current->close_on_exec &= ~(1<<arg);
	(current->filp[arg] = current->filp[fd])->f_count++;	// === key === 引用计数增加
	return arg;
}

// 复制指定文件句柄 oldfd 为句柄值等于 newfd 的句柄。如果 newfd 已经打开，则首先关闭之
int sys_dup2(unsigned int oldfd, unsigned int newfd)
{
	sys_close(newfd);
	return dupfd(oldfd,newfd);
}

// 复制指定文件句柄 oldfd 为句柄值最小的未用句柄
int sys_dup(unsigned int fildes)
{
	return dupfd(fildes,0);
}

/**
 * @brief 文件句柄操作函数
 * 
 * @param fd 文件句柄
 * @param cmd 操作命令
 * @param arg 指定的新文件句柄，或文件状态和访问模式
 * @return int 有多种含义，如文件句柄号，句柄的flag， 0-成功， -1-错误
 */
int sys_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg)
{	
	struct file * filp;

	if (fd >= NR_OPEN || !(filp = current->filp[fd]))
		return -EBADF;
	switch (cmd) {
		case F_DUPFD:		// 复制fd
			return dupfd(fd,arg);
		case F_GETFD:		// 取文件句柄的执行时关闭标志
			return (current->close_on_exec>>fd)&1;
		case F_SETFD:		// 设置句柄执行时关闭标志
			if (arg&1)
				current->close_on_exec |= (1<<fd);
			else
				current->close_on_exec &= ~(1<<fd);
			return 0;
		case F_GETFL:		// 取文件状态标志和访问模式
			return filp->f_flags;
		case F_SETFL:		// 设置文件状态和访问模式(根据 arg 设置添加、非阻塞标志)
			filp->f_flags &= ~(O_APPEND | O_NONBLOCK);
			filp->f_flags |= arg & (O_APPEND | O_NONBLOCK);
			return 0;
		case F_GETLK:	case F_SETLK:	case F_SETLKW:		// 未实现
			return -1;
		default:
			return -1;
	}
}
