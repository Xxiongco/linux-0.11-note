/*
 *  linux/init/main.c
 *
 *  (C) 1991  Linus Torvalds
 */

// LIBRARY is for asm in unistd.h
#define __LIBRARY__                                     
#include <unistd.h>
#include <time.h>

/*
 * we need this inline - forking from kernel space will result
 * in NO COPY ON WRITE (!!!), until an execve is executed. This
 * is no problem, but for the stack. This is handled by not letting
 * main() use the stack at all after fork(). Thus, no function
 * calls - which means inline code for fork too, as otherwise we
 * would use the stack upon exit from 'fork()'.
 *
 * Actually only pause and fork are needed inline, so that there
 * won't be any messing with the stack from main(), but we define
 * some others too.
 */
static inline _syscall0(int,fork)                  //inline is like #define, but expande in compile, and more flexible with variables
static inline _syscall0(int,pause)                 // 用于系统调用，0个参数
static inline _syscall1(int,setup,void *,BIOS)     //0 for 0 variable, 1 for 1 variable
static inline _syscall0(int,sync)                  //update filesystem in hardware disk 

#include <linux/tty.h>
#include <linux/sched.h>                           //task_struct
#include <linux/head.h>                            //GDT，LDT
#include <asm/system.h>                        
#include <asm/io.h>

#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>                                  //file control
#include <sys/types.h>

#include <linux/fs.h>                               //file system

static char printbuf[1024];

extern int vsprintf();
extern void init(void);
extern void blk_dev_init(void);
extern void chr_dev_init(void);
extern void hd_init(void);
extern void floppy_init(void);
extern void mem_init(long start, long end);
extern long rd_init(long mem_start, int length);
extern long kernel_mktime(struct tm * tm);
extern long startup_time;

/*
 * This is set up by the setup-routine at boot-time
 */
#define EXT_MEM_K (*(unsigned short *)0x90002)              // 1M 以后的扩展内存大小
#define DRIVE_INFO (*(struct drive_info *)0x90080)          // 来自BIOS信息，硬盘参数表（机器系统数据）
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)          // 来自BIOS信息，根设备号（软盘）

/*
 * Yeah, yeah, it's ugly, but I cannot find how to do this correctly
 * and this seems to work. I anybody has more info on the real-time
 * clock I'd be interested. Most of this was trial and error, and some
 * bios-listing reading. Urghh.
 */
// 这段宏读取 CMOS 实时时钟信息
#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)     //4 bit for a decimal number 0~9, at most 99

// 初始化时间， 从CMOS芯片中获取
static void time_init(void)
{
	struct tm time;

	do {
		time.tm_sec = CMOS_READ(0);             // 电源关闭时CMOS芯片中的RTC（Real Time Clock）由计算机内部的电池供电，保持活动状态
		time.tm_min = CMOS_READ(2);
		time.tm_hour = CMOS_READ(4);
		time.tm_mday = CMOS_READ(7);
		time.tm_mon = CMOS_READ(8);
		time.tm_year = CMOS_READ(9);
	} while (time.tm_sec != CMOS_READ(0));
	BCD_TO_BIN(time.tm_sec);
	BCD_TO_BIN(time.tm_min);
	BCD_TO_BIN(time.tm_hour);
	BCD_TO_BIN(time.tm_mday);
	BCD_TO_BIN(time.tm_mon);
	BCD_TO_BIN(time.tm_year);
	time.tm_mon--;
	startup_time = kernel_mktime(&time);
}

static long memory_end = 0;
static long buffer_memory_end = 0;
static long main_memory_start = 0;

struct drive_info { char dummy[32]; } drive_info;

