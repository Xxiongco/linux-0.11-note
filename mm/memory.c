/*
 *  linux/mm/memory.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 * demand-loading started 01.12.91 - seems it is high on the list of
 * things wanted, and it should be easy to implement. - Linus
 */

/*
 * Ok, demand-loading was easy, shared pages a little bit tricker. Shared
 * pages started 02.12.91, seems to work. - Linus.
 *
 * Tested sharing by executing about 30 /bin/sh: under the old kernel it
 * would have taken more than the 6M I have free, but it worked well as
 * far as I could see.
 *
 * Also corrected some "invalidate()"s - I wasn't doing enough of them.
 */

#include <signal.h>

#include <asm/system.h>

#include <linux/sched.h>
#include <linux/head.h>
#include <linux/kernel.h>

volatile void do_exit(long code);

// out of memory 内存已经用完
static inline volatile void oom(void)		
{
	printk("out of memory\n\r");
	do_exit(SIGSEGV);
}

 // 刷新页变换高速缓冲。 
 // 为了提高地址转换的效率，CPU 将最近使用的页表数据存放在芯片中高速缓冲中。在修改过页表 
 // 信息之后，就需要刷新该缓冲区。这里使用重新加载页目录基址寄存器 cr3 的方法来进行刷新。
#define invalidate() \
__asm__("movl %%eax,%%cr3"::"a" (0))

/* these are not to be changed without changing head.s etc */
#define LOW_MEM 0x100000							// 内存低端（1M）
#define PAGING_MEMORY (15*1024*1024)				// 分页内存 15M
#define PAGING_PAGES (PAGING_MEMORY>>12)            // 0xf00 = 3840  分页后的页数
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)			// 计算指定地址的页面号
#define USED 100

#define CODE_SPACE(addr) ((((addr)+4095)&~4095) < \
current->start_code + current->end_code)

static long HIGH_MEMORY = 0;

// 复制页面
#define copy_page(from,to) \
__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024):"cx","di","si")

static unsigned char mem_map [ PAGING_PAGES ] = {0,};		// 内存映射字节位图(1 字节代表 1 页)

/*
 * Get physical address of first (actually last :-) free page, and mark it
 * used. If no free pages left, return 0.
 */
//// 取空闲页面。如果已经没有内存了，则返回 0。 
 // 输入：%1(ax=0) - 0；%2(LOW_MEM)；%3(cx=PAGING PAGES)；%4(di=mem_map+PAGING_PAGES-1)。 
 // 输出：返回%0(ax=页面号)。 
 // 从内存映像末端开始向前扫描所有页面标志（页面总数为 PAGING_PAGES），如果有页面空闲（对应 
 // 内存映像位为 0）则返回页面地址。
unsigned long get_free_page(void)
{
	// 在mem_map 中反向扫描得到空闲页面，故从16M高地址开始分配页面
register unsigned long __res asm("ax");

__asm__("std ; repne ; scasb\n\t"
	"jne 1f\n\t"
	"movb $1,1(%%edi)\n\t"
	"sall $12,%%ecx\n\t"
	"addl %2,%%ecx\n\t"
	"movl %%ecx,%%edx\n\t"
	"movl $1024,%%ecx\n\t"
	"leal 4092(%%edx),%%edi\n\t"
	"rep ; stosl\n\t"
	"movl %%edx,%%eax\n"
	"1:"
	:"=a" (__res)
	:"0" (0),"i" (LOW_MEM),"c" (PAGING_PAGES),
	"D" (mem_map+PAGING_PAGES-1)
	:"di","cx","dx");
return __res;
}

/*
 * Free a page of memory at physical address 'addr'. Used by
 * 'free_page_tables()'
 */
void free_page(unsigned long addr)
{
	if (addr < LOW_MEM) return;
	if (addr >= HIGH_MEMORY)
		panic("trying to free nonexistent page");
	addr -= LOW_MEM;			// 物理地址-低端内存位置，再除以 4KB，得页面号
	addr >>= 12;
	if (mem_map[addr]--) return;		// 如果对应内存页面映射字节不等于 0，则减 1 返回
	mem_map[addr]=0;
	panic("trying to free free page");
}

