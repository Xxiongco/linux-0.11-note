#ifndef _BLK_H
#define _BLK_H

#define NR_BLK_DEV	7		// 块设备的数量
/*
 * NR_REQUEST is the number of entries in the request-queue.
 * NOTE that writes may use only the low 2/3 of these: reads
 * take precedence.
 *
 * 32 seems to be a reasonable number: enough to get some benefit
 * from the elevator-mechanism, but not so much as to lock a lot of
 * buffers when they are in the queue. 64 seems to be too many (easily
 * long pauses in reading when heavy writing/syncing is going on)
 */
#define NR_REQUEST	32		// 请求（读硬盘）数量

/*
 * Ok, this is an expanded form so that we can use the same
 * request for paging requests when that is implemented. In
 * paging, 'bh' is NULL, and 'waiting' is used to wait for
 * read/write completion.
 */
struct request {
	int dev;						/* -1 if no request, -1 就表示空闲*/
	int cmd;						/* READ or WRITE */
	int errors;     				//表示操作时产生的错误次数
	unsigned long sector;      		//表示起始扇区
	unsigned long nr_sectors;   	//扇区数
	char * buffer;              	//表示数据缓冲区，也就是读盘之后的数据放在内存中的什么位置
	struct task_struct * waiting;   // task_struct 结构，这可以表示一个进程，也就表示是哪个进程发起了这个请求
	struct buffer_head * bh;        //buffer header  缓冲区头指针
	struct request * next;			// 指向下一项请求
};

/*
 * This is used in the elevator algorithm: Note that
 * reads always go before writes. This is natural: reads
 * are much more time-critical than writes.
  
  扫描算法（SCAN), 单向运行，到头翻转
  */
#define IN_ORDER(s1,s2) \
((s1)->cmd<(s2)->cmd || (s1)->cmd==(s2)->cmd && \
((s1)->dev < (s2)->dev || ((s1)->dev == (s2)->dev && \
(s1)->sector < (s2)->sector)))

// 块设备结构。
struct blk_dev_struct {
	void (*request_fn)(void);			// 请求操作的函数指针，可以使硬盘，软盘或者其他
	struct request * current_request;	// 请求信息结构
};

extern struct blk_dev_struct blk_dev[NR_BLK_DEV];		// 块设备数组，每种块设备占用一项
extern struct request request[NR_REQUEST];				// 请求队列数组
extern struct task_struct * wait_for_request;			// 等待请求的任务结构

#ifdef MAJOR_NR

/*
 * Add entries as needed. Currently the only block devices
 * supported are hard-disks and floppies.
 */

#if (MAJOR_NR == 1)    /* RAM 盘（内存虚拟盘）*/
/* ram disk */
#define DEVICE_NAME "ramdisk"
#define DEVICE_REQUEST do_rd_request            // 设备请求函数 do_rd_request()
#define DEVICE_NR(device) ((device) & 7)        // 设备号(0--7)
#define DEVICE_ON(device)                       // 开启设备。虚拟盘无须开启和关闭。
#define DEVICE_OFF(device)						// 关闭设备

#elif (MAJOR_NR == 2)	// 软驱的主设备号是 2
/* floppy */
#define DEVICE_NAME "floppy"
#define DEVICE_INTR do_floppy
#define DEVICE_REQUEST do_fd_request
#define DEVICE_NR(device) ((device) & 3)
#define DEVICE_ON(device) floppy_on(DEVICE_NR(device))
#define DEVICE_OFF(device) floppy_off(DEVICE_NR(device))

#elif (MAJOR_NR == 3)      // 硬盘主设备号是 3
/* harddisk */
#define DEVICE_NAME "harddisk"
#define DEVICE_INTR do_hd                         // 设备中断处理程序 do_hd()。
#define DEVICE_REQUEST do_hd_request
#define DEVICE_NR(device) (MINOR(device)/5)      // 设备号（0--1）。每个硬盘可以有 4 个分区
#define DEVICE_ON(device)                        // 硬盘一直在工作，无须开启和关闭
#define DEVICE_OFF(device)

#elif
/* unknown blk device */
#error "unknown blk device"

#endif

#define CURRENT (blk_dev[MAJOR_NR].current_request)	 	// CURRENT 为指定主设备号的当前请求结构
#define CURRENT_DEV DEVICE_NR(CURRENT->dev)				// CURRENT_DEV 为 CURRENT 的设备号

#ifdef DEVICE_INTR
void (*DEVICE_INTR)(void) = NULL;
#endif
static void (DEVICE_REQUEST)(void);

// 释放锁定的缓冲区
extern inline void unlock_buffer(struct buffer_head * bh)
{
	if (!bh->b_lock)
		printk(DEVICE_NAME ": free buffer being unlocked\n");
	bh->b_lock=0;                 // 否则将该缓冲区解锁
	wake_up(&bh->b_wait);         // 唤醒等待该缓冲区的进程
}

// 结束请求
extern inline void end_request(int uptodate)
{
	DEVICE_OFF(CURRENT->dev);
	if (CURRENT->bh) {
		CURRENT->bh->b_uptodate = uptodate;		// 置更新标志
		unlock_buffer(CURRENT->bh);
	}
	if (!uptodate) {
		printk(DEVICE_NAME " I/O error\n\r");
		printk("dev %04x, block %d\n\r",CURRENT->dev,
			CURRENT->bh->b_blocknr);
	}
	wake_up(&CURRENT->waiting);
	wake_up(&wait_for_request);
	CURRENT->dev = -1;                 //no free???????????
	CURRENT = CURRENT->next;
}

// 检查指针，设备号，是否锁定
#define INIT_REQUEST \
repeat: \
	if (!CURRENT) \
		return; \
	if (MAJOR(CURRENT->dev) != MAJOR_NR) \
		panic(DEVICE_NAME ": request list destroyed"); \
	if (CURRENT->bh) { \
		if (!CURRENT->bh->b_lock) \
			panic(DEVICE_NAME ": block not locked"); \
	}
// 如果在进行请求操作时缓冲区没锁定则死机

#endif

#endif
