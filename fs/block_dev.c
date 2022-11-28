/*
 *  linux/fs/block_dev.c
 *
 *  (C) 1991  Linus Torvalds
 */
// 该文件包括 block_read()和 block_write()两个函数。这两个函数是供系统调用函数 read()和 write()
// 调用的，其它地方没有引用。
// 块设备读写操作所使用的函数之间的层次关系为： 
// • read()，write() 
// • block_read()，block_write()，file_read()，file_write()，read_pipe()，write() 
// • bread()或 breada() 
// • getblk() 
// • ll_rw_block()

#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <asm/system.h>

/**
 * @brief 数据块写函数 - 向指定设备从给定偏移处写入指定字节数据
 * 对于内核来说，写操作是向高速缓冲区中写入数据，什么时候数据最终写入设备是由高速缓冲管理
 * 程序决定并处理的。另外，因为设备是以块为单位进行读写的，因此对于写开始位置不处于块起始
 * 处时，需要先将开始字节所在的整个块读出，然后将需要写的数据从写开始处填写满该块，再将完
 * 整的一块数据写盘（即交由高速缓冲程序去处理）
 * 
 * @param dev 设备号
 * @param pos 偏移量
 * @param buf 待写入字节数组
 * @param count 写入字节数量
 * @return int 成功写入字节数量
 */
int block_write(int dev, long * pos, char * buf, int count)
{
	int block = *pos >> BLOCK_SIZE_BITS;   // 由 pos 地址换算成开始读写块的块序号 block。并求出需读第 1 字节在该块中的偏移位置 offset
	int offset = *pos & (BLOCK_SIZE-1);
	int chars;
	int written = 0;
	struct buffer_head * bh;
	register char * p;

	while (count>0) {
		chars = BLOCK_SIZE - offset;    // 计算在该块中可写入的字节数。如果需要写入的字节数填不满一块，则只需写 count 字节
		if (chars > count)
			chars=count;
		if (chars == BLOCK_SIZE)	// 如果正好要写 1 块数据，则直接申请 1 块高速缓冲块，否则需要读入将被修改的数据块，并预读下两块数据
			bh = getblk(dev,block);
		else
			bh = breada(dev,block,block+1,block+2,-1);
		block++;
		if (!bh)
			return written?written:-EIO;
		p = offset + bh->b_data;		// p为写指针
		offset = 0;
		*pos += chars;
		written += chars;
		count -= chars;
		while (chars-->0)
			*(p++) = get_fs_byte(buf++);
		bh->b_dirt = 1;
		brelse(bh);
	}
	return written;
}

/**
 * @brief 从指定设备的指定位置读入指定字节数的数据到buf
 * 
 * @param dev 设备号
 * @param pos 位置
 * @param buf 数据目的缓冲区
 * @param count 字节数
 * @return int 成功读取字节数
 */
int block_read(int dev, unsigned long * pos, char * buf, int count)
{
	int block = *pos >> BLOCK_SIZE_BITS;
	int offset = *pos & (BLOCK_SIZE-1);
	int chars;
	int read = 0;
	struct buffer_head * bh;
	register char * p;

	while (count>0) {
		chars = BLOCK_SIZE-offset;
		if (chars > count)
			chars = count;
		if (!(bh = breada(dev,block,block+1,block+2,-1)))
			return read?read:-EIO;
		block++;
		p = offset + bh->b_data;
		offset = 0;
		*pos += chars;
		read += chars;
		count -= chars;
		while (chars-->0)
			put_fs_byte(*(p++),buf++);
		brelse(bh);
	}
	return read;
}
