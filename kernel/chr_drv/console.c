/*
 *  linux/kernel/console.c
 *
 *  (C) 1991  Linus Torvalds
 */

/*
 *	console.c
 *
 * This module implements the console io functions
 *	'void con_init(void)'
 *	'void con_write(struct tty_queue * queue)'
 * Hopefully this will be a rather complete VT102 implementation.
 *
 * Beeping thanks to John T Kohl.
 */

/* 
 * 该模块实现控制台输入输出功能 
 * 'void con_init(void)' 
 * 'void con_write(struct tty_queue * queue)' 
 * 希望这是一个非常完整的 VT102 实现。 
 * 
 * 感谢 John T Kohl 实现了蜂鸣指示。 
 */

/*
 *  NOTE!!! We sometimes disable and enable interrupts for a short while
 * (to put a word in video IO), but this will work even for keyboard
 * interrupts. We know interrupts aren't enabled when getting a keyboard
 * interrupt, as we use trap-gates. Hopefully all is well.
 */

/*
 * Code to check for different video-cards mostly by Galen Hunt,
 * <g-hunt@ee.utah.edu>
 */

#include <linux/sched.h>
#include <linux/tty.h>
#include <asm/io.h>
#include <asm/system.h>

/*
 * These are set up by the setup-routine at boot-time:
 */

#define ORIG_X			(*(unsigned char *)0x90000)     					// 光标列号。
#define ORIG_Y			(*(unsigned char *)0x90001)     					// 光标行号。
#define ORIG_VIDEO_PAGE		(*(unsigned short *)0x90004)       				// 显示页面
#define ORIG_VIDEO_MODE		((*(unsigned short *)0x90006) & 0xff)    		// 显示模式。
#define ORIG_VIDEO_COLS 	(((*(unsigned short *)0x90006) & 0xff00) >> 8)  // 字符列数。
#define ORIG_VIDEO_LINES	(25)                                            // 显示行数。
#define ORIG_VIDEO_EGA_AX	(*(unsigned short *)0x90008)                  
#define ORIG_VIDEO_EGA_BX	(*(unsigned short *)0x9000a)    				// 显示内存大小和色彩模式
#define ORIG_VIDEO_EGA_CX	(*(unsigned short *)0x9000c)    				// 显示卡特性参数

#define VIDEO_TYPE_MDA		0x10	/* Monochrome Text Display	*/   /* 单色文本 */
#define VIDEO_TYPE_CGA		0x11	/* CGA Display 			*/  /* CGA 显示器 */
#define VIDEO_TYPE_EGAM		0x20	/* EGA/VGA in Monochrome Mode	*/
#define VIDEO_TYPE_EGAC		0x21	/* EGA/VGA in Color Mode	*/

#define NPAR 16

extern void keyboard_interrupt(void);

static unsigned char	video_type;		/* Type of display being used 使用的显示类型	*/
static unsigned long	video_num_columns;	/* Number of text columns	*/    /* 屏幕文本列数 */
static unsigned long	video_size_row;		/* Bytes per row		*/
static unsigned long	video_num_lines;	/* Number of test lines		*/  /* 屏幕文本行数 */
static unsigned char	video_page;		/* Initial video page		*/
static unsigned long	video_mem_start;	/* Start of video RAM		*/
static unsigned long	video_mem_end;		/* End of video RAM (sort of)	*/
static unsigned short	video_port_reg;		/* Video register select port	*/
static unsigned short	video_port_val;		/* Video register value port	*/
static unsigned short	video_erase_char;	/* Char+Attrib to erase with	*/
                                            /* 擦除字符属性与字符(0x0720) */
											// 0x20是要显示的字符，0x20对应的字符是空格，0x07对应黑底白字显示方式，
											// 以黑底白色的方式显示空格那就是一个黑色块

