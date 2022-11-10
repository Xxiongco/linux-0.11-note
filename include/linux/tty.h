/*
 * 'tty.h' defines some structures used by tty_io.c and some defines.
 *
 * NOTE! Don't touch this without checking that nothing in rs_io.s or
 * con_io.s breaks. Some constants are hardwired into the system (mainly
 * offsets into 'tty_queue'
 */

// teletype缩写，是由虚拟控制台、串口以及伪终端设备组成的终端设备

#ifndef _TTY_H
#define _TTY_H

#include <termios.h>

#define TTY_BUF_SIZE 1024		// tty 缓冲区大小

// tty 等待队列数据结构
struct tty_queue {
	unsigned long data;
	unsigned long head;
	unsigned long tail;
	struct task_struct * proc_list;
	char buf[TTY_BUF_SIZE];
};

#define INC(a) ((a) = ((a)+1) & (TTY_BUF_SIZE-1))
#define DEC(a) ((a) = ((a)-1) & (TTY_BUF_SIZE-1))
#define EMPTY(a) ((a).head == (a).tail)
#define LEFT(a) (((a).tail-(a).head-1)&(TTY_BUF_SIZE-1))
#define LAST(a) ((a).buf[(TTY_BUF_SIZE-1)&((a).head-1)])
#define FULL(a) (!LEFT(a))
#define CHARS(a) (((a).head-(a).tail)&(TTY_BUF_SIZE-1))
#define GETCH(queue,c) \
(void)({c=(queue).buf[(queue).tail];INC((queue).tail);})
#define PUTCH(c,queue) \
(void)({(queue).buf[(queue).head]=(c);INC((queue).head);})

#define INTR_CHAR(tty) ((tty)->termios.c_cc[VINTR])
#define QUIT_CHAR(tty) ((tty)->termios.c_cc[VQUIT])
#define ERASE_CHAR(tty) ((tty)->termios.c_cc[VERASE])
#define KILL_CHAR(tty) ((tty)->termios.c_cc[VKILL])
#define EOF_CHAR(tty) ((tty)->termios.c_cc[VEOF])
#define START_CHAR(tty) ((tty)->termios.c_cc[VSTART])
#define STOP_CHAR(tty) ((tty)->termios.c_cc[VSTOP])
#define SUSPEND_CHAR(tty) ((tty)->termios.c_cc[VSUSP])

struct tty_struct {
	struct termios termios;   // 终端 io 属性和控制字符数据结构
	int pgrp;                 // 所属进程组。
	int stopped;              // 停止标志
	void (*write)(struct tty_struct * tty);   // tty 写函数指针
	struct tty_queue read_q;                  // tty 读队列
	struct tty_queue write_q;
	struct tty_queue secondary;               // tty 辅助队列(存放规范模式字符序列)
	};

extern struct tty_struct tty_table[];		// tty 结构数组

/*	intr=^C		quit=^|		erase=del	kill=^U
	eof=^D		vtime=\0	vmin=\1		sxtc=\0
	start=^Q	stop=^S		susp=^Z		eol=\0
	reprint=^R	discard=^U	werase=^W	lnext=^V
	eol2=\0
*/
/* 中断 intr=^C 退出 quit=^| 删除 erase=del 终止 kill=^U 
 * 文件结束 eof=^D vtime=\0 vmin=\1 sxtc=\0 
 * 开始 start=^Q 停止 stop=^S 挂起 susp=^Z 行结束 eol=\0 
 * 重显 reprint=^R 丢弃 discard=^U werase=^W lnext=^V 
 * 行结束 eol2=\0 
 */ 
 // 控制字符对应的 ASCII 码值。[8 进制]
#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0"

// 异步串行通信初始化	serial
void rs_init(void);
// 控制终端初始化	console
void con_init(void);
// tty 初始化
void tty_init(void);

int tty_read(unsigned c, char * buf, int n);
int tty_write(unsigned c, char * buf, int n);

void rs_write(struct tty_struct * tty);
void con_write(struct tty_struct * tty);

// 复制成规范模式字符序列
void copy_to_cooked(struct tty_struct * tty);

#endif
