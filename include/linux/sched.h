#ifndef _SCHED_H
#define _SCHED_H

// 调度程序头文件，定义了任务结构 task_struct、初始任务 0 的数据，还有一些有关描述符参数设置
// 和获取的嵌入式汇编函数宏语句。    很重要！！
 
#define NR_TASKS 64		// 系统中同时最多任务（进程）数
#define HZ 100			// 定义系统时钟滴答频率(1 百赫兹，每个滴答 10ms)

#define FIRST_TASK task[0]				// 任务 0 比较特殊，所以特意给它单独定义一个符号
#define LAST_TASK task[NR_TASKS-1]		// 任务数组中的最后一项任务

#include <linux/head.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <signal.h>

#if (NR_OPEN > 32)
#error "Currently the close-on-exec-flags are in one word, max 32 files/proc"
#endif

// 创建、就绪、执行、阻塞、终止 是进程的一般5种理论状态
// linux有7中状态
// 1.运行状态， 包括就绪态和运行态
// 2.可中断睡眠状态， 也就是阻塞状态，收到信号后可以执行信号处理函数
// 3.不可中断的睡眠状态， 磁盘IO时会出现这种状态，进程无法被中断，不能响应信号
// 4.停止状态
// 5.退出状态
// 6.僵尸状态， 子进程先于父进程退出，并且父进程没有调用wait或waitpid回收子进程。此时子进程即处于僵尸状态
// 7.跟踪状态， 当利用gdb调试某个程序，程序停留在某个断点处时，就处于跟踪状态
// ps -aux 查看进程状态
#define TASK_RUNNING		    0	// 进程正在运行或已准备就绪，就绪态+运行态
#define TASK_INTERRUPTIBLE	    1	// 进程处于可中断等待状态
#define TASK_UNINTERRUPTIBLE	2	// 进程处于不可中断等待状态，主要用于 I/O 操作等待
#define TASK_ZOMBIE		        3	// 进程处于僵死状态，已经停止运行，但父进程还没发信号
#define TASK_STOPPED		    4	// 进程已停止

#ifndef NULL
#define NULL ((void *) 0)
#endif

// 复制进程的页目录页表。Linus 认为这是内核中最复杂的函数之一。( mm/memory.c, 105 )
extern int copy_page_tables(unsigned long from, unsigned long to, long size);
// 释放页表所指定的内存块及页表本身。( mm/memory.c, 150 )
extern int free_page_tables(unsigned long from, unsigned long size);

// 调度程序的初始化函数。( kernel/sched.c, 385 )
extern void sched_init(void);
// 进程调度函数。( kernel/sched.c, 104 )
extern void schedule(void);
// 异常(陷阱)中断处理初始化函数，设置中断调用门并允许中断请求信号。( kernel/traps.c, 181 )
extern void trap_init(void);
// 显示内核出错信息，然后进入死循环。( kernel/panic.c, 16 )
extern void panic(const char * str);
// 往 tty 上写指定长度的字符串。( kernel/chr_drv/tty_io.c, 290 )
extern int tty_write(unsigned minor,char * buf,int count);

typedef int (*fn_ptr)();   // 定义函数指针类型

// 下面是数学协处理器使用的结构，主要用于保存进程切换时 i387 的执行状态信息
struct i387_struct {
	long	cwd;
	long	swd;
	long	twd;
	long	fip;
	long	fcs;
	long	foo;
	long	fos;
	long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
};

// 任务状态段(Task State Segment)，104字节，保存任务信息，任务（进程/线程）切换用，由TR（任务寄存器）寻址。字段构成
// 1. 寄存器保存区域
// 2. 内核堆栈指针区域   一个任务可能具有四个堆栈，对应四个特权级。四个堆栈需要四个堆栈指针，3级属于用户态，没有后缀
// 3. 地址映射寄存器    用于分页寻址，似乎线程切换不需要
// 4. 链接字段     前一任务的TSS描述符的选择子
// 5. 其他字段     I/O许可位图
struct tss_struct {
	long	back_link;	/* 16 high bits zero  前一执行任务的TSS任务的描述符 */   
	long	esp0;									// 内核0级堆栈栈顶指针
	long	ss0;		/* 16 high bits zero 0级堆栈栈段寄存器 */		
	long	esp1;									// 1级堆栈栈顶指针
	long	ss1;		/* 16 high bits zero 1级堆栈栈段寄存器 */
	long	esp2;									// 2级堆栈栈顶指针
	long	ss2;		/* 16 high bits zero 2级堆栈栈段寄存器 */
	long	cr3;									// 控制寄存器3，存储页目录地址
	long	eip;									// 指令寄存器
	long	eflags;									// 标志寄存器
	long	eax,ecx,edx,ebx;						// 通用寄存器
	long	esp;									// 栈顶指针
	long	ebp;									// 栈底指针
	long	esi;									// 源地址
	long	edi;									// 目的地址
	long	es;		/* 16 high bits zero 额外段寄存器 */
	long	cs;		/* 16 high bits zero 代码段寄存器 */
	long	ss;		/* 16 high bits zero 栈段寄存器 */
	long	ds;		/* 16 high bits zero 数据段寄存器 */
	long	fs;		/* 16 high bits zero 文件段寄存器 */
	long	gs;		/* 16 high bits zero 额外段寄存器 */
	long	ldt;		/* 16 high bits zero 局部描述符 */
	long	trace_bitmap;	/* bits: trace 0, bitmap 16-31 */  //  当任务进行切换时导致 CPU 产生一个调试(debug)异常的 T-比特位（调试跟踪位）；I/O 比特位图基地址
	struct i387_struct i387;
};
// I/O 比特位图中的每 1 比特对应 1 个 I/O 端口。比如端口 41 的比特位就是 I/O 位图基地址+5，位偏移
// 1 处。在保护模式中，当遇到 1 个 I/O 指令时(IN, INS, OUT, OUTS)，CPU 首先就会检查当前特权级是否
// 小于标志寄存器的 IOPL，如果这个条件满足，就执行该 I/O 操作。如果不满足，那么 CPU 就会检查 TSS 中
// 的 I/O 比特位图。如果相应比特位是置位的，就会产生一般保护性异常，否则就会执行该 I/O 操作


