/*
 *  linux/kernel/tty_io.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'tty_io.c' gives an orthogonal (正交的) feeling to tty's, be they consoles
 * or rs-channels. It also implements echoing, cooked mode etc.
 *
 * Kill-line thanks to John T Kohl.
 */
#include <ctype.h>
#include <errno.h>
#include <signal.h>

#define ALRMMASK (1<<(SIGALRM-1))
#define KILLMASK (1<<(SIGKILL-1))
#define INTMASK (1<<(SIGINT-1))
#define QUITMASK (1<<(SIGQUIT-1))
#define TSTPMASK (1<<(SIGTSTP-1))

#include <linux/sched.h>
#include <linux/tty.h>
#include <asm/segment.h>
#include <asm/system.h>

#define _L_FLAG(tty,f)	((tty)->termios.c_lflag & f)
#define _I_FLAG(tty,f)	((tty)->termios.c_iflag & f)
#define _O_FLAG(tty,f)	((tty)->termios.c_oflag & f)

#define L_CANON(tty)	_L_FLAG((tty),ICANON)
#define L_ISIG(tty)	_L_FLAG((tty),ISIG)
#define L_ECHO(tty)	_L_FLAG((tty),ECHO)
#define L_ECHOE(tty)	_L_FLAG((tty),ECHOE)
#define L_ECHOK(tty)	_L_FLAG((tty),ECHOK)
#define L_ECHOCTL(tty)	_L_FLAG((tty),ECHOCTL)
#define L_ECHOKE(tty)	_L_FLAG((tty),ECHOKE)

#define I_UCLC(tty)	_I_FLAG((tty),IUCLC)
#define I_NLCR(tty)	_I_FLAG((tty),INLCR)
#define I_CRNL(tty)	_I_FLAG((tty),ICRNL)
#define I_NOCR(tty)	_I_FLAG((tty),IGNCR)

#define O_POST(tty)	_O_FLAG((tty),OPOST)
#define O_NLCR(tty)	_O_FLAG((tty),ONLCR)
#define O_CRNL(tty)	_O_FLAG((tty),OCRNL)
#define O_NLRET(tty)	_O_FLAG((tty),ONLRET)
#define O_LCUC(tty)	_O_FLAG((tty),OLCUC)

 // tty 数据结构的 tty_table 数组。其中包含三个初始化项数据，分别对应控制台、串口终端 1 和 
 // 串口终端 2 的初始化数据。
struct tty_struct tty_table[] = {
	{
		{ICRNL,		/* change incoming CR to NL */       	// 将输入的CR转换为NL
		OPOST|ONLCR,	/* change outgoing NL to CRNL */	// 将输出的NL转换为CRNL
		0,													// 控制模式标志初始化为0
		ISIG | ICANON | ECHO | ECHOCTL | ECHOKE,			// 本地模式标志
		0,		/* console termio */						// 控制台termio， 不用线路规程
		INIT_C_CC},											// 控制字符数组
		0,			/* initial pgrp */							// 所属初始进程组
		0,			/* initial stopped */						// 初始停止标志
		con_write,												// tty写函数指针
		{0,0,0,0,""},		/* console read-queue */			// tty控制台读队列
		{0,0,0,0,""},		/* console write-queue */			// tty控制台写队列
		{0,0,0,0,""}		/* console secondary queue */		// tty控制台辅助（第二）队列
	},{
		{0, /* no translation */
		0,  /* no translation */
		B2400 | CS8,
		0,
		0,
		INIT_C_CC},
		0,
		0,
		rs_write,
		{0x3f8,0,0,0,""},		/* rs 1 */
		{0x3f8,0,0,0,""},
		{0,0,0,0,""}
	},{
		{0, /* no translation */
		0,  /* no translation */
		B2400 | CS8,
		0,
		0,
		INIT_C_CC},
		0,
		0,
		rs_write,
		{0x2f8,0,0,0,""},		/* rs 2 */
		{0x2f8,0,0,0,""},
		{0,0,0,0,""}
	}
};

/*
 * these are the tables used by the machine code handlers.
 * you can implement pseudo-tty's or something by changing
 * them. Currently not done.
 */
// tty 缓冲队列地址表。rs_io.s 汇编程序使用，用于取得读写缓冲队列地址。
struct tty_queue * table_list[]={
	&tty_table[0].read_q, &tty_table[0].write_q,
	&tty_table[1].read_q, &tty_table[1].write_q,
	&tty_table[2].read_q, &tty_table[2].write_q
	};

// 初始化终端
void tty_init(void)
{
	rs_init();          // 初始化串行中断程序和串行接口 1 和 2。(serial.c, 37)
	con_init();         // 初始化控制台终端。(console.c, 617)
}