/*
 * This function frees a continuos block of page tables, as needed
 * by 'exit()'. As does copy_page_tables(), this handles only 4Mb blocks.
 */
/**
 * @brief 释放页表连续的内存块
 * 
 * @param from 起始基地址
 * @param size 释放的长度
 * @return int 0-成功
 */
int free_page_tables(unsigned long from,unsigned long size)
{
	unsigned long *pg_table;
	unsigned long * dir, nr;

	if (from & 0x3fffff)
		panic("free_page_tables called with wrong alignment");
	if (!from)
		panic("Trying to free up swapper memory space");
	size = (size + 0x3fffff) >> 22;
	dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */
	for ( ; size-->0 ; dir++) {
		if (!(1 & *dir))
			continue;
		pg_table = (unsigned long *) (0xfffff000 & *dir);
		for (nr=0 ; nr<1024 ; nr++) {
			if (1 & *pg_table)
				free_page(0xfffff000 & *pg_table);
			*pg_table = 0;
			pg_table++;
		}
		free_page(0xfffff000 & *dir);
		*dir = 0;
	}
	invalidate();		// 刷新页变换高速缓冲
	return 0;
}

/*
 *  Well, here is one of the most complicated functions in mm. It
 * copies a range of linerar addresses by copying only the pages.
 * Let's hope this is bug-free, 'cause this one I don't want to debug :-)
 *
 * Note! We don't copy just any chunks of memory - addresses have to
 * be divisible by 4Mb (one page-directory entry), as this makes the
 * function easier. It's used only by fork anyway.
 *
 * NOTE 2!! When from==0 we are copying kernel space for the first
 * fork(). Then we DONT want to copy a full page-directory entry, as
 * that would lead to some serious memory waste - we just copy the
 * first 160 pages - 640kB. Even that is more than we need, but it
 * doesn't take any more memory - we don't copy-on-write in the low
 * 1 Mb-range, so the pages can be shared with the kernel. Thus the
 * special case for nr=xxxx.
 */
/**
 * @brief 复制进程的页目录页表
 * 
 * @param from 源地址的内存偏移
 * @param to 目的地址的内存偏移
 * @param size 需要复制的内存大小
 * @return int 
 */
int copy_page_tables(unsigned long from,unsigned long to,long size)   
{
	// 进程1创建时 from = 0, to = 64M，  size = 640k或160个页面
	unsigned long * from_page_table;
	unsigned long * to_page_table;
	unsigned long this_page;
	unsigned long * from_dir, * to_dir;
	unsigned long nr;

	if ((from&0x3fffff) || (to&0x3fffff))    // 源地址和目的地址都需要是 4Mb 的倍数。否则出错，死机
		panic("copy_page_tables called with wrong alignment");
	// 取得源地址和目的地址的目录项(from_dir 和 to_dir)	 分别是0和64
	from_dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */
	to_dir = (unsigned long *) ((to>>20) & 0xffc);
	size = ((unsigned) (size+0x3fffff)) >> 22;     // 计算要复制的内存块占用的页表数  4M（1个页目录项管理的页面大小 = 1024*4K）的数量
	for( ; size-->0 ; from_dir++,to_dir++) {
		if (1 & *to_dir)                 // 如果目的目录项指定的页表已经存在，死机
			panic("copy_page_tables: already exist");
		if (!(1 & *from_dir))            // 如果源目录项未被使用，不用复制，跳过
			continue;
		from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
		if (!(to_page_table = (unsigned long *) get_free_page()))     // // 为目的页表取一页空闲内存. 关键！！是目的页表存储的地址
			return -1;	/* Out of memory, see freeing */
		*to_dir = ((unsigned long) to_page_table) | 7;    // 设置目的目录项信息。7 是标志信息，表示(Usr, R/W, Present)
		nr = (from==0)?0xA0:1024;              // 如果是进程0复制给进程1，则复制160个页面；否则将1024个页面全部复制
		for ( ; nr-- > 0 ; from_page_table++,to_page_table++) {    
			this_page = *from_page_table;     // 复制！
			if (!(1 & this_page))
				continue;
			this_page &= ~2;         // 010， 代表用户，只读，存在
			*to_page_table = this_page;       // 复制！
			if (this_page > LOW_MEM) {    // 如果该页表项所指页面的地址在 1M 以上，则需要设置内存页面映射数组 mem_map[]
				*from_page_table = this_page;
				this_page -= LOW_MEM;
				this_page >>= 12;
				mem_map[this_page]++;
			}
		}
	}
	invalidate();     // 刷新页变换高速缓冲
	return 0;
}