// 以下这些变量用于屏幕卷屏操作。
static unsigned long	origin;		/* Used for EGA/VGA fast scroll	*/  /* 用于 EGA/VGA 快速滚屏 */ // 滚屏起始内存地址。
static unsigned long	scr_end;	/* Used for EGA/VGA fast scroll	*/
static unsigned long	pos;        // 当前光标对应的显示内存位置。
static unsigned long	x,y;            // 当前光标位置。x表示列， y表示行，屏幕左上角为（0,0），右下方为第一象限
static unsigned long	top,bottom;    // 滚动时顶行行号；底行行号
// state 用于标明处理 ESC 转义序列时的当前步骤。npar,par[]用于存放 ESC 序列的中间处理参数
static unsigned long	state=0;
static unsigned long	npar,par[NPAR];
static unsigned long	ques=0;
static unsigned char	attr=0x07;   // 字符属性(黑底白字=0x07，其他还有加粗，反显等)

static void sysbeep(void);

/*
 * this is what the terminal answers to a ESC-Z or csi0c
 * query (= vt100 response).
 */
// csi 控制序列引导码
#define RESPONSE "\033[?1;2c"

/* NOTE! gotoxy thinks x==video_num_columns is ok */
// 前往新坐标，并更新光标对应现存位置
static inline void gotoxy(unsigned int new_x,unsigned int new_y)
{
	// 如果输入的光标行号超出显示器列数，或者光标行号超出显示的最大行数，则退出。
	if (new_x > video_num_columns || new_y >= video_num_lines)
		return;
	x=new_x;
	y=new_y;
	pos=origin + y*video_size_row + (x<<1);   //why x<<1???
}

//// 设置滚屏起始显示内存地址。
static inline void set_origin(void)
{
	cli();
	outb_p(12, video_port_reg);
	outb_p(0xff&((origin-video_mem_start)>>9), video_port_val);
	outb_p(13, video_port_reg);
	outb_p(0xff&((origin-video_mem_start)>>1), video_port_val);
	sti();
}

 //// 向上卷动一行（屏幕窗口向下移动）。 
 // 将屏幕窗口向下移动一行。   screen up
static void scrup(void)
{
	if (video_type == VIDEO_TYPE_EGAC || video_type == VIDEO_TYPE_EGAM)
	{
		if (!top && bottom == video_num_lines) {        // top=0, bottom=最底部，表示整屏窗口向下移动
			origin += video_size_row;
			pos += video_size_row;
			scr_end += video_size_row;
			if (scr_end > video_mem_end) {
				__asm__("cld\n\t"
					"rep\n\t"
					"movsl\n\t"
					"movl _video_num_columns,%1\n\t"
					"rep\n\t"
					"stosw"
					::"a" (video_erase_char),
					"c" ((video_num_lines-1)*video_num_columns>>1),
					"D" (video_mem_start),
					"S" (origin)
					:"cx","di","si");
				scr_end -= origin-video_mem_start;
				pos -= origin-video_mem_start;
				origin = video_mem_start;
			} else {                                     
				__asm__("cld\n\t"
					"rep\n\t"
					"stosw"
					::"a" (video_erase_char),
					"c" (video_num_columns),
					"D" (scr_end-video_size_row)
					:"cx","di");
			}
			set_origin();
		} else {                                           // 否则表示不是整屏移动
		// 从指定行top开始的所有行向上移动1行（删除1行）， 直接将屏幕从top到末端显示
		// 内存数据向上移动1行， 并在新出现的行上填入擦除字符
			__asm__("cld\n\t"                          // 清方向位。
				"rep\n\t"                              // 循环操作，将 top+1 到 bottom 行
				"movsl\n\t"                            // 所对应的内存块移到 top 行开始处。
				"movl _video_num_columns,%%ecx\n\t"    // ecx = 1 行字符数。
				"rep\n\t"                              // 在新行上填入擦除字符。
				"stosw"
				::"a" (video_erase_char),
				"c" ((bottom-top-1)*video_num_columns>>1),
				"D" (origin+video_size_row*top),
				"S" (origin+video_size_row*(top+1))
				:"cx","di","si");
		}
	}
 // 如果显示类型不是 EGA(是 MDA)，则执行下面移动操作。因为 MDA 显示控制卡会自动调整超出显示范围 
 // 的情况，也即会自动翻卷指针，所以这里不对屏幕内容对应内存超出显示内存的情况单独处理。处理 
 // 方法与 EGA 非整屏移动情况完全一样。
	else		/* Not EGA/VGA */
	{
		__asm__("cld\n\t"
			"rep\n\t"
			"movsl\n\t"
			"movl _video_num_columns,%%ecx\n\t"
			"rep\n\t"
			"stosw"
			::"a" (video_erase_char),
			"c" ((bottom-top-1)*video_num_columns>>1),
			"D" (origin+video_size_row*top),
			"S" (origin+video_size_row*(top+1))
			:"cx","di","si");
	}
}

 //// 向下卷动一行（屏幕窗口向上移动）。 
 // 将屏幕窗口向上移动一行，屏幕显示的内容向下移动 1 行，在被移动开始行的上方出现一新行