//// tty 键盘终端字符处理函数。 
 // 参数：tty - 相应 tty 终端结构指针；mask - 信号屏蔽位。
//  主要是设置信号屏蔽位
void tty_intr(struct tty_struct * tty, int mask)
{
	int i;

	if (tty->pgrp <= 0)
		return;
	for (i=0;i<NR_TASKS;i++)
		if (task[i] && task[i]->pgrp==tty->pgrp)
			task[i]->signal |= mask;
}

/// 如果队列缓冲区空则让进程进入可中断的睡眠状态
static void sleep_if_empty(struct tty_queue * queue)
{
	cli();
	while (!current->signal && EMPTY(*queue))
		interruptible_sleep_on(&queue->proc_list);
	sti();
}

// 若队列缓冲区满则让进程进入可中断的睡眠状态
static void sleep_if_full(struct tty_queue * queue)
{
	if (!FULL(*queue))
		return;
	cli();
	while (!current->signal && LEFT(*queue)<128)
		interruptible_sleep_on(&queue->proc_list);
	sti();
}

//// 等待按键。 
 // 如果控制台的读队列缓冲区空则让进程进入可中断的睡眠状态。
void wait_for_keypress(void)
{
	sleep_if_empty(&tty_table[0].secondary);
}

// 复制成规范模式字符序列：回车换行大小写转化，正确处理键盘信号
// 参数：tty - 指定终端的 tty 结构。
void copy_to_cooked(struct tty_struct * tty)
{
	signed char c;

	while (!EMPTY(tty->read_q) && !FULL(tty->secondary)) {
		GETCH(tty->read_q,c);
		//回车转换行
		if (c==13)
			if (I_CRNL(tty))
				c=10;
			else if (I_NOCR(tty))
				continue;
			else ;
		//换行转回车	
		else if (c==10 && I_NLCR(tty))
			c=13;

		//转小写	
		if (I_UCLC(tty))
			c=tolower(c);
			
		if (L_CANON(tty)) {
			// ctrl+c
			if (c==KILL_CHAR(tty)) {
				/* deal with killing the input line */
				while(!(EMPTY(tty->secondary) ||
				        (c=LAST(tty->secondary))==10 ||
				        c==EOF_CHAR(tty))) {
					if (L_ECHO(tty)) {
						if (c<32)
							PUTCH(127,tty->write_q);
						PUTCH(127,tty->write_q);
						tty->write(tty);
					}
					DEC(tty->secondary.head);
				}
				continue;
			}
			//辅助队列不空或者最后是 换行/结束字符（EOF），往写队列加入擦除字符（127），辅助队列后退1B
			if (c==ERASE_CHAR(tty)) {
				if (EMPTY(tty->secondary) ||
				   (c=LAST(tty->secondary))==10 ||
				   c==EOF_CHAR(tty))
					continue;
				if (L_ECHO(tty)) {
					if (c<32)
						PUTCH(127,tty->write_q);
					PUTCH(127,tty->write_q);
					tty->write(tty);
				}
				DEC(tty->secondary.head);
				continue;
			}
			//停止字符
			if (c==STOP_CHAR(tty)) {
				tty->stopped=1;
				continue;
			}
			//开始字符
			if (c==START_CHAR(tty)) {
				tty->stopped=0;
				continue;
			}
		}
		//为进程产生信号
		if (L_ISIG(tty)) {
			//键盘中断信号
			if (c==INTR_CHAR(tty)) {
				tty_intr(tty,INTMASK);
				continue;
			}
			//键盘退出信号
			if (c==QUIT_CHAR(tty)) {
				tty_intr(tty,QUITMASK);
				continue;
			}
		}
		//换行符或文件结束符
		if (c==10 || c==EOF_CHAR(tty))
			tty->secondary.data++;
		//本地模式标志集中回显标志 ECHO 置位，特殊字符也会显示	
		if (L_ECHO(tty)) {
			if (c==10) {
				PUTCH(10,tty->write_q);
				PUTCH(13,tty->write_q);
			} else if (c<32) {
				if (L_ECHOCTL(tty)) {
					PUTCH('^',tty->write_q);
					PUTCH(c+64,tty->write_q);
				}
			} else
				PUTCH(c,tty->write_q);
			tty->write(tty);
		}
		PUTCH(c,tty->secondary);
	}
	wake_up(&tty->secondary.proc_list);
}


 /**
  * @brief 从指定的设备中读取指定长度的字节至指定的缓冲区中
  * 
  * @param channel 指定的子设备号； 本版本 linux 内核的终端只有 3 个子设备，分别是控制台(0)、串口终端 1(1)和串口终端 2(2)。 
  * @param buf 目标缓冲区
  * @param nr 欲读取的字节数
  * @return int 已经读取的字节数
  */
