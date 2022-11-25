/*
 *  linux/kernel/serial.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *	serial.c
 *
 * This module implements the rs232 io functions
 *	void rs_write(struct tty_struct * queue);
 *	void rs_init(void);
 * and all interrupts pertaining to serial IO.
 */
/* 
 * serial.c 
 * 该程序用于实现 rs232 的输入输出功能 
 * void rs_write(struct tty_struct *queue); 
 * void rs_init(void); 
 * 以及与传输 IO 有关系的所有中断处理程序。 
 */

#include <linux/tty.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <asm/io.h>

#define WAKEUP_CHARS (TTY_BUF_SIZE/4)    // 当写队列中含有 WAKEUP_CHARS 个字符时，就开始发送

extern void rs1_interrupt(void);         // 串行口 1 的中断处理程序(rs_io.s, 34)
extern void rs2_interrupt(void);

// port: 串口 1 - 0x3F8，串口 2 - 0x2F8
static void init(int port)
{
	outb_p(0x80,port+3);	/* set DLAB of line control reg */    /* 设置线路控制寄存器的 DLAB 位(位 7) */
	outb_p(0x30,port);	/* LS of divisor (48 -> 2400 bps */    /* 发送波特率因子低字节，0x30->2400bps */
	outb_p(0x00,port+1);	/* MS of divisor */
	outb_p(0x03,port+3);	/* reset DLAB */
	outb_p(0x0b,port+4);	/* set DTR,RTS, OUT_2 */
	outb_p(0x0d,port+1);	/* enable all intrs but writes */
	(void)inb(port);	/* read data port to reset things (?) */
}

//串口中断的开启
// 初始化串行中断程序和串行接口
void rs_init(void)
{
	set_intr_gate(0x24,rs1_interrupt);
	set_intr_gate(0x23,rs2_interrupt);
	init(tty_table[1].read_q.data);
	init(tty_table[2].read_q.data);
	outb(inb_p(0x21)&0xE7,0x21);
}

/*
 * This routine gets called when tty_write has put something into
 * the write_queue. It must check wheter the queue is empty, and
 * set the interrupt register accordingly
 *
 *	void _rs_write(struct tty_struct * tty);
 */
void rs_write(struct tty_struct * tty)
{
	cli();
 // 如果写队列不空，则从 0x3f9(或 0x2f9) 首先读取中断允许寄存器内容，添上发送保持寄存器 
 // 中断允许标志（位 1）后，再写回该寄存器
	if (!EMPTY(tty->write_q))
		outb(inb_p(tty->write_q.data+1)|0x02,tty->write_q.data+1);
	sti();
}