static void scrdown(void)
{
	if (video_type == VIDEO_TYPE_EGAC || video_type == VIDEO_TYPE_EGAM)
	{
		__asm__("std\n\t"
			"rep\n\t"
			"movsl\n\t"
			"addl $2,%%edi\n\t"	/* %edi has been decremented by 4 */
			"movl _video_num_columns,%%ecx\n\t"
			"rep\n\t"
			"stosw"
			::"a" (video_erase_char),
			"c" ((bottom-top-1)*video_num_columns>>1),
			"D" (origin+video_size_row*bottom-4),
			"S" (origin+video_size_row*(bottom-1)-4)
			:"ax","cx","di","si");
	}
	else		/* Not EGA/VGA */
	{
		__asm__("std\n\t"
			"rep\n\t"
			"movsl\n\t"
			"addl $2,%%edi\n\t"	/* %edi has been decremented by 4 */
			"movl _video_num_columns,%%ecx\n\t"
			"rep\n\t"
			"stosw"
			::"a" (video_erase_char),
			"c" ((bottom-top-1)*video_num_columns>>1),
			"D" (origin+video_size_row*bottom-4),
			"S" (origin+video_size_row*(bottom-1)-4)
			:"ax","cx","di","si");
	}
}

// 光标位置下移一行(lf - line feed 换行)。
static void lf(void)
{
	if (y+1<bottom) {
		y++;
		pos += video_size_row;
		return;
	}
	// 如果光标在bottom，需要将屏幕上移一行
	scrup();
}

/// 光标上移一行(ri - reverse line feed 反向换行)。
static void ri(void)
{
	if (y>top) {
		y--;
		pos -= video_size_row;
		return;
	}
	scrdown();
}

// 光标回到第 1 列(0 列)左端(cr - carriage return 回车)。  Home key
static void cr(void)
{
	pos -= x<<1;
	x=0;
}

// 擦除光标前一字符(用空格替代)(del - delete 删除)。
static void del(void)
{
	if (x) {
		// pos 后退 2 字节(对应屏幕上一个字符)，
		pos -= 2;
		x--;
		// 将光标所在位置字符擦除
		*(unsigned short *)pos = video_erase_char;
	}
}

 //// 删除屏幕上与光标位置相关的部分，以屏幕为单位。csi - 控制序列引导码(Control Sequence 
 // Introducer)。 
 // ANSI 转义序列：'ESC [sJ'(s = 0 删除光标到屏幕底端；1 删除屏幕开始到光标处；2 整屏删除)。
 // 参数：par - 对应上面 s。
static void csi_J(int par)
{
	long count __asm__("cx");
	long start __asm__("di");

	switch (par) {
		case 0:	/* erase from cursor to end of display */
			count = (scr_end-pos)>>1;
			start = pos;
			break;
		case 1:	/* erase from start to cursor */
			count = (pos-origin)>>1;
			start = origin;
			break;
		case 2: /* erase whole display */
			count = video_num_columns * video_num_lines;
			start = origin;
			break;
		default:
			return;
	}
 // 然后使用擦除字符填写删除字符的地方。 
 // %0 - ecx(要删除的字符数 count)；%1 - edi(删除操作开始地址)；%2 - eax（填入的擦除字符）。	
	__asm__("cld\n\t"
		"rep\n\t"
		"stosw\n\t"
		::"c" (count),
		"D" (start),"a" (video_erase_char)
		:"cx","di");
}

 //// 删除行内与光标位置相关的部分，以一行为单位。 
 // ANSI 转义字符序列：'ESC [sK'(s = 0 删除到行尾；1 从开始删除；2 整行都删除)。
