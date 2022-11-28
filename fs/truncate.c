/*
 *  linux/fs/truncate.c
 *
 *  (C) 1991  Linus Torvalds
 */
// 本程序用于释放指定 i 节点在设备上占用的所有逻辑块，包括直接块、一次间接块和二次间接块。

#include <linux/sched.h>

#include <sys/stat.h>

// 释放一次间接块
static void free_ind(int dev,int block)
{
	struct buffer_head * bh;
	unsigned short * p;
	int i;

	if (!block)
		return;
	if (bh=bread(dev,block)) {
		p = (unsigned short *) bh->b_data;
		for (i=0;i<512;i++,p++)
			if (*p)
				free_block(dev,*p);		// === key ===
		brelse(bh);
	}
	free_block(dev,block);
}

// 释放二次间接块
static void free_dind(int dev,int block)
{
	struct buffer_head * bh;
	unsigned short * p;
	int i;

	if (!block)
		return;
	if (bh=bread(dev,block)) {
		p = (unsigned short *) bh->b_data;
		for (i=0;i<512;i++,p++)
			if (*p)
				free_ind(dev,*p);		// === key ===
		brelse(bh);
	}
	free_block(dev,block);
}

//// 将节点对应的文件长度截为 0，并释放占用的设备空间。
void truncate(struct m_inode * inode)
{
	int i;

	if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
		return;
	for (i=0;i<7;i++)		// 释放 i 节点的 7 个直接逻辑块，并将这 7 个逻辑块项全置零
		if (inode->i_zone[i]) {
			free_block(inode->i_dev,inode->i_zone[i]);
			inode->i_zone[i]=0;
		}
	free_ind(inode->i_dev,inode->i_zone[7]);		// 释放一次间接块
	free_dind(inode->i_dev,inode->i_zone[8]);		// 释放二次间接块
	inode->i_zone[7] = inode->i_zone[8] = 0;
	inode->i_size = 0;
	inode->i_dirt = 1;
	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
}

