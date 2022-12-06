#ifndef _CTYPE_H
#define _CTYPE_H
// 字符串类型

#define _U	0x01	/* upper */        // 大写
#define _L	0x02	/* lower */        // 小写
#define _D	0x04	/* digit */        // 数字
#define _C	0x08	/* cntrl */        // 该比特位用于控制字符
#define _P	0x10	/* punct */        // 该比特位用于标点字符
#define _S	0x20	/* white space (space/lf/tab) */        // 用于空白字符，如空格、\t、\n 等
#define _X	0x40	/* hex digit */                     // 该比特位用于十六进制数字
#define _SP	0x80	/* hard space (0x20) */              // 该比特位用于空格字符(0x20)

extern unsigned char _ctype[];
extern char _ctmp;

#define isalnum(c) ((_ctype+1)[c]&(_U|_L|_D))
#define isalpha(c) ((_ctype+1)[c]&(_U|_L))
#define iscntrl(c) ((_ctype+1)[c]&(_C))
#define isdigit(c) ((_ctype+1)[c]&(_D))
#define isgraph(c) ((_ctype+1)[c]&(_P|_U|_L|_D))
#define islower(c) ((_ctype+1)[c]&(_L))
#define isprint(c) ((_ctype+1)[c]&(_P|_U|_L|_D|_SP))
#define ispunct(c) ((_ctype+1)[c]&(_P))
#define isspace(c) ((_ctype+1)[c]&(_S))
#define isupper(c) ((_ctype+1)[c]&(_U))
#define isxdigit(c) ((_ctype+1)[c]&(_D|_X))

#define isascii(c) (((unsigned) c)<=0x7f)
#define toascii(c) (((unsigned) c)&0x7f)

#define tolower(c) (_ctmp=c,isupper(_ctmp)?_ctmp-('A'-'a'):_ctmp)
#define toupper(c) (_ctmp=c,islower(_ctmp)?_ctmp-('a'-'A'):_ctmp)

#endif