static void csi_K(int par)
{
	long count __asm__("cx");
	long start __asm__("di");

	switch (par) {
		case 0:	/* erase from cursor to end of line */
			if (x>=video_num_columns)
				return;
			count = video_num_columns-x;
			start = pos;
			break;
		case 1:	/* erase from start of line to cursor */
			start = pos - (x<<1);
			count = (x<video_num_columns)?x:video_num_columns;
			break;
		case 2: /* erase whole line */
			start = pos - (x<<1);
			count = video_num_columns;
			break;
		default:
			return;
	}
	__asm__("cld\n\t"
		"rep\n\t"
		"stosw\n\t"
		::"c" (count),
		"D" (start),"a" (video_erase_char)
		:"cx","di");
}

 //// 允许翻译(重显)（允许重新设置字符显示方式，比如加粗、加下划线、闪烁、反显等）。 
//  设置字体格式，但是不包括颜色
 // ANSI 转义字符序列：'ESC [nm'。n = 0 正常显示；1 加粗；4 加下划线；7 反显；27 正常显示。
void csi_m(void)
{
	int i;

	for (i=0;i<=npar;i++)
		switch (par[i]) {
			case 0:attr=0x07;break;
			case 1:attr=0x0f;break;
			case 4:attr=0x0f;break;
			case 7:attr=0x70;break;
			case 27:attr=0x07;break;
		}
}

 //// 根据设置显示光标。 
 // 根据显示内存光标对应位置 pos，设置显示控制器光标的显示位置。
static inline void set_cursor(void)
{
	cli();
	outb_p(14, video_port_reg);
	outb_p(0xff&((pos-video_mem_start)>>9), video_port_val);
	outb_p(15, video_port_reg);
	outb_p(0xff&((pos-video_mem_start)>>1), video_port_val);
	sti();
}

 //// 发送对终端 VT100 的响应序列。 
 // 将响应序列放入读缓冲队列中。
static void respond(struct tty_struct * tty)
{
	char * p = RESPONSE;

	cli();
	while (*p) {
		PUTCH(*p,tty->read_q);
		p++;
	}
	sti();
	copy_to_cooked(tty);   // 转换成规范模式(放入辅助队列中)。
}

 //// 在光标处插入一字符
static void insert_char(void)
{
	int i=x;
	unsigned short tmp, old = video_erase_char;
	unsigned short * p = (unsigned short *) pos;

// 光标开始的所有字符右移一格，并将擦除字符插入在光标所在处。 
 // 若一行上都有字符的话，则行最后一个字符将不会更动☺?
	while (i++<video_num_columns) {
		tmp=*p;
		*p=old;
		old=tmp;
		p++;
	}
}

//// 在光标处插入一行（则光标将处在新的空行上）。 
 // 将屏幕从光标所在行到屏幕底向下卷动一行。
static void insert_line(void)
{
	int oldtop,oldbottom;

	oldtop=top;
	oldbottom=bottom;
	top=y;
	bottom = video_num_lines;
	scrdown();
	top=oldtop;
	bottom=oldbottom;
}

//// 删除光标处的一个字符。
static void delete_char(void)
{
	int i;
	unsigned short * p = (unsigned short *) pos;

	if (x>=video_num_columns)
		return;
	i = x;
	while (++i < video_num_columns) {
		*p = *(p+1);
		p++;
	}
	*p = video_erase_char;
}

//// 删除光标所在行。 
 // 从光标所在行开始屏幕内容上卷一行
