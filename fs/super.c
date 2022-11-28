/*
 *  linux/fs/super.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * super.c contains code to handle the super-block tables.
 */
// 处理超级块

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#include <errno.h>
#include <sys/stat.h>

int sync_dev(int dev);				// 对指定设备执行高速缓冲与设备上数据的同步操作
void wait_for_keypress(void);		// 等待击键

/* set_bit uses setb, as gas doesn't recognize setc */
#define set_bit(bitnr,addr) ({ \
register int __res __asm__("ax"); \
__asm__("bt %2,%3;setb %%al":"=a" (__res):"a" (0),"r" (bitnr),"m" (*(addr))); \
__res; })

struct super_block super_block[NR_SUPER];   // 根设备都挂载于此处
/* this is initialized in init/main.c    setup */
int ROOT_DEV = 0;

//// 锁定指定的超级块
static void lock_super(struct super_block * sb)
{
	cli();
	while (sb->s_lock)
		sleep_on(&(sb->s_wait));
	sb->s_lock = 1;
	sti();
}

//// 对指定超级块解锁
static void free_super(struct super_block * sb)
{
	cli();
	sb->s_lock = 0;
	wake_up(&(sb->s_wait));
	sti();
}

//// 睡眠等待超级块解锁
static void wait_on_super(struct super_block * sb)
{
	cli();
	while (sb->s_lock)
		sleep_on(&(sb->s_wait));
	sti();
}

//// 取指定设备的超级块。返回该超级块结构指针
struct super_block * get_super(int dev)
{
	struct super_block * s;

	if (!dev)
		return NULL;
	s = 0+super_block;          // s 指向超级块数组开始处。搜索整个超级块数组，寻找指定设备的超级块
	while (s < NR_SUPER+super_block)
	// 如果当前搜索项是指定设备的超级块，则首先等待该超级块解锁（若已经被其它进程上锁的话）。 
	// 在等待期间，该超级块有可能被其它设备使用，因此此时需再判断一次是否是指定设备的超级块， 
 	// 如果是则返回该超级块的指针。否则就重新对超级块数组再搜索一遍，因此 s 重又指向超级块数组 
 	// 开始处
		if (s->s_dev == dev) {
			wait_on_super(s);
			if (s->s_dev == dev)
				return s;
			s = 0+super_block;
		} else
			s++;
	return NULL;
}

//// 释放指定设备的超级块
void put_super(int dev)
{
	struct super_block * sb;
	struct m_inode * inode;
	int i;

	if (dev == ROOT_DEV) {
		printk("root diskette changed: prepare for armageddon\n\r");
		return;
	}
	if (!(sb = get_super(dev)))
		return;
	if (sb->s_imount) {
		printk("Mounted disk changed - tssk, tssk\n\r");
		return;
	}
	lock_super(sb);
	sb->s_dev = 0;
	for(i=0;i<I_MAP_SLOTS;i++)
		brelse(sb->s_imap[i]);
	for(i=0;i<Z_MAP_SLOTS;i++)
		brelse(sb->s_zmap[i]);
	free_super(sb);
	return;
}

//// 从设备上读取超级块到内存中。 
 // 如果该设备的超级块已经在高速缓冲中并且有效，则直接返回该超级块的指针
static struct super_block * read_super(int dev)
{
	struct super_block * s;
	struct buffer_head * bh;    // 高速缓冲块，保存信息的中间人
	int i,block;

