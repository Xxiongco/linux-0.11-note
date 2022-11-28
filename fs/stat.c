/*
 *  linux/fs/stat.c
 *
 *  (C) 1991  Linus Torvalds
 */
// 该程序实现取文件状态信息系统调用函数 stat()和 fstat()，并将信息存放在用户的文件状态结构缓
// 冲区中。stat()是利用文件名取信息，而 fstat()是使用文件句柄(描述符)来取信息。

#include <errno.h>
#include <sys/stat.h>

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

/**
 * @brief 复制文件状态信息，放入statbuf
 * 
 * @param inode 文件对应的i节点
 * @param statbuf stat文件状态结构指针，用于存放取得的状态信息
 */
static void cp_stat(struct m_inode * inode, struct stat * statbuf)
{
	struct stat tmp;
	int i;

	verify_area(statbuf,sizeof (* statbuf));
	tmp.st_dev = inode->i_dev;
	tmp.st_ino = inode->i_num;
	tmp.st_mode = inode->i_mode;
	tmp.st_nlink = inode->i_nlinks;
	tmp.st_uid = inode->i_uid;
	tmp.st_gid = inode->i_gid;
	tmp.st_rdev = inode->i_zone[0];
	tmp.st_size = inode->i_size;
	tmp.st_atime = inode->i_atime;
	tmp.st_mtime = inode->i_mtime;
	tmp.st_ctime = inode->i_ctime;
	for (i=0 ; i<sizeof (tmp) ; i++)
		put_fs_byte(((char *) &tmp)[i],&((char *) statbuf)[i]);
}

// 根据文件名获取文件状态信息
int sys_stat(char * filename, struct stat * statbuf)
{
	struct m_inode * inode;

	if (!(inode=namei(filename)))		// 首先根据文件名找出对应的 i 节点
		return -ENOENT;
	cp_stat(inode,statbuf);
	iput(inode);
	return 0;
}

//  根据文件句柄获取文件状态信息； 文件句柄可以直接定位到inode
int sys_fstat(unsigned int fd, struct stat * statbuf)
{
	struct file * f;
	struct m_inode * inode;

	if (fd >= NR_OPEN || !(f=current->filp[fd]) || !(inode=f->f_inode))
		return -EBADF;
	cp_stat(inode,statbuf);
	return 0;
}