static void delete_line(void)
{
	int oldtop,oldbottom;

	oldtop=top;
	oldbottom=bottom;
	top=y;
	bottom = video_num_lines;
	scrup();
	top=oldtop;
	bottom=oldbottom;
}

 //// 在光标处插入 nr 个字符。 
 // ANSI 转义字符序列：'ESC [n@ '。 
 // 参数 nr = 上面 n。
static void csi_at(unsigned int nr)
{
	if (nr > video_num_columns)
		nr = video_num_columns;
	else if (!nr)
		nr = 1;
	while (nr--)
		insert_char();
}

 //// 在光标位置处插入 nr 行。 
 // ANSI 转义字符序列'ESC [nL'。
static void csi_L(unsigned int nr)
{
	if (nr > video_num_lines)
		nr = video_num_lines;
	else if (!nr)
		nr = 1;
	while (nr--)
		insert_line();
}

 //// 删除光标处的 nr 个字符。 
 // ANSI 转义序列：'ESC [nP'。
static void csi_P(unsigned int nr)
{
	if (nr > video_num_columns)
		nr = video_num_columns;
	else if (!nr)
		nr = 1;
	while (nr--)
		delete_char();
}

 //// 删除光标处的 nr 行。 
 // ANSI 转义序列：'ESC [nM'。
static void csi_M(unsigned int nr)
{
	if (nr > video_num_lines)
		nr = video_num_lines;
	else if (!nr)
		nr=1;
	while (nr--)
		delete_line();
}

static int saved_x=0;   // 保存的光标列号。
static int saved_y=0;   // 保存的光标行号。

//// 保存当前光标位置。
static void save_cur(void)
{
	saved_x=x;
	saved_y=y;
}

//// 恢复保存的光标位置。
static void restore_cur(void)
{
	gotoxy(saved_x, saved_y);
}

 //// 控制台写函数。 
 // 从终端对应的 tty 写缓冲队列中取字符，并显示在屏幕上