	if (!dev)
		return NULL;
	check_disk_change(dev);
	if (s = get_super(dev))         // 如果该设备的超级块已经在高速缓冲中，则直接返回
		return s;
	for (s = 0+super_block ;; s++) {   // 否则，首先在超级块数组中找出一个空项(也即其 s_dev=0 的项)。如果数组已经占满则返回空指针
		if (s >= NR_SUPER+super_block)
			return NULL;
		if (!s->s_dev)   // 当前dev为空
			break;
	}
	// 找到超级块空项后，就将该超级块用于指定设备，对该超级块进行部分初始化
	s->s_dev = dev;
	s->s_isup = NULL;
	s->s_imount = NULL;
	s->s_time = 0;
	s->s_rd_only = 0;
	s->s_dirt = 0;
	lock_super(s);
	if (!(bh = bread(dev,1))) {          // 从设备上读取超级块信息到 bh 指向的缓冲区中
		s->s_dev=0;
		free_super(s);
		return NULL;
	}
	// 将设备上读取的超级块信息复制到内存超级块结构中。并释放存放读取信息的高速缓冲块
	*((struct d_super_block *) s) =
		*((struct d_super_block *) bh->b_data);
	brelse(bh);
	if (s->s_magic != SUPER_MAGIC) {    // 说明设备上不是正确的文件系统
		s->s_dev = 0;
		free_super(s);
		return NULL;
	}
	// 下面开始读取设备上 i 节点位图和逻辑块位图数据
	for (i=0;i<I_MAP_SLOTS;i++)               // 初始化
		s->s_imap[i] = NULL;
	for (i=0;i<Z_MAP_SLOTS;i++)
		s->s_zmap[i] = NULL;
	block=2;
	for (i=0 ; i < s->s_imap_blocks ; i++)     // 读取
		if (s->s_imap[i]=bread(dev,block))     // 表示inode
			block++;
		else
			break;
	for (i=0 ; i < s->s_zmap_blocks ; i++)
		if (s->s_zmap[i]=bread(dev,block))      // 表示逻辑块
			block++;
		else
			break;
	if (block != 2+s->s_imap_blocks+s->s_zmap_blocks) {
		for(i=0;i<I_MAP_SLOTS;i++)
			brelse(s->s_imap[i]);
		for(i=0;i<Z_MAP_SLOTS;i++)
			brelse(s->s_zmap[i]);
		s->s_dev=0;
		free_super(s);
		return NULL;
	}
	// 一切成功。对于申请空闲 i 节点的函数来讲，如果设备上所有的 i 节点已经全被使用，则查找 
    // 函数会返回 0 值。因此 0 号 i 节点是不能用的，所以这里将位图中的最低位设置为 1，以防止文件 
    // 系统分配 0 号 i 节点。同样的道理，也将逻辑块位图的最低位设置为 1
	s->s_imap[0]->b_data[0] |= 1;
	s->s_zmap[0]->b_data[0] |= 1;
	// 解锁该超级块，并返回超级块指针
	free_super(s);
	return s;
}

/**
 * @brief 卸载文件系统调用函数
 * 
 * @param dev_name 设备文件名
 * @return int 0-成功，other-错误号
 */
int sys_umount(char * dev_name)
{
	struct m_inode * inode;
	struct super_block * sb;
	int dev;

	if (!(inode=namei(dev_name)))
		return -ENOENT;
	dev = inode->i_zone[0];
	if (!S_ISBLK(inode->i_mode)) {
		iput(inode);
		return -ENOTBLK;
	}
	iput(inode);
	if (dev==ROOT_DEV)
		return -EBUSY;
	if (!(sb=get_super(dev)) || !(sb->s_imount))
		return -ENOENT;
	if (!sb->s_imount->i_mount)
		printk("Mounted inode has i_mount=0\n");
	for (inode=inode_table+0 ; inode<inode_table+NR_INODE ; inode++)
		if (inode->i_dev==dev && inode->i_count)
				return -EBUSY;
	sb->s_imount->i_mount=0;
	iput(sb->s_imount);
	sb->s_imount = NULL;
	iput(sb->s_isup);
	sb->s_isup = NULL;
	put_super(dev);
	sync_dev(dev);
	return 0;
}


 /**
  * @brief 安装文件系统
  * 
  * @param dev_name 设备文件名
  * @param dir_name 安装到的目录名
  * @param rw_flag 被安装文件的读写标志
  * @return int 0-成功，other-错误号
  */