// 任务（进程）数据结构
struct task_struct {
/* these are hardcoded - don't touch */
	long state;	/* -1 unrunnable, 0 runnable, >0 stopped */			// 任务的运行状态（-1 不可运行，0 可运行(就绪)，>0 已停止）
	long counter;    //counter 值的计算方式为 counter = counter /2 + priority,  优先执行counter最大的任务;  任务运行时间计数(递减)（滴答数）(时间片)
	long priority;		// 运行优先数。任务开始运行时 counter = priority，越大运行越长
	long signal;		// 信号。是位图，每个比特位代表一种信号，信号值=位偏移值+1
	struct sigaction sigaction[32];		// 信号执行属性结构，对应信号将要执行的操作和标志信息
	long blocked;	/* bitmap of masked signals */  // 进程信号屏蔽码（对应信号位图）
/* various fields */
	int exit_code;													//任务执行停止的退出码，其父进程会取
	unsigned long start_code,end_code,end_data,brk,start_stack;		// 代码段地址,代码长度，数据长度，总长度，栈段地址
	long pid,father,pgrp,session,leader;							// 进程标识号，父进程号，父进程组号，会话号，会话首领
	unsigned short uid,euid,suid;									// 用户标识号， 有效用户id，保存的用户id
	unsigned short gid,egid,sgid;									// 组标识号，有效组号，保存的组号
	long alarm;														// 报警定时器值
	long utime,stime,cutime,cstime,start_time;						// 用户态运行时间，系统态运行时间，子进程用户态运行时间，子进程系统态运行时间，进程开始运行时刻
	unsigned short used_math;										// 是否使用了协处理器
/* file system info */
	int tty;		/* -1 if no tty, so it must be signed */	// 进程使用 tty 的子设备号。-1 表示没有使用
	unsigned short umask;										// 文件创建属性屏蔽位
	struct m_inode * pwd;										// 当前工作目录i节点
	struct m_inode * root;										// 跟目录i节点
	struct m_inode * executable;								// 执行文件i节点
	unsigned long close_on_exec;								// 执行时关闭文件句柄位图标志
	struct file * filp[NR_OPEN];								// 进程使用的文件表结构，用于保存文件句柄
/* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
	struct desc_struct ldt[3];										// 局部描述符段， 0-空，1-代码段 cs，2-数据和堆栈段 ds&ss
/* tss for this task */
	struct tss_struct tss;										// 本进程的任务状态段信息结构
};

/*
 *  INIT_TASK is used to set up the first task table, touch at
 * your own risk!. Base=0, limit=0x9ffff (=640kB)
 */
#define INIT_TASK \
/* state etc */	{ 0,15,15, \
/* signals */	0,{{},},0, \
/* ec,brk... */	0,0,0,0,0,0, \
/* pid etc.. */	0,-1,0,0,0, \
/* uid etc */	0,0,0,0,0,0, \
/* alarm */	0,0,0,0,0,0, \
/* math */	0, \
/* fs info */	-1,0022,NULL,NULL,NULL,0, \
/* filp */	{NULL,}, \
	{ \
		{0,0}, \
/* ldt  */	{0x9f,0xc0fa00}, \
		{0x9f,0xc0f200}, \
	}, \
/*tss*/	{0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&pg_dir,\
	 0,0,0,0,0,0,0,0, \
	 0,0,0x17,0x17,0x17,0x17,0x17,0x17, \
	 _LDT(0),0x80000000, \
		{} \
	}, \
}
// 代码长 640K，基址 0x0，G=1，D=1，DPL=3，P=1 TYPE=0x0a
// 数据长 640K，基址 0x0，G=1，D=1，DPL=3，P=1 TYPE=0x02


extern struct task_struct *task[NR_TASKS];			// 任务数组
extern struct task_struct *last_task_used_math;		// 上一个使用过协处理器的进程
extern struct task_struct *current;					// 当前进程变量
extern long volatile jiffies;						// 从开机开始算起的滴答数（10ms/滴答）
extern long startup_time;							// 开机时间。从 1970:0:0:0 开始计时的秒数

