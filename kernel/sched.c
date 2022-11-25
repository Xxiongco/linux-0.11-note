/*
 *  linux/kernel/sched.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid(), which just extracts a field from
 * current-task
 */
/* 
 * 'sched.c'是主要的内核文件。其中包括有关调度的基本函数(sleep_on、wakeup、schedule 等)以及 
 * 一些简单的系统调用函数（比如 getpid()，仅从当前任务中获取一个字段）。 
 */
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#include <signal.h>

//  取信号 nr 在信号位图中对应位的二进制数值。信号编号 1-32    比如信号 5 的位图数值 = 1<<(5-1) = 16 = 00010000b
#define _S(nr) (1<<((nr)-1))
// 除了SIGKILL和SIGSTOP之外其他都是可阻塞的信号(…10111111111011111111b)
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

/**
 * @brief 显示任务号nr的进程号、进程状态和内核堆栈空闲字节数（内核堆栈大小为4k，用task_union存储）
 * 
 * @param nr 任务号
 * @param p 任务结构体
 */
void show_task(int nr,struct task_struct * p)
{
	int i,j = 4096-sizeof(struct task_struct);

	printk("%d: pid=%d, state=%d, ",nr,p->pid,p->state);
	i=0;
	while (i<j && !((char *)(p+1))[i])	// 检测指定任务数据结构以后等于 0 的字节数
		i++;
	printk("%d (of %d) chars free in kernel stack\n\r",i,j);
}

// 显示所有任务的任务号、进程号、进程状态和内核堆栈空闲字节数
void show_stat(void)
{
	int i;

	for (i=0;i<NR_TASKS;i++)
		if (task[i])
			show_task(i,task[i]);
}

//  定义每个时间片的滴答数
#define LATCH (1193180/HZ)

extern void mem_use(void);

extern int timer_interrupt(void);
extern int system_call(void);

// 任务联合，包括任务结构体和内核堆栈
union task_union {
	struct task_struct task;
	char stack[PAGE_SIZE];
};

// 定义初始任务的数据(sched.h 中) 非常重要！！！！
static union task_union init_task = {INIT_TASK,};

long volatile jiffies=0;		// 滴答数
long startup_time=0;                       // 开机时间。从 1970-1-1，0:0:0 开始计时的秒数
struct task_struct *current = &(init_task.task);		// 当前任务指针
struct task_struct *last_task_used_math = NULL;      // 使用过协处理器任务的指针

struct task_struct * task[NR_TASKS] = {&(init_task.task), };	// 最多64个任务

long user_stack [ PAGE_SIZE>>2 ] ;    //定义用户堆栈数组（1024 项）

struct {                             // 堆栈开始结构（地址指针，数据段选择符）
	long * a;
	short b;
	} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };		// 洪湖堆栈从最高地址开始
/*
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
void math_state_restore()
{
	if (last_task_used_math == current)
		return;
	__asm__("fwait");
	if (last_task_used_math) {
		__asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
	}
	last_task_used_math=current;
	if (current->used_math) {
		__asm__("frstor %0"::"m" (current->tss.i387));
	} else {
		__asm__("fninit"::);
		current->used_math=1;
	}
}

/*
 *  'schedule()' is the scheduler function. This is GOOD CODE! There
 * probably won't be any reason to change this, as it should work well
 * in all circumstances (ie gives IO-bound processes good response etc).
 * The one thing you might take a look at is the signal-handler code here.
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 * 进程调度
 */
void schedule(void)
{
	int i,next,c;
	struct task_struct ** p;

/* check alarm, wake up any interruptible tasks that have got a signal */
// ========== 根据信号唤醒进程 ===========
	for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)
		if (*p) {                  // 如果任务的 alarm 时间已经过期(alarm<jiffies),在信号位图中置 SIGALRM 信号，然后清 alarm
			if ((*p)->alarm && (*p)->alarm < jiffies) {
					(*p)->signal |= (1<<(SIGALRM-1));
					(*p)->alarm = 0;
				}
			if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&        //则置任务为就绪状态
			(*p)->state==TASK_INTERRUPTIBLE)
				(*p)->state=TASK_RUNNING;
		}

/* this is the scheduler proper: */

	while (1) {
		c = -1;
		next = 0;
		i = NR_TASKS;
		p = &task[NR_TASKS];
		while (--i) {
			if (!*--p)
				continue;
			if ((*p)->state == TASK_RUNNING && (*p)->counter > c)   //get counter_max 值
				c = (*p)->counter, next = i;
		}
		if (c) break;
		for(p = &LAST_TASK ; p > &FIRST_TASK ; --p)                //calculate counter
			if (*p)
				(*p)->counter = ((*p)->counter >> 1) +
						(*p)->priority;
	}
	switch_to(next);
}