void con_write(struct tty_struct * tty)
{
	int nr;
	char c;

    // 首先取得写缓冲队列中现有字符数 nr，然后针对每个字符进行处理。  nr表示字符个数
	nr = CHARS(tty->write_q);
	while (nr--) {
 // 从写队列中取一字符 c，根据前面所处理字符的状态 state 分别处理。状态之间的转换关系为： 
 // state = 0：初始状态；或者原始状态 4；或者原始状态 1，但字符不是'['； 
         // 1：原始状态 0，并且字符是转义字符 ESC(0x1b = 033 = 27)； 
         // 2：原始状态 1，并且字符是'['； 
         // 3：原始状态 2；或者原始状态 3，并且字符是';'或数字。 
         // 4：原始状态 3，并且字符不是';'或数字；	
		GETCH(tty->write_q,c);
		switch(state) {
			case 0:
				if (c>31 && c<127) {     //可打印字符正常输出
					if (x>=video_num_columns) {
						x -= video_num_columns;
						pos -= video_size_row;
						lf();
					}
					__asm__("movb _attr,%%ah\n\t"
						"movw %%ax,%1\n\t"
						::"a" (c),"m" (*(short *)pos)
						:"ax");
					pos += 2;
					x++;
				} else if (c==27)        // 如果字符 c 是转义字符 ESC，则转换状态 state 到 1
					state=1;
				else if (c==10 || c==11 || c==12)    // 如果字符 c 是换行符(10)，或是垂直制表符 VT(11)，或者是换页符 FF(12)，则移动光标到下一行
					lf();
				else if (c==13)      // 如果字符 c 是回车符 CR(13)，则将光标移动到头列(0 列)
					cr();
				else if (c==ERASE_CHAR(tty))     // 如果字符 c 是 DEL(127)
					del();
				else if (c==8) {      // 如果字符 c 是 BS(backspace,8)
					if (x) {
						x--;
						pos -= 2;
					}
				} else if (c==9) {    // 如果字符 c 是水平制表符 TAB(9)
					c=8-(x&7);
					x += c;
					pos += c<<1;
					if (x>video_num_columns) {
						x -= video_num_columns;
						pos -= video_size_row;
						lf();
					}
					c=9;    //reset c
				} else if (c==7)     // 如果字符 c 是响铃符 BEL(7)
					sysbeep();
				break;
			case 1:
				state=0;
				if (c=='[')               // 如果字符 c 是'['，则将状态 state 转到 2
					state=2;
				else if (c=='E')         // 如果字符 c 是'E'，则光标移到下一行开始处(0 列)
					gotoxy(0,y+1);
				else if (c=='M')         // 如果字符 c 是'M'，则光标上移一行
					ri();
				else if (c=='D')         // 如果字符 c 是'D'，则光标下移一行
					lf();
				else if (c=='Z')         // 如果字符 c 是'Z'，则发送终端应答字符序列   
					respond(tty);
				else if (x=='7')         // 如果字符 c 是'7'，则保存当前光标位置, 注意这里代码写错！应该是(c=='7')
					save_cur();
				else if (x=='8')         // 如果字符 c 是'8'，则恢复到原保存的光标位置。注意这里代码写错！应该是(c=='8')
					restore_cur();
				break;
			case 2:
 // 首先对 ESC 转义字符序列参数使用的处理数组 par[]清零，索引变量 npar 指向首项，并且设置状态 
 // 为 3。若此时字符不是'?'，则直接转到状态 3 去处理，否则去读一字符，再到状态 3 处理代码处。
				for(npar=0;npar<NPAR;npar++)
					par[npar]=0;
				npar=0;
				state=3;
				if (ques=(c=='?'))
					break;
			case 3:
				if (c==';' && npar<NPAR-1) {
					npar++;
					break;
				} else if (c>='0' && c<='9') {
					par[npar]=10*par[npar]+c-'0';
					break;
				} else state=4;
			case 4:
				state=0;
				switch(c) {
					case 'G': case '`':                     // CSI n G; 若n=0, 光标右移一格
						if (par[0]) par[0]--;
						gotoxy(par[0],y);
						break;
					case 'A':                              // CSI n A; 若n=0,光标上移一行
						if (!par[0]) par[0]++;
						gotoxy(x,y-par[0]);
						break;
					case 'B': case 'e':                    // CSI n B; cursor down
						if (!par[0]) par[0]++;
						gotoxy(x,y+par[0]);
						break;
					case 'C': case 'a':                    // CSI n C; cursor forward
						if (!par[0]) par[0]++;
						gotoxy(x+par[0],y);
						break;
					case 'D':                              // CSI n D; cursor back
						if (!par[0]) par[0]++;
						gotoxy(x-par[0],y);
						break;
					case 'E':                              // CSI n E; cursor next line
						if (!par[0]) par[0]++;
						gotoxy(0,y+par[0]);
						break;
					case 'F':                             // CSI n F; cursor previous line
						if (!par[0]) par[0]++;
						gotoxy(0,y-par[0]);
						break;
					case 'd':                            // CSI n d; cursor jump to line n
						if (par[0]) par[0]--;
						gotoxy(x,par[0]);
						break;
					case 'H': case 'f':                  // CSI n ; m H;  cursor jump to line n, col m
						if (par[0]) par[0]--;
						if (par[1]) par[1]--;
						gotoxy(par[1],par[0]);
						break;
					case 'J':                            // CSI n J;  Erase in Display
						csi_J(par[0]);
						break;
					case 'K':                           // CSI n K; erase in line
						csi_K(par[0]);
						break;
					case 'L':                           // CSI n L; insert lines
						csi_L(par[0]);
						break;
					case 'M':                           // CSI n M; del n lines
						csi_M(par[0]);
						break;
					case 'P':                           // CSI n P; del n characters 
						csi_P(par[0]);
						break;
					case '@':                           // CSI n @; insert n characters
						csi_at(par[0]);
						break;
					case 'm':                           // CSI n m; Select Graphic Rendition, 改变字符显示属性
						csi_m();
						break;
					case 'r':                           // 如果字符 c 是'r'，则表示用两个参数设置滚屏的起始行号和终止行号
						if (par[0]) par[0]--;
						if (!par[1]) par[1] = video_num_lines;
						if (par[0] < par[1] &&
						    par[1] <= video_num_lines) {
							top=par[0];
							bottom=par[1];
						}
						break;
					case 's':                          // 如果字符 c 是's'，则表示保存当前光标所在位置
						save_cur();
						break;
					case 'u':                          // 如果字符 c 是'u'，则表示恢复光标到原保存的位置处
						restore_cur();
						break;
				}
		}
	}
	set_cursor();
}