/*
 * This function puts a page in memory at the wanted address.
 * It returns the physical address of the page gotten, 0 if
 * out of memory (either when trying to access page-table or
 * page.)
 */
/**
 * @brief 在指定的物理地址处申请一个页面， 主要工作是在页目录和页表中设置指定页面的信息
 * 
 * @param page 物理地址中的内存偏移
 * @param address 逻辑地址（线性地址）
 * @return unsigned long 成功申请到的页面地址
 */
unsigned long put_page(unsigned long page,unsigned long address)
{
	unsigned long tmp, *page_table;

/* NOTE !!! This uses the fact that _pg_dir=0 */

	if (page < LOW_MEM || page >= HIGH_MEMORY)
		printk("Trying to put page %p at %p\n",page,address);
	if (mem_map[(page-LOW_MEM)>>12] != 1)
		printk("mem_map disagrees with %p at %p\n",page,address);
	page_table = (unsigned long *) ((address>>20) & 0xffc);
	if ((*page_table)&1)			// 如果该目录项有效(P=1)(也即指定的页表在内存中)
		page_table = (unsigned long *) (0xfffff000 & *page_table);
	else {
		if (!(tmp=get_free_page()))
			return 0;
		*page_table = tmp|7;		// 否则，申请空闲页面给页表使用，并在对应目录项中置相应标志 7（User, U/S, R/W）。然后将 该页表的地址->page_table
		page_table = (unsigned long *) tmp;
	}
	page_table[(address>>12) & 0x3ff] = page | 7;		// 在页表中设置指定地址页面的页表项内容。每个页表共可有 1024 项(0x3ff)
/* no need for invalidate */
	return page;
}

/**
 * @brief 取消写保护页面（undo write protect page)，即写时复制
 * 
 * @param table_entry 页表项指针
 */
void un_wp_page(unsigned long * table_entry)
{
	unsigned long old_page,new_page;

	old_page = 0xfffff000 & *table_entry;		// 取原页面地址
	// 如果原页面地址大于内存低端 LOW_MEM(1Mb)并且其在页面映射字节数组中值为 1（表示仅被 
 	// 引用 1 次，页面没有被共享），则在该页面的页表项中置 R/W 标志（可写），并刷新页变换 
	// 高速缓冲，然后返回。
	if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)]==1) {
		*table_entry |= 2;
		invalidate();
		return;
	}
	// 否则，申请一页空闲页面
	if (!(new_page=get_free_page()))
		oom();
	// 如果原页面大于内存低端（则意味着 mem_map[]>1，页面是共享的），则将原页面的页面映射 
 	// 数组值递减 1。然后将指定页表项内容更新为新页面的地址，并置可读写等标志(U/S, R/W, P)。 
 	// 刷新页变换高速缓冲。最后将原页面内容复制到新页面。	
	if (old_page >= LOW_MEM)
		mem_map[MAP_NR(old_page)]--;
	*table_entry = new_page | 7;
	invalidate();
	copy_page(old_page,new_page);
}	

/*
 * This routine handles present pages, when users try to write
 * to a shared page. It is done by copying the page to a new address
 * and decrementing the shared-page counter for the old page.
 *
 * If it's in code space we exit with a segment error.
 */
