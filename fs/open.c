/*
 *  linux/fs/open.c
 *
 *  (C) 1991  Linus Torvalds
 */
// 操作文件相关函数。 打开，关闭，创建目录，改变模式等

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>

// 取文件系统信息系统调用函数
int sys_ustat(int dev, struct ustat * ubuf)
{
	return -ENOSYS;
}

/**
 * @brief 设置文件访问和修改时间。
 * 如果 times 指针不为 NULL，则取 utimbuf 结构中的时间信息来设置文件的访问和修改时间；
 * 如果 times 指针是 NULL，则取系统当前时间来设置指定文件的访问和修改时间域。
 * 
 * @param filename 文件名
 * @param times 是访问和修改时间结构指针
 * @return int 
 */
int sys_utime(char * filename, struct utimbuf * times)
{
	struct m_inode * inode;
	long actime,modtime;

	if (!(inode=namei(filename)))
		return -ENOENT;
	if (times) {
		actime = get_fs_long((unsigned long *) &times->actime);
		modtime = get_fs_long((unsigned long *) &times->modtime);
	} else
		actime = modtime = CURRENT_TIME;
	inode->i_atime = actime;
	inode->i_mtime = modtime;
	inode->i_dirt = 1;
	iput(inode);
	return 0;
}

/*
 * XXX should we use the real or effective uid?  BSD uses the real uid,
 * so as to make this call useful to setuid programs.
 */
/**
 * @brief 检查对文件的访问权限
 * 
 * @param filename 文件名
 * @param mode 屏蔽码，由 R_OK(4)、W_OK(2)、X_OK(1)和 F_OK(0)组成
 * @return int 0-允许访问，others-错误码
 */
int sys_access(const char * filename,int mode)
{
	struct m_inode * inode;
	int res, i_mode;

	mode &= 0007;
	if (!(inode=namei(filename)))
		return -EACCES;
	i_mode = res = inode->i_mode & 0777;
	iput(inode);
	if (current->uid == inode->i_uid)
		res >>= 6;
	else if (current->gid == inode->i_gid)
		res >>= 6;
	if ((res & 0007 & mode) == mode)
		return 0;
	/*
	 * XXX we are doing this test last because we really should be
	 * swapping the effective with the real user id (temporarily),
	 * and then calling suser() routine.  If we do call the
	 * suser() routine, it needs to be called last. 
	 */
	if ((!current->uid) &&
	    (!(mode & 1) || (i_mode & 0111)))
		return 0;
	return -EACCES;
}

// 改变当前工作目录
int sys_chdir(const char * filename)
{
	struct m_inode * inode;

	if (!(inode = namei(filename)))
		return -ENOENT;
	if (!S_ISDIR(inode->i_mode)) {
		iput(inode);
		return -ENOTDIR;
	}
	iput(current->pwd);
	current->pwd = inode;
	return (0);
}

// 改变根目录，将指定的路径名改为根目录'/'
int sys_chroot(const char * filename)
{
	struct m_inode * inode;

	if (!(inode=namei(filename)))
		return -ENOENT;
	if (!S_ISDIR(inode->i_mode)) {
		iput(inode);
		return -ENOTDIR;
	}
	iput(current->root);
	current->root = inode;
	return (0);
}

//// 修改文件属性
int sys_chmod(const char * filename,int mode)
{
	struct m_inode * inode;

	if (!(inode=namei(filename)))
		return -ENOENT;
	if ((current->euid != inode->i_uid) && !suser()) {
		iput(inode);
		return -EACCES;
	}
	inode->i_mode = (mode & 07777) | (inode->i_mode & ~07777);
	inode->i_dirt = 1;
	iput(inode);
	return 0;
}

/**
 * @brief 修改文件宿主
 * 
 * @param filename 文件名
 * @param uid 用户id
 * @param gid 组id
 * @return int 0-成功，others-错误码
 */
int sys_chown(const char * filename,int uid,int gid)
{
	struct m_inode * inode;

	if (!(inode=namei(filename)))
		return -ENOENT;
	if (!suser()) {
		iput(inode);
		return -EACCES;
	}
	inode->i_uid=uid;
	inode->i_gid=gid;
	inode->i_dirt=1;
	iput(inode);
	return 0;
}


/**
 * @brief 打开（或创建）文件
 * 
 * @param filename 文件名
 * @param flag 打开文件标志：  O_RDONLY 只写；O_WRONLY 只读； O_RDWR 读写，O_CREAT 创建；
 * 							  O_EXCL 如果使用O_CREAT时文件存在，就返回错误信息，它可以测试文件是否存在；O_APPEND 追加
 * @param mode 文件的许可属性： S_IRWXU 文件宿主具有读、写和执行权限； S_IRUSR 用户具有读文件权限； S_IRWXG 组成员具有读、写和执行权限 
 * @return int 成功-返回文件句柄，失败-错误码
 */
int sys_open(const char * filename,int flag,int mode)
{
	struct m_inode * inode;
	struct file * f;
	int i,fd;

	// 将用户设置的模式与进程的模式屏蔽码相与，产生许可的文件模式
	mode &= 0777 & ~current->umask;
	for(fd=0 ; fd<NR_OPEN ; fd++) 
		if (!current->filp[fd])    // 查找一个filp中的空闲项
			break;
	if (fd>=NR_OPEN)
		return -EINVAL;
	current->close_on_exec &= ~(1<<fd);
	f=0+file_table;
	for (i=0 ; i<NR_FILE ; i++,f++)   
		if (!f->f_count) break;    // 搜索file_table中的空闲文件结构项
	if (i>=NR_FILE)
		return -EINVAL;
	(current->filp[fd]=f)->f_count++;    // 让进程的对应文件句柄的文件结构指针指向搜索到的文件结构，并令句柄引用计数递增 1
	if ((i=open_namei(filename,flag,mode,&inode))<0) {    // 获取文件i节点
		current->filp[fd]=NULL;
		f->f_count=0;
		return i;
	}
/* ttys are somewhat special (ttyxx major==4, tty major==5) */
	if (S_ISCHR(inode->i_mode))
		if (MAJOR(inode->i_zone[0])==4) {
			if (current->leader && current->tty<0) {
				current->tty = MINOR(inode->i_zone[0]);
				tty_table[current->tty].pgrp = current->pgrp;
			}
		} else if (MAJOR(inode->i_zone[0])==5)
			if (current->tty<0) {
				iput(inode);
				current->filp[fd]=NULL;
				f->f_count=0;
				return -EPERM;
			}
/* Likewise with block-devices: check for floppy_change */
	if (S_ISBLK(inode->i_mode))
		check_disk_change(inode->i_zone[0]);
	f->f_mode = inode->i_mode;
	f->f_flags = flag;
	f->f_count = 1;
	f->f_inode = inode;
	f->f_pos = 0;
	return (fd);
}

// 创建文件
int sys_creat(const char * pathname, int mode)
{
	return sys_open(pathname, O_CREAT | O_TRUNC, mode);
}

// 关闭文件
int sys_close(unsigned int fd)
{	
	struct file * filp;

	if (fd >= NR_OPEN)
		return -EINVAL;
	current->close_on_exec &= ~(1<<fd);
	if (!(filp = current->filp[fd]))
		return -EINVAL;
	current->filp[fd] = NULL;
	if (filp->f_count == 0)
		panic("Close: file count is 0");
	if (--filp->f_count)
		return (0);
	iput(filp->f_inode);
	return (0);
}
