/*
 *  linux/kernel/signal.c
 *
 *  (C) 1991  Linus Torvalds
 */

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <signal.h>

volatile void do_exit(int error_code);

// 获取当前任务信号屏蔽位图（屏蔽码）
int sys_sgetmask()
{
	return current->blocked;
}

// 设置新的信号屏蔽位图。SIGKILL 不能被屏蔽。返回值是原信号屏蔽位图
int sys_ssetmask(int newmask)
{
	int old=current->blocked;

	current->blocked = newmask & ~(1<<(SIGKILL-1));
	return old;
}

// 复制 sigaction 数据到 fs 数据
static inline void save_old(char * from,char * to)
{
	int i;

	verify_area(to, sizeof(struct sigaction));         // 验证 to 处的内存是否足够
	for (i=0 ; i< sizeof(struct sigaction) ; i++) {
		put_fs_byte(*from,to);         // 复制到 fs 段。一般是用户数据段
		from++;
		to++;
	}
}

// 把 sigaction 数据从 fs 数据段复制到 to 处
static inline void get_new(char * from,char * to)
{
	int i;

	for (i=0 ; i< sizeof(struct sigaction) ; i++)
		*(to++) = get_fs_byte(from++);
}

// signal()系统调用。类似于 sigaction()。为指定的信号安装新的信号句柄(信号处理程序)。 
 // 信号句柄可以是用户指定的函数，也可以是 SIG_DFL（默认句柄）或 SIG_IGN（忽略）。 
 // 参数 signum --指定的信号；handler -- 指定的句柄；restorer -- [??] 
 // 函数返回原信号句柄。
 /**
  * @brief 为指定的信号安装新的信号句柄（信号处理程序）
  * 
  * @param signum 信号值（指定的信号）
  * @param handler 指定的句柄
  * @param restorer 指定的恢复句柄
  * @return int 返回当前任务的指定信号处理句柄（？？返回原信号句柄）
  */
int sys_signal(int signum, long handler, long restorer)
{
	struct sigaction tmp;

	if (signum<1 || signum>32 || signum==SIGKILL)
		return -1;
	tmp.sa_handler = (void (*)(int)) handler;		// 指定的信号处理句柄
	tmp.sa_mask = 0;								// 执行时的信号屏蔽码
	tmp.sa_flags = SA_ONESHOT | SA_NOMASK;			// 该句柄只使用 1 次后就恢复到默认值
	tmp.sa_restorer = (void (*)(void)) restorer;	// 指定信号恢复句柄
	handler = (long) current->sigaction[signum-1].sa_handler;
	current->sigaction[signum-1] = tmp;
	return handler;
}

/**
 * @brief 改变进程在收到一个信号时的操作
 * 
 * @param signum 除了 SIGKILL 以外的任何信号
 * @param action 新的信号操作结构体
 * @param oldaction 保存原始action
 * @return int success-0, fail-1
 */
int sys_sigaction(int signum, const struct sigaction * action,
	struct sigaction * oldaction)
{
	struct sigaction tmp;

	if (signum<1 || signum>32 || signum==SIGKILL)
		return -1;
	tmp = current->sigaction[signum-1];
	get_new((char *) action,
		(char *) (signum-1+current->sigaction));
	if (oldaction)
		save_old((char *) &tmp,(char *) oldaction);
	if (current->sigaction[signum-1].sa_flags & SA_NOMASK)
		current->sigaction[signum-1].sa_mask = 0;
	else
		current->sigaction[signum-1].sa_mask |= (1<<(signum-1));
	return 0;
}

 // 系统调用中断处理程序中真正的信号处理程序（在 kernel/system_call.s,119 行）。 
 // 该段代码的主要作用是将信号的处理句柄插入到用户程序堆栈中，并在本系统调用结束 
 // 返回后立刻执行信号句柄程序，然后继续执行用户的程序。
/**
 * @brief 处理信号中断
 * 
 * @param signr 信号值
 * @param eax 通常用来执行加法，函数调用的返回值一般也放在这里面
 * @param ebx 数据存取
 * @param ecx 通常用来作为计数器，如for循环
 * @param edx 读写I/O端口时，edx用来存放端口号
 * @param fs 文件数据段
 * @param es 扩展数据段
 * @param ds 数据段
 * @param eip 指令寄存器
 * @param cs 代码段
 * @param eflags 标志寄存器
 * @param esp 栈顶指针
 * @param ss 栈段
 */
void do_signal(long signr,long eax, long ebx, long ecx, long edx,
	long fs, long es, long ds,
	long eip, long cs, long eflags,
	unsigned long * esp, long ss)
{
	unsigned long sa_handler;
	long old_eip=eip;
	struct sigaction * sa = current->sigaction + signr - 1;
	int longs;
	unsigned long * tmp_esp;

	sa_handler = (unsigned long) sa->sa_handler;
	if (sa_handler==1)
		return;
	if (!sa_handler) {
		if (signr==SIGCHLD)
			return;
		else
			do_exit(1<<(signr-1));
	}
	if (sa->sa_flags & SA_ONESHOT)
		sa->sa_handler = NULL;
	*(&eip) = sa_handler;							// 将用户调用系统调用的代码指针 eip 指向该信号处理句柄。
	longs = (sa->sa_flags & SA_NOMASK)?7:8;
	*(&esp) -= longs;
	verify_area(esp,longs*4);
	
	// 在用户堆栈中从下到上存放 sa_restorer, 信号 signr, 屏蔽码 blocked(如果 SA_NOMASK 置位), 
    // eax, ecx, edx, eflags 和用户程序原代码指针
	tmp_esp=esp;
	put_fs_long((long) sa->sa_restorer,tmp_esp++);
	put_fs_long(signr,tmp_esp++);
	if (!(sa->sa_flags & SA_NOMASK))
		put_fs_long(current->blocked,tmp_esp++);
	put_fs_long(eax,tmp_esp++);
	put_fs_long(ecx,tmp_esp++);
	put_fs_long(edx,tmp_esp++);
	put_fs_long(eflags,tmp_esp++);
	put_fs_long(old_eip,tmp_esp++);
	current->blocked |= sa->sa_mask;
}

// 该程序执行完之后栈中各个寄存器已经就位，然后继续执行信号处理句柄