/*
 *  linux/kernel/blk_dev/ll_rw.c
 *
 * (C) 1991 Linus Torvalds
 */

/*
 * This handles all read/write requests to block devices
 */
/* 
 * 该程序处理块设备的所有读/写操作。 
 */
#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#include "blk.h"

/*
 * The request-struct contains all necessary data
 * to load a nr of sectors into memory
 */
struct request request[NR_REQUEST];

/*
 * used to wait on when there are no free requests
 */
struct task_struct * wait_for_request = NULL;

/* blk_dev_struct is:
 *	do_request-address
 *	next-request
 */
struct blk_dev_struct blk_dev[NR_BLK_DEV] = {
	{ NULL, NULL },		/* no_dev */          // 0 - 无设备。
	{ NULL, NULL },		/* dev mem */         // 1 - 内存
	{ NULL, NULL },		/* dev fd */          // 2 - 软驱设备
	{ NULL, NULL },		/* dev hd */          // 3 - 硬盘设备
	{ NULL, NULL },		/* dev ttyx */        // 4 - ttyx 设备
	{ NULL, NULL },		/* dev tty */         // 5 - tty 设备
	{ NULL, NULL }		/* dev lp */          // 6 - lp 打印机设备
};

// 锁定指定的缓冲区 bh。如果指定的缓冲区已经被其它任务锁定，则使自己睡眠（不可中断地等待）， 
// 直到被执行解锁缓冲区的任务明确地唤醒。
static inline void lock_buffer(struct buffer_head * bh)
{
	cli();
	while (bh->b_lock)              // 如果缓冲区已被锁定，则睡眠，直到缓冲区解锁，然后马上锁定之
		sleep_on(&bh->b_wait); 
	bh->b_lock=1;
	sti();
}

// 解锁缓冲区头bh
static inline void unlock_buffer(struct buffer_head * bh)
{
	if (!bh->b_lock)
		printk("ll_rw_block.c: buffer not locked\n\r");
	bh->b_lock = 0;
	wake_up(&bh->b_wait);		// 唤醒等待该缓冲区的任务
}

/*
 * add-request adds a request to the linked list.
 * It disables interrupts so that it can muck with the
 * request-lists in peace.
 */

/**
 * @brief  向链表中加入请求项
 * 
 * @param dev 指定的设备，一般为硬盘
 * @param req 指定的请求项
 */
static void add_request(struct blk_dev_struct * dev, struct request * req)    
{
	struct request * tmp;

	req->next = NULL;
	cli();
	if (req->bh)
		req->bh->b_dirt = 0;
	if (!(tmp = dev->current_request)) {	// 如果 dev 的当前请求(current_request)子段为空，则表示目前该设备没有请求项，本次是第 1 个请求项
		dev->current_request = req;
		sti();
		(dev->request_fn)();		// 真正的读写请求发生位置。如果是第一个请求项，则直接执行
		return;
	}
	// 如果目前该设备已经有请求项在等待，则首先利用电梯算法搜索最佳位置，然后将当前请求插入请求链表中； 请求函数在时钟中断时执行
	for ( ; tmp->next ; tmp=tmp->next)
		if ((IN_ORDER(tmp,req) ||
		    !IN_ORDER(tmp,tmp->next)) &&
		    IN_ORDER(req,tmp->next))
			break;
	req->next=tmp->next;
	tmp->next=req;
	sti();
}


/**
 * @brief 创建请求项并插入请求队列
 * 
 * @param major 主设备号
 * @param rw 命令（读/写）
 * @param bh 存放数据的缓冲区头指针
 */
static void make_request(int major,int rw, struct buffer_head * bh)
{
	struct request * req;
	int rw_ahead;

/* WRITEA/READA is special case - it is not really needed, so if the */
/* buffer is locked, we just forget about it, else it's a normal read */
	if (rw_ahead = (rw == READA || rw == WRITEA)) {
		if (bh->b_lock)
			return;
		if (rw == READA)
			rw = READ;
		else
			rw = WRITE;
	}
	if (rw!=READ && rw!=WRITE)
		panic("Bad block dev command, must be R/W/RA/WA");
	lock_buffer(bh);
	if ((rw == WRITE && !bh->b_dirt) || (rw == READ && bh->b_uptodate)) {
		unlock_buffer(bh);
		return;
	}
repeat:
/* we don't allow the write-requests to fill up the queue completely:
 * we want some room for reads: they take precedence. The last third
 * of the requests are only for reads.
 */
/* 我们不能让队列中全都是写请求项：我们需要为读请求保留一些空间：读操作 
 * 是优先的。请求队列的后三分之一空间是为读准备的。 
 */
	if (rw == READ)
		req = request+NR_REQUEST;          // 对于读请求，将队列指针指向队列尾部
	else
		req = request+((NR_REQUEST*2)/3);    // 对于写请求，队列指针指向队列 2/3 处
/* find an empty request */
	while (--req >= request)
		if (req->dev<0)
			break;
/* if none found, sleep on new requests: check for rw_ahead */
	if (req < request) {
		if (rw_ahead) {
			unlock_buffer(bh);
			return;
		}
		sleep_on(&wait_for_request);
		goto repeat;
	}
/* fill up the request-info, and add it to the queue */
	req->dev = bh->b_dev;
	req->cmd = rw;
	req->errors=0;
	req->sector = bh->b_blocknr<<1;
	req->nr_sectors = 2;
	req->buffer = bh->b_data;
	req->waiting = NULL;
	req->bh = bh;
	req->next = NULL;
	add_request(major+blk_dev,req);
}


// 该函数主要是在 fs/buffer.c 中被调用。实际的读写操作是由设备的 request_fn()函数完成。 
// 对于硬盘操作，该函数是 do_hd_request()
// 软盘			do_fd_request
// 虚拟盘		do_rd_request()
/**
 * @brief 低层读写数据块函数
 * 
 * @param rw 命令（读/写）
 * @param bh 存放数据的缓冲区头指针
 */
void ll_rw_block(int rw, struct buffer_head * bh)
{
	unsigned int major;

	if ((major=MAJOR(bh->b_dev)) >= NR_BLK_DEV ||
	!(blk_dev[major].request_fn)) {
		printk("Trying to read nonexistent block-device\n\r");
		return;
	}
	make_request(major,rw,bh);
}

// 初始化读盘请求，32个元素，未来可能会串起来
void blk_dev_init(void)
{
	int i;

	for (i=0 ; i<NR_REQUEST ; i++) {
		request[i].dev = -1;
		request[i].next = NULL;
	}
}