/*
 *  void con_init(void);
 *
 * This routine initalizes console interrupts, and does nothing
 * else. If you want the screen to clear, call tty_write with
 * the appropriate escape-sequece.
 *
 * Reads the information preserved by setup.s to determine the current display
 * type and sets everything accordingly.
 */
void con_init(void)
{
	register unsigned char a;
	char *display_desc = "????";
	char *display_ptr;
                                                      // 第一部分 获取显示模式相关信息
	video_num_columns = ORIG_VIDEO_COLS;
	video_size_row = video_num_columns * 2;
	video_num_lines = ORIG_VIDEO_LINES;
	video_page = ORIG_VIDEO_PAGE;       // 当前显示页面
	video_erase_char = 0x0720;
	
	if (ORIG_VIDEO_MODE == 7)			/* Is this a monochrome display? */
	{
		video_mem_start = 0xb0000;
		video_port_reg = 0x3b4;
		video_port_val = 0x3b5;
		if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10)
		{
			video_type = VIDEO_TYPE_EGAM;
			video_mem_end = 0xb8000;
			display_desc = "EGAm";
		}
		else
		{
			video_type = VIDEO_TYPE_MDA;
			video_mem_end	= 0xb2000;
			display_desc = "*MDA";
		}
	}
	else								/* If not, it is color. */
	{
		                                               // 第二部分 显存映射的内存区域
		video_mem_start = 0xb8000;
		video_port_reg	= 0x3d4;
		video_port_val	= 0x3d5;
		if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10)
		{
			video_type = VIDEO_TYPE_EGAC;
			video_mem_end = 0xbc000;
			display_desc = "EGAc";
		}
		else
		{
			video_type = VIDEO_TYPE_CGA;
			video_mem_end = 0xba000;
			display_desc = "*CGA";
		}
	}

	/* Let the user known what kind of display driver we are using */
	// 在屏幕的右上角显示显示描述字符串。采用的方法是直接将字符串写到显示内存的相应位置处。 
 // 首先将显示指针 display_ptr 指到屏幕第一行右端差 4 个字符处(每个字符需 2 个字节，因此减 8)
	display_ptr = ((char *)video_mem_start) + video_size_row - 8;
	while (*display_desc)
	{
		*display_ptr++ = *display_desc++;
		display_ptr++;
	}
	
	/* Initialize the variables used for scrolling (mostly EGA/VGA)	*/
                                                         // 第三部分 滚动屏幕操作时的信息
	origin	= video_mem_start;
	scr_end	= video_mem_start + video_num_lines * video_size_row;
	top	= 0;
	bottom	= video_num_lines;
                                                         // 第四部分 定位光标并开启键盘中断
	gotoxy(ORIG_X,ORIG_Y);
	set_trap_gate(0x21,&keyboard_interrupt);
	outb_p(inb_p(0x21)&0xfd,0x21);
	a=inb_p(0x61);
	outb_p(a|0x80,0x61);
	outb(a,0x61);
}
/* from bsd-net-2: */

//// 停止蜂鸣
void sysbeepstop(void)
{
	/* disable counter 2 */
	outb(inb_p(0x61)&0xFC, 0x61);
}

int beepcount = 0;

static void sysbeep(void)
{
	/* enable counter 2 */
	outb_p(inb_p(0x61)|3, 0x61);
	/* set command for counter 2, 2 byte write */
	outb_p(0xB6, 0x43);
	/* send 0x637 for 750 HZ */
	outb_p(0x37, 0x42);
	outb(0x06, 0x42);
	/* 1/8 second */
	beepcount = HZ/8;	
}