/**
 * @brief 页面异常中断处理调用函数，即写共享页面，需要取消写保护，并进行写时复制
 *        它是通过将页面复制到一个新地址上并递减原页面的共享页面计数值实现的
 * 
 * @param error_code  CPU 自动产生的错误码
 * @param address 页面线性地址
 */
void do_wp_page(unsigned long error_code,unsigned long address)
{
#if 0
/* we cannot do this yet: the estdio library writes to code space */
/* stupid, stupid. I really want the libc.a from GNU */
	if (CODE_SPACE(address))
		do_exit(SIGSEGV);
#endif

	// 处理取消页面保护。参数指定页面在页表中的页表项指针，其计算方法是： 
 	// ((address>>10) & 0xffc)：计算指定地址的页面在页表中的偏移地址； 
 	// (0xfffff000 &((address>>20) &0xffc))：取目录项中页表的地址值， 
 	// 其中((address>>20) &0xffc)计算页面所在页表的目录项指针； 
 	// 两者相加即得指定地址对应页面的页表项指针
	un_wp_page((unsigned long *)
		(((address>>10) & 0xffc) + (0xfffff000 &
		*((unsigned long *) ((address>>20) &0xffc)))));

}

//// 写页面验证。若可写则直接写，否则写时复制
void write_verify(unsigned long address)
{
	unsigned long page;

	if (!( (page = *((unsigned long *) ((address>>20) & 0xffc)) )&1))
		return;
	page &= 0xfffff000;
	page += ((address>>10) & 0xffc);
	// 如果该页面不可写(标志 R/W 没有置位)，则执行共享检验和复制页面操作（写时复制）
	if ((3 & *(unsigned long *) page) == 1)  /* non-writeable, present */
		un_wp_page((unsigned long *) page);
	return;
}

//// 取指定地址的空页面。 
// 与 get_free_page()不同。get_free_page()仅置内存页面映射数组 mem_map[]中的引用标志。 
// 而这里 get_empty_page()不仅是取指定地址处的页面，还进一步调用 put_page()，将页面信息 
// 添加到页目录和页表中。
void get_empty_page(unsigned long address)
{
	unsigned long tmp;

	if (!(tmp=get_free_page()) || !put_page(tmp,address)) {
		free_page(tmp);		/* 0 is ok - ignored */
		oom();
	}
}

/*
 * try_to_share() checks the page at address "address" in the task "p",
 * to see if it exists, and if it is clean. If so, share it with the current
 * task.
 *
 * NOTE! This assumes we have checked that p != current, and that they
 * share the same executable.
 */
/**
 * @brief 在任务p中检查位于地址address处的页面，看页面是否存在，是否干净。
 *        如果是干净的话，就与当前任务共享 
 * 
 * @param address 指定的内存地址
 * @param p 指定的任务
 * @return int 1-成功，0-失败
 */
static int try_to_share(unsigned long address, struct task_struct * p)
{
	unsigned long from;
	unsigned long to;
	unsigned long from_page;
	unsigned long to_page;
	unsigned long phys_addr;

	from_page = to_page = ((address>>20) & 0xffc);		// 求指定内存地址的页目录项
	from_page += ((p->start_code>>20) & 0xffc);			// 计算地址在指定进程 p 中对应的页目录项
	to_page += ((current->start_code>>20) & 0xffc);		// 计算地址在当前进程中对应的页目录项
/* is there a page-directory at from? */
	from = *(unsigned long *) from_page;
	if (!(from & 1))		// 如果该目录项无效(P=0)
		return 0;
	from &= 0xfffff000;		// 则取该目录项对应页表地址
	from_page = from + ((address>>10) & 0xffc);		// 计算地址对应的页表项指针值
	phys_addr = *(unsigned long *) from_page;
/* is the page clean and present? */
	if ((phys_addr & 0x41) != 0x01)
		return 0;
	phys_addr &= 0xfffff000;
	if (phys_addr >= HIGH_MEMORY || phys_addr < LOW_MEM)
		return 0;
	to = *(unsigned long *) to_page;
	if (!(to & 1))
		if (to = get_free_page())
			*(unsigned long *) to_page = to | 7;
		else
			oom();
	to &= 0xfffff000;
	to_page = to + ((address>>10) & 0xffc);
	if (1 & *(unsigned long *) to_page)
		panic("try_to_share: to_page already exists");
/* share them: write-protect */
	*(unsigned long *) from_page &= ~2;		// 对 p 进程中页面置写保护标志(置 R/W=0)。并且当前进程中的对应页表项指向它
	*(unsigned long *) to_page = *(unsigned long *) from_page;
	invalidate();
	// 计算所操作页面的页面号，并将对应页面映射数组项中的引用递增 1
	phys_addr -= LOW_MEM;
	phys_addr >>= 12;
	mem_map[phys_addr]++;
	return 1;
}

