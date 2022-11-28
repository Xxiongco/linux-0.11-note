/*
 *  linux/fs/char_dev.c
 *
 *  (C) 1991  Linus Torvalds
 */
// 字符设备读写相关

#include <errno.h>
#include <sys/types.h>

#include <linux/sched.h>
#include <linux/kernel.h>

#include <asm/segment.h>
#include <asm/io.h>

extern int tty_read(unsigned minor,char * buf,int count);
extern int tty_write(unsigned minor,char * buf,int count);

typedef (*crw_ptr)(int rw,unsigned minor,char * buf,int count,off_t * pos);		// 定义字符设备读写函数指针类型

/**
 * @brief 终端读写操作函数
 * 
 * @param rw 读写命令
 * @param minor 终端子设备号
 * @param buf 缓冲区
 * @param count 读写字节数
 * @param pos 读写操作当前指针
 * @return int 实际读写的字节数
 */
static int rw_ttyx(int rw,unsigned minor,char * buf,int count,off_t * pos)
{
	return ((rw==READ)?tty_read(minor,buf,count):
		tty_write(minor,buf,count));
}

//// 终端读写操作函数。 
// 同上 rw_ttyx()，只是增加了对进程是否有控制终端的检测
static int rw_tty(int rw,unsigned minor,char * buf,int count, off_t * pos)
{
	if (current->tty<0)
		return -EPERM;
	return rw_ttyx(rw,current->tty,buf,count,pos);
}

static int rw_ram(int rw,char * buf, int count, off_t *pos)
{
	return -EIO;
}

static int rw_mem(int rw,char * buf, int count, off_t * pos)
{
	return -EIO;
}

static int rw_kmem(int rw,char * buf, int count, off_t * pos)
{
	return -EIO;
}

/**
 * @brief 端口读写操作函数
 * 
 * @param rw 读写命令
 * @param buf 缓冲区
 * @param count 读写字节数
 * @param pos 端口地址
 * @return int 实际读写的字节数
 */
static int rw_port(int rw,char * buf, int count, off_t * pos)
{
	int i=*pos;

	while (count-->0 && i<65536) {
		if (rw==READ)
			put_fs_byte(inb(i),buf++);
		else
			outb(get_fs_byte(buf++),i);
		i++;
	}
	i -= *pos;
	*pos += i;
	return i;
}

/**
 * @brief 内存读写操作函数
 * 
 * @param rw 读写命令
 * @param minor 子设备号
 * @param buf 缓冲区
 * @param count 读写字节数
 * @param pos 读写操作当前指针
 * @return int 实际的读写字节数
 */
static int rw_memory(int rw, unsigned minor, char * buf, int count, off_t * pos)
{
	switch(minor) {
		case 0:
			return rw_ram(rw,buf,count,pos);
		case 1:
			return rw_mem(rw,buf,count,pos);
		case 2:
			return rw_kmem(rw,buf,count,pos);
		case 3:
			return (rw==READ)?0:count;	/* rw_null */
		case 4:
			return rw_port(rw,buf,count,pos);
		default:
			return -EIO;
	}
}

// 定义系统中设备种数。
#define NRDEVS ((sizeof (crw_table))/(sizeof (crw_ptr)))

static crw_ptr crw_table[]={
	NULL,		/* nodev */				/* 无设备(空设备) */
	rw_memory,	/* /dev/mem etc */		/* 内存 */
	NULL,		/* /dev/fd */			/* /dev/fd 软驱 */
	NULL,		/* /dev/hd */			/* /dev/hd 硬盘 */
	rw_ttyx,	/* /dev/ttyx */			/* /dev/ttyx 串口终端1 */
	rw_tty,		/* /dev/tty */			/* /dev/tty 串口终端2 */
	NULL,		/* /dev/lp */			/* /dev/lp 打印机 */
	NULL};		/* unnamed pipes */		/* 未命名管道 */

/**
 * @brief 字符设备读写操作函数
 * 
 * @param rw 读写命令
 * @param dev 设备号
 * @param buf 缓冲区
 * @param count 读写字节数
 * @param pos 读写指针
 * @return int 实际读写字节数
 */
int rw_char(int rw,int dev, char * buf, int count, off_t * pos)
{
	crw_ptr call_addr;

	if (MAJOR(dev)>=NRDEVS)
		return -ENODEV;
	if (!(call_addr=crw_table[MAJOR(dev)]))
		return -ENODEV;
	return call_addr(rw,MINOR(dev),buf,count,pos);
}
