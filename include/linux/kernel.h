/*
 * 'kernel.h' contains some often-used function prototypes etc
 */
// 定义了一些内核常用的函数原型等，用户态不常用

// 验证给定地址开始的内存块是否超限。若超限则追加内存。( kernel/fork.c, 24 )
void verify_area(void * addr,int count);
// 显示内核出错信息，然后进入死循环。( kernel/panic.c, 16 )
volatile void panic(const char * str);
// 标准打印（显示）函数。( init/main.c, 151)
int printf(const char * fmt, ...);
// 内核专用的打印信息函数，功能与 printf()相同。( kernel/printk.c, 21 )
int printk(const char * fmt, ...);
// 往 tty 上写指定长度的字符串。( kernel/chr_drv/tty_io.c, 290 )
int tty_write(unsigned ch,char * buf,int count);
// 通用内核内存分配函数。( lib/malloc.c, 117)
void * malloc(unsigned int size);
// 释放指定对象占用的内存。( lib/malloc.c, 182)
void free_s(void * obj, int size);

#define free(x) free_s((x), 0)

/*
 * This is defined as a macro, but at some point this might become a
 * real subroutine that sets a flag if it returns true (to do
 * BSD-style accounting where the process is flagged if it uses root
 * privs).  The implication of this is that you should do normal
 * permissions checks first, and check suser() last.
 */
#define suser() (current->euid == 0)    // 检测是否是超级用户