int tty_read(unsigned channel, char * buf, int nr)
{
	struct tty_struct * tty;
	char c, * b=buf;
	int minimum,time,flag=0;
	long oldalarm;


 // 所以任何大于 2 的子设备号都是非法的。写的字节数当然也不能小于 0 的。
	if (channel>2 || nr<0) return -1;
	tty = &tty_table[channel];
	//// 首先取进程中的(报警)定时值(滴答数)。
	oldalarm = current->alarm;
	// 并设置读操作超时定时值 time 和需要最少读取的字符个数 minimum。
	time = 10L*tty->termios.c_cc[VTIME];
	minimum = tty->termios.c_cc[VMIN];
	if (time && !minimum) {
		minimum=1;
		if (flag=(!oldalarm || time+jiffies<oldalarm))
			current->alarm = time+jiffies;
	}
	if (minimum>nr)
		minimum=nr;
	while (nr>0) {
		if (flag && (current->signal & ALRMMASK)) {
			current->signal &= ~ALRMMASK;
			break;
		}
		if (current->signal)
			break;
		if (EMPTY(tty->secondary) || (L_CANON(tty) &&
		!tty->secondary.data && LEFT(tty->secondary)>20)) {
			sleep_if_empty(&tty->secondary);
			continue;
		}
		do {
			GETCH(tty->secondary,c);      //============key========= 放入辅助队列
			if (c==EOF_CHAR(tty) || c==10)
				tty->secondary.data--;
			if (c==EOF_CHAR(tty) && L_CANON(tty))
				return (b-buf);
			else {
				put_fs_byte(c,b++);      //============key========= 放入buffer
				if (!--nr)
					break;
			}
		} while (nr>0 && !EMPTY(tty->secondary));
		if (time && !L_CANON(tty))
			if (flag=(!oldalarm || time+jiffies<oldalarm))
				current->alarm = time+jiffies;
			else
				current->alarm = oldalarm;
		if (L_CANON(tty)) {
			if (b-buf)
				break;
		} else if (b-buf >= minimum)
			break;
	}
	current->alarm = oldalarm;
	if (current->signal && !(b-buf))
		return -EINTR;
	return (b-buf);                 // 返回已读取的字符数
}


/**
 * @brief 向指定的设备写入字符
 * 
 * @param channel 指定的子设备号
 * @param buf 字符来源缓冲区
 * @param nr 待写入的字节数
 * @return int 已经写入的字节数
 */
int tty_write(unsigned channel, char * buf, int nr)
{
	static cr_flag=0;
	struct tty_struct * tty;
	char c, *b=buf;

	if (channel>2 || nr<0) return -1;
	tty = channel + tty_table;
	while (nr>0) {
		sleep_if_full(&tty->write_q);
		if (current->signal)		// 如果当前进程有信号要处理，则退出
			break;
		while (nr>0 && !FULL(tty->write_q)) {
			c=get_fs_byte(b);		//============key========= 从缓冲区得到字符
			if (O_POST(tty)) {
				if (c=='\r' && O_CRNL(tty))
					c='\n';
				else if (c=='\n' && O_NLRET(tty))
					c='\r';
				if (c=='\n' && !cr_flag && O_NLCR(tty)) {
					cr_flag = 1;
					PUTCH(13,tty->write_q);
					continue;
				}
				if (O_LCUC(tty))
					c=toupper(c);
			}
			b++; nr--;
			cr_flag = 0;
			PUTCH(c,tty->write_q);     //============key========= 将字符写入队列
		}
		// 若字节全部写完，或者写队列已满，则程序执行到这里。调用对应 tty 的写函数，若还有字节要写， 
 		// 则等待写队列不满，所以调用调度程序，先去执行其它任务
		tty->write(tty);				 //============key========= 将字符写入tty
		if (nr>0)
			schedule();
	}
	return (b-buf);
}

/*
 * Jeh, sometimes I really like the 386.
 * This routine is called from an interrupt,
 * and there should be absolutely no problem
 * with sleeping even in an interrupt (I hope).
 * Of course, if somebody proves me wrong, I'll
 * hate intel for all time :-). We'll have to
 * be careful and see to reinstating the interrupt
 * chips before calling this, though.
 *
 * I don't think we sleep here under normal circumstances
 * anyway, which is good, as the task sleeping might be
 * totally innocent.
 */
// 在串口读字符中断(rs_io.s, 109)和键盘中断(kerboard.S, 69)中调用。
//// tty 中断处理调用函数 - 执行 tty 中断处理。 
 // 参数：tty - 指定的 tty 终端号（0，1 或 2）。 
 // 将指定 tty 终端队列缓冲区中的字符复制成规范(熟)模式字符并存放在辅助队列(规范模式队列)中。
void do_tty_interrupt(int tty)
{
	copy_to_cooked(tty_table+tty);
}

void chr_dev_init(void)
{
}