// pause()系统调用。转换当前任务的状态为可中断的等待状态，并重新调度。
int sys_pause(void)
{
	current->state = TASK_INTERRUPTIBLE;
	schedule();
	return 0;
}

// 将当前任务置为不可中断的等待状态,并让睡眠对象的任务指针指向当前任务，只有明确地唤醒时才会返回
void sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp = *p;
	*p = current;
	current->state = TASK_UNINTERRUPTIBLE;
	schedule();
	if (tmp)
		tmp->state=0;
}

// 将当前任务置为可中断的等待状态，等待指定（*p）任务的完成
void interruptible_sleep_on(struct task_struct **p)
{
	struct task_struct *tmp;

	if (!p)
		return;
	if (current == &(init_task.task))
		panic("task[0] trying to sleep");
	tmp=*p;
	*p=current;
repeat:	current->state = TASK_INTERRUPTIBLE;
	schedule();
	if (*p && *p != current) {
		(**p).state=0;
		goto repeat;
	}
	*p=NULL;
	if (tmp)
		tmp->state=0;           //TASK_RUNNING
}

// 将状态置为 TASK_RUNNING
void wake_up(struct task_struct **p)
{
	if (p && *p) {
		(**p).state=0;
		*p=NULL;                //?????????????  相当于释放指针
	}
}

/*
 * OK, here are some floppy things that shouldn't be in the kernel
 * proper. They are here because the floppy needs a timer, and this
 * was the easiest way of doing it.
 */
static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
static int  mon_timer[4]={0,0,0,0};
static int moff_timer[4]={0,0,0,0};
unsigned char current_DOR = 0x0C;

int ticks_to_floppy_on(unsigned int nr)
{
	extern unsigned char selected;
	unsigned char mask = 0x10 << nr;

	if (nr>3)
		panic("floppy_on: nr>3");
	moff_timer[nr]=10000;		/* 100 s = very big :-) */
	cli();				/* use floppy_off to turn it off */
	mask |= current_DOR;
	if (!selected) {
		mask &= 0xFC;
		mask |= nr;
	}
	if (mask != current_DOR) {
		outb(mask,FD_DOR);
		if ((mask ^ current_DOR) & 0xf0)
			mon_timer[nr] = HZ/2;
		else if (mon_timer[nr] < 2)
			mon_timer[nr] = 2;
		current_DOR = mask;
	}
	sti();
	return mon_timer[nr];
}

void floppy_on(unsigned int nr)
{
	cli();
	while (ticks_to_floppy_on(nr))
		sleep_on(nr+wait_motor);
	sti();
}

void floppy_off(unsigned int nr)
{
	moff_timer[nr]=3*HZ;
}

void do_floppy_timer(void)
{
	int i;
	unsigned char mask = 0x10;

	for (i=0 ; i<4 ; i++,mask <<= 1) {
		if (!(mask & current_DOR))
			continue;
		if (mon_timer[i]) {
			if (!--mon_timer[i])
				wake_up(i+wait_motor);
		} else if (!moff_timer[i]) {
			current_DOR &= ~mask;
			outb(current_DOR,FD_DOR);
		} else
			moff_timer[i]--;
	}
}

#define TIME_REQUESTS 64   // 最多可有 64 个定时器链表（64 个任务）

// 定时器链表结构和定时器数组
static struct timer_list {
	long jiffies;                   // 定时滴答数
	void (*fn)();					// 定时器处理程序
	struct timer_list * next;		// 下一个定时器
} timer_list[TIME_REQUESTS], * next_timer = NULL;

// 添加定时器。输入参数为指定的定时值(滴答数)和相应的处理程序指针
void add_timer(long jiffies, void (*fn)(void))
{
	struct timer_list * p;

	if (!fn)
		return;
	cli();
	if (jiffies <= 0)              // 如果定时值<=0，则立刻调用其处理程序。并且该定时器不加入链表中
		(fn)();
	else {
		for (p = timer_list ; p < timer_list + TIME_REQUESTS ; p++)
			if (!p->fn)              // 从定时器数组中，找一个空闲项
				break;
		if (p >= timer_list + TIME_REQUESTS)            // 如果已经用完了定时器数组，则系统崩溃
			panic("No more time requests free");
		p->fn = fn;
		p->jiffies = jiffies;
		p->next = next_timer;
		next_timer = p;
		while (p->next && p->next->jiffies < p->jiffies) {         // 链表项按定时值从小到大排序
			p->jiffies -= p->next->jiffies;
			fn = p->fn;
			p->fn = p->next->fn;
			p->next->fn = fn;
			jiffies = p->jiffies;
			p->jiffies = p->next->jiffies;
			p->next->jiffies = jiffies;
			p = p->next;
		}
	}
	sti();
}