int sys_mount(char * dev_name, char * dir_name, int rw_flag)
{
	struct m_inode * dev_i, * dir_i;
	struct super_block * sb;
	int dev;

	if (!(dev_i=namei(dev_name)))             // 首先根据设备文件名找到对应的 i 节点，并取其中的设备号
		return -ENOENT;
	dev = dev_i->i_zone[0];                   // 通过i节点，获取设备号
	if (!S_ISBLK(dev_i->i_mode)) {
		iput(dev_i);
		return -EPERM;
	}
	iput(dev_i);                              // 释放刚申请的 i 节点 dev_i
	if (!(dir_i=namei(dir_name)))
		return -ENOENT;
	if (dir_i->i_count != 1 || dir_i->i_num == ROOT_INO) {
		iput(dir_i);
		return -EBUSY;
	}
	if (!S_ISDIR(dir_i->i_mode)) {
		iput(dir_i);
		return -EPERM;
	}
	if (!(sb=read_super(dev))) {               // 通过设备号，读取设备的超级块
		iput(dir_i);
		return -EBUSY;
	}
	if (sb->s_imount) {
		iput(dir_i);
		return -EBUSY;
	}
	if (dir_i->i_mount) {
		iput(dir_i);
		return -EPERM;
	}
	sb->s_imount=dir_i;             // 被安装设备挂载到目录设备节点
	dir_i->i_mount=1;               // 已挂载，已修改
	dir_i->i_dirt=1;		/* NOTE! we don't iput(dir_i) */
	return 0;			/* we do that in umount 在umount 中释放 */
}

// 加载根文件系统  统开机初始化设置时(sys_setup())调用的
void mount_root(void)
{
	int i,free;
	struct super_block * p;
	struct m_inode * mi;

	if (32 != sizeof (struct d_inode))
		panic("bad i-node size");
	for(i=0;i<NR_FILE;i++)               // 初始化文件表数组（共 64 项，也即系统同时只能打开 64 个文件），将所有文件结构中的引用计数设置为0
		file_table[i].f_count=0;
	if (MAJOR(ROOT_DEV) == 2) {          // 如果根文件系统所在设备是软盘的话，就提示“插入根文件系统盘，并按回车键”，并等待按键
		printk("Insert root floppy and press ENTER");
		wait_for_keypress();
	}
	for(p = &super_block[0] ; p < &super_block[NR_SUPER] ; p++) {    // 初始化超级块数组（共 8 项）
		p->s_dev = 0;
		p->s_lock = 0;
		p->s_wait = NULL;
	}
	if (!(p=read_super(ROOT_DEV)))                      // 读根设备上超级块
		panic("Unable to mount root");
	if (!(mi=iget(ROOT_DEV,ROOT_INO)))                  //从设备上读取文件系统的根 i 节点
		panic("Unable to read root i-node");
	mi->i_count += 3 ;	/* NOTE! it is logically used 4 times, not 1 */
	p->s_isup = p->s_imount = mi;                       // 设置i节点挂载
	current->pwd = mi;                                  // 设置当前工作目录和根目录i节点
	current->root = mi;
	// 统计该设备上空闲块数。
	// 首先令 i 等于超级块中的设备逻辑块总数
	// 然后根据位图统计出空闲总块
	// 接着统计设备上空闲i节点数
	free=0;
	i=p->s_nzones;                    
	while (-- i >= 0)
		if (!set_bit(i&8191,p->s_zmap[i>>13]->b_data))
			free++;
	printk("%d/%d free blocks\n\r",free,p->s_nzones);
	free=0;
	i=p->s_ninodes+1;
	while (-- i >= 0)
		if (!set_bit(i&8191,p->s_imap[i>>13]->b_data))
			free++;
	printk("%d/%d free inodes\n\r",free,p->s_ninodes);
}