void main(void)		/* This really IS void, no error here. */
{			/* The startup routine assumes (well, ...) this */
/*
 * Interrupts are still disabled. Do necessary setups, then
 * enable them
 */
/* 
 * 此时中断仍被禁止着，做完必要的设置后就将其开启。 
 */
// =================================== 内存布局 ==========================================
// 下面这段代码用于保存： 
 // 根设备号 ÎROOT_DEV； 高速缓存末端地址Î buffer_memory_end； 
 // 机器内存数Î memory_end；主内存开始地址 Îmain_memory_start；
 	ROOT_DEV = ORIG_ROOT_DEV;
 	drive_info = DRIVE_INFO;
	memory_end = (1<<20) + (EXT_MEM_K<<10);        // 内存大小=1Mb 字节+扩展内存(k)*1024 字节
	memory_end &= 0xfffff000;                      // 忽略不到 4Kb（1 页）的内存数
	// 假设物理内存为16M，进入第二个分支
	// 主内存开始地址和缓冲区结束地址相同，均为4M位置
	if (memory_end > 16*1024*1024)
		memory_end = 16*1024*1024;    // 如果内存超过 16Mb，则按 16Mb 计
	if (memory_end > 12*1024*1024) 
		buffer_memory_end = 4*1024*1024;
	else if (memory_end > 6*1024*1024)
		buffer_memory_end = 2*1024*1024;
	else
		buffer_memory_end = 1*1024*1024;
	main_memory_start = buffer_memory_end;
#ifdef RAMDISK
	main_memory_start += rd_init(main_memory_start, RAMDISK*1024);
#endif
// =================================== 各种初始化 =========================================
// 包括内存，设备，时间，硬盘，特权等级等
	mem_init(main_memory_start,memory_end);                    //如果内存为8M，分布形式为  0          2M             4M          6M            16M          
	// 中断表与函数挂接                                         //                        |  内核程序  |   缓冲区       |   虚拟盘  |    主内存    | 
	trap_init();                                                 
	//  初始化request[32] 请求项， 用于缓冲区与块设备之间的沟通协调
	blk_dev_init(); 
	// 初始化字符设备，目前为空函数   
	chr_dev_init();
	// 初始化串行口， 包括串口和控制台（serial.c console.c)
	tty_init();
	// 初始化时间
	time_init();
	// 初始化进程0
	sched_init();
	// 缓冲区初始化
	buffer_init(buffer_memory_end);
	// 硬盘软盘初始化
	hd_init();
	// 软驱初始化
	floppy_init();
	// 开启中断
	sti();
	// 进程均为3特权等级，此处的进程0也需要翻转到3用户态模式
	move_to_user_mode();
	if (!fork()) {		/* we count on this going ok */       // 进程1
		init();                                               //child process
	}
/*
 *   NOTE!!   For any other task 'pause()' would mean we have to get a
 * signal to awaken, but task0 is the sole exception (see 'schedule()')
 * as task 0 gets activated at every idle moment (when no other tasks
 * can run). For task0 'pause()' just means we go check if some other
 * task can run, and if not we return here.
 */
	for(;;) pause();
}

// 产生格式化信息并输出到标准输出设备 stdout(1)，这里是指屏幕上显示。
static int printf(const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);                                        //va_start, va_arg, va_end 用于不定长参数
	write(1,printbuf,i=vsprintf(printbuf, fmt, args));          //1 for stdout
	va_end(args);
	return i;
}

static char * argv_rc[] = { "/bin/sh", NULL };		// 调用执行程序时参数的字符串数组
static char * envp_rc[] = { "HOME=/", NULL };		// 调用执行程序时的环境字符串数组。

static char * argv[] = { "-/bin/sh",NULL };
static char * envp[] = { "HOME=/usr/root", NULL };

// 文件交互初始化，用于进程1
void init(void)
{
	int pid,i;

	setup((void *) &drive_info);			// 读取硬盘参数包括分区表信息并建立虚拟盘和安装根文件系统
	(void) open("/dev/tty0",O_RDWR,0);                        // 返回的句柄号 0 -- stdin 标准输入设备
	(void) dup(0);                                            // 复制句柄，产生句柄 1 号 -- stdout 标准输出设备
	(void) dup(0);                                            // 复制句柄，产生句柄 2 号 -- stderr 标准出错输出设备
	printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
		NR_BUFFERS*BLOCK_SIZE);                               // 打印缓冲区块数和总字节数，每块 1024 字节
	printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);
	if (!(pid=fork())) {                                      // 进程1创建进程2！！！！
		close(0);
		if (open("/etc/rc",O_RDONLY,0))         // 以只读形式打开rc文件（配置文件），占据0号文件描述符
			_exit(1);
		execve("/bin/sh",argv_rc,envp_rc);       // 加载shell程序. 此shell程序读取rc文件并启动update进程，用于同步缓冲区的数据到外设
		_exit(2);                                // 每隔一段时间，update程序会被唤醒
	}
	// shell加载普通文件 如/etc/rc 后会退出，所以下面需要重新启动一个shell
	// shell加载字符设备文件不会退出， 如/dev/tty0
	if (pid>0)                                                //parent process
		while (pid != wait(&i))                               //父进程等待子进程的结束。&i 是存放返回状态信息的位置。
			/* nothing */;
	while (1) {                                               //创建的子进程的执行已停止或终止,再创建一个子进程
		if ((pid=fork())<0) {
			printf("Fork failed in init\r\n");
			continue;
		}
		if (!pid) {
			close(0);close(1);close(2);
			setsid();
			(void) open("/dev/tty0",O_RDWR,0);     // 标准输入设备 stdin
			(void) dup(0);                         // 标准输出设备 stdout
			(void) dup(0);                         // 标准错误输出设备 stderr
			_exit(execve("/bin/sh",argv,envp));
		}
		while (1)
			if (pid == wait(&i))
				break;
		printf("\n\rchild %d died with code %04x\n\r",pid,i);
		sync();
	}
	_exit(0);	/* NOTE! _exit, not exit() */
}


// 对于 0.11 版内核，通过上面代码分析可知只要根文件系统是一个 MINIX 文件系统，并且其中只要包
// 含文件/etc/rc、/bin/sh、/dev/* 以及一些目录/etc/、/dev/、/bin/、/home/、/home/root/ 就可以构
// 成一个最简单的根文件系统，让 Linux 运行起来