#define CURRENT_TIME (startup_time+jiffies/HZ)		// 当前时间（秒数）

// 添加定时器函数（定时时间 jiffies 滴答数，定时到时调用函数*fn()）。( kernel/sched.c,272)
extern void add_timer(long jiffies, void (*fn)(void));
// 不可中断的等待睡眠。( kernel/sched.c, 151 )
extern void sleep_on(struct task_struct ** p);
// 可中断的等待睡眠。( kernel/sched.c, 167 )
extern void interruptible_sleep_on(struct task_struct ** p);
// 明确唤醒睡眠的进程。( kernel/sched.c, 188 )
extern void wake_up(struct task_struct ** p);

/*
 * Entry into gdt where to find first TSS. 0-nul, 1-cs, 2-ds, 3-syscall
 * 4-TSS0, 5-LDT0, 6-TSS1 etc ...
 */
#define FIRST_TSS_ENTRY 4											// 全局表中第 1 个任务状态段(TSS)描述符的选择符索引号
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)							// 全局表中第 1 个局部描述符表(LDT)描述符的选择符索引号
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))		// 宏定义，计算在全局表中第 n 个任务的 TSS 描述符的索引号（选择符）
#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))		// 宏定义，计算在全局表中第 n 个任务的 LDT 描述符的索引号
#define ltr(n) __asm__("ltr %%ax"::"a" (_TSS(n)))					// 宏定义，加载第 n 个任务的任务寄存器 tr
#define lldt(n) __asm__("lldt %%ax"::"a" (_LDT(n)))					// 宏定义，加载第 n 个任务的局部描述符表寄存器 ldtr
// 取当前运行任务的任务号（是任务数组中的索引值，与进程号 pid 不同）
#define str(n) \
__asm__("str %%ax\n\t" \
	"subl %2,%%eax\n\t" \
	"shrl $4,%%eax" \
	:"=a" (n) \
	:"a" (0),"i" (FIRST_TSS_ENTRY<<3))
/*
 *	switch_to(n) should switch tasks to task nr n, first
 * checking that n isn't the current task, in which case it does nothing.
 * This also clears the TS-flag if the task we switched to has used
 * tha math co-processor latest.
 * 切换任务
 */
#define switch_to(n) {\
struct {long a,b;} __tmp; \
__asm__("cmpl %%ecx,_current\n\t" \
	"je 1f\n\t" \
	"movw %%dx,%1\n\t" \
	"xchgl %%ecx,_current\n\t" \
	"ljmp %0\n\t" \
	"cmpl %%ecx,_last_task_used_math\n\t" \
	"jne 1f\n\t" \
	"clts\n" \
	"1:" \
	::"m" (*&__tmp.a),"m" (*&__tmp.b), \
	"d" (_TSS(n)),"c" ((long) task[n])); \
}

// 页面地址对准。（在内核代码中没有任何地方引用!!）
#define PAGE_ALIGN(n) (((n)+0xfff)&0xfffff000)

// 设置位于地址 addr 处描述符中的各基地址字段(基地址是 base)，参见列表后说明
#define _set_base(addr,base) \
__asm__("movw %%dx,%0\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %%dl,%1\n\t" \
	"movb %%dh,%2" \
	::"m" (*((addr)+2)), \
	  "m" (*((addr)+4)), \
	  "m" (*((addr)+7)), \
	  "d" (base) \
	:"dx")

// 设置位于地址 addr 处描述符中的段限长字段(段长是 limit)
#define _set_limit(addr,limit) \
__asm__("movw %%dx,%0\n\t" \
	"rorl $16,%%edx\n\t" \
	"movb %1,%%dh\n\t" \
	"andb $0xf0,%%dh\n\t" \
	"orb %%dh,%%dl\n\t" \
	"movb %%dl,%1" \
	::"m" (*(addr)), \
	  "m" (*((addr)+6)), \
	  "d" (limit) \
	:"dx")

// 设置局部描述符表中 ldt 描述符的基地址字段
#define set_base(ldt,base) _set_base( ((char *)&(ldt)) , base )
// 设置局部描述符表中 ldt 描述符的段长字段
#define set_limit(ldt,limit) _set_limit( ((char *)&(ldt)) , (limit-1)>>12 )

// 从地址 addr 处描述符中取段基地址。功能与_set_base()正好相反
#define _get_base(addr) ({\
unsigned long __base; \
__asm__("movb %3,%%dh\n\t" \
	"movb %2,%%dl\n\t" \
	"shll $16,%%edx\n\t" \
	"movw %1,%%dx" \
	:"=d" (__base) \
	:"m" (*((addr)+2)), \
	 "m" (*((addr)+4)), \
	 "m" (*((addr)+7))); \
__base;})

// 取局部描述符表中 ldt 所指段描述符中的基地址
#define get_base(ldt) _get_base( ((char *)&(ldt)) )

// 取段选择符 segment 的段长值
#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__("lsll %1,%0\n\tincl %0":"=r" (__limit):"r" (segment)); \
__limit;})

#endif