/*
 * share_page() tries to find a process that could share a page with
 * the current one. Address is the address of the wanted page relative
 * to the current data space.
 *
 * We first check if it is at all feasible by checking executable->i_count.
 * It should be >1 if there are other tasks sharing this inode.
 */
/**
 * @brief 识图找到一个进程，它可以与当前进程共享页面
 * 
 * @param address 期望共享的某页面地址
 * @return int 1-成功，0-失败
 */
static int share_page(unsigned long address)
{
	struct task_struct ** p;

	if (!current->executable)
		return 0;
	if (current->executable->i_count < 2)
		return 0;
	for (p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
		if (!*p)
			continue;
		if (current == *p)
			continue;
		if ((*p)->executable != current->executable)
			continue;
		if (try_to_share(address,*p))
			return 1;
	}
	return 0;
}

/**
 * @brief 缺页异常处理函数
 * 
 * @param error_code CPU自动产生的错误码
 * @param address 页面线性地址
 */
void do_no_page(unsigned long error_code,unsigned long address)
{
	int nr[4];
	unsigned long tmp;
	unsigned long page;
	int block,i;

	address &= 0xfffff000;		// 页面地址
	tmp = address - current->start_code;
	if (!current->executable || tmp >= current->end_data) {
		get_empty_page(address);
		return;
	}
	if (share_page(tmp))
		return;
	if (!(page = get_free_page()))
		oom();
/* remember that 1 block is used for header */
	block = 1 + tmp/BLOCK_SIZE;
	for (i=0 ; i<4 ; block++,i++)
		nr[i] = bmap(current->executable,block);
	bread_page(page,current->executable->i_dev,nr);
	i = tmp + 4096 - current->end_data;
	tmp = page + 4096;
	while (i-- > 0) {
		tmp--;
		*(char *)tmp = 0;
	}
	if (put_page(page,address))
		return;
	free_page(page);
	oom();
}

// 假定内存为16M
// start_mem = 4 * 1024 * 1024
// end_mem = 16 * 1024 * 1024
// PAGING_PAGES = 15*1024*1024 >> 12   mem_map 大小为3k，每一项映射一个4k的页面，数组较大，用以适配不同的内存大小
// 注意mem_map支队1MB以上的内存分页进行管理
// 将0~2M的内存标记为USED， 剩下的清零
void mem_init(long start_mem, long end_mem)
{
	int i;

	HIGH_MEMORY = end_mem;
	for (i=0 ; i<PAGING_PAGES ; i++)
		mem_map[i] = USED;
	i = MAP_NR(start_mem);
	end_mem -= start_mem;
	end_mem >>= 12;
	while (end_mem-->0)
		mem_map[i++]=0;
}

// 计算内存空闲页面数并显示
void calc_mem(void)
{
	int i,j,k,free=0;
	long * pg_tbl;

	for(i=0 ; i<PAGING_PAGES ; i++)
		if (!mem_map[i]) free++;
	printk("%d pages free (of %d)\n\r",free,PAGING_PAGES);
	for(i=2 ; i<1024 ; i++) {
		if (1&pg_dir[i]) {
			pg_tbl=(long *) (0xfffff000 & pg_dir[i]);
			for(j=k=0 ; j<1024 ; j++)
				if (pg_tbl[j]&1)
					k++;
			printk("Pg-dir[%d] uses %d pages\n",i,k);
		}
	}
}