// 时钟中断 C 函数处理程序，
// 每个时钟中断都要更新首个任务的定时器，
// 然后进行相应的定时器处理程序，然后进行任务调度
void do_timer(long cpl)      //current priority level
{
	extern int beepcount;            // 扬声器发声时间滴答数
	extern void sysbeepstop(void);   //关闭扬声器

	if (beepcount)
		if (!--beepcount)
			sysbeepstop();

	if (cpl)
		current->utime++;
	else
		current->stime++;

// 如果有用户的定时器存在，则将链表第 1 个定时器的值减 1。如果已等于 0，则调用相应的处理 
 // 程序，并将该处理程序指针置为空。然后去掉该项定时器
	if (next_timer) {
		next_timer->jiffies--;
		while (next_timer && next_timer->jiffies <= 0) {
			void (*fn)(void);
			
			fn = next_timer->fn;
			next_timer->fn = NULL;
			next_timer = next_timer->next;
			(fn)();
		}
	}
	if (current_DOR & 0xf0)
		do_floppy_timer();
	if ((--current->counter)>0) return;      // 当前线程还有剩余时间片，直接返回
	current->counter=0;
	if (!cpl) return;						 // 对于超级用户程序，不依赖 counter 值进行调度
	schedule();                              
}

// 系统调用功能 - 设置报警定时时间值(秒)。 
 // 如果已经设置过 alarm 值，则返回旧值，否则返回 0
int sys_alarm(long seconds)
{
	int old = current->alarm;

	if (old)
		old = (old - jiffies) / HZ;
	current->alarm = (seconds>0)?(jiffies+HZ*seconds):0;
	return (old);
}

// 取当前进程号 pid
int sys_getpid(void)
{
	return current->pid;
}

// 取父进程号ppid
int sys_getppid(void)
{
	return current->father;
}

// 取用户号uid
int sys_getuid(void)
{
	return current->uid;
}

// 取euid
int sys_geteuid(void)
{
	return current->euid;
}

// 取组号 gid。
int sys_getgid(void)
{
	return current->gid;
}

// 取 egid。
int sys_getegid(void)
{
	return current->egid;
}

// 系统调用功能 -- 降低对 CPU 的使用优先权
// 应该限制 increment 大于 0，否则的话,可使优先权增大！！
int sys_nice(long increment)
{
	if (current->priority-increment>0)
		current->priority -= increment;
	return 0;
}

// 进程调度初始化
void sched_init(void)
{
	int i;
	struct desc_struct * p;

	if (sizeof(struct sigaction) != 16)
		panic("Struct sigaction MUST be 16 bytes");
	set_tss_desc(gdt+FIRST_TSS_ENTRY,&(init_task.task.tss));           // 设置初始任务, 非常重要：初始化进程0
	set_ldt_desc(gdt+FIRST_LDT_ENTRY,&(init_task.task.ldt));
	p = gdt+2+FIRST_TSS_ENTRY;
	// 清任务数组和描述符表项（注意 i=1 开始，所以初始任务的描述符还在）。
	for(i=1;i<NR_TASKS;i++) {
		task[i] = NULL;
		p->a=p->b=0;
		p++;
		p->a=p->b=0;
		p++;
	}
/* Clear NT, so that we won't have troubles with that later on */
/* 清除标志寄存器中的位 NT，这样以后就不会有麻烦 */
	__asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl");
	//给 tr 寄存器赋值， 指向任务状态段 tss
	ltr(0);
	//给 ldtr 寄存器赋值, 指向局部描述符表 ldt
	lldt(0);
	// 下面代码用于初始化 8253 定时器
	outb_p(0x36,0x43);		/* binary, mode 3, LSB/MSB, ch 0 */
	outb_p(LATCH & 0xff , 0x40);	/* LSB */
	outb(LATCH >> 8 , 0x40);	/* MSB */             //  这四行代码就开启了这个定时器，之后这个定时器变会持续的、以一定频率的向 CPU 发出中断信号， 中断处理程序为 timer_interrupt
	set_intr_gate(0x20,&timer_interrupt);
	outb(inb_p(0x21)&~0x01,0x21);
	set_system_gate(0x80,&system_call);          // 所有用户态程序想要调用内核提供的方法，都需要基于这个系统调用来进行
}
