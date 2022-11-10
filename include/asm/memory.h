/*
 *  NOTE!!! memcpy(dest,src,n) assumes ds=es=normal data segment. This
 *  goes for all kernel functions (ds=es=kernel space, fs=local data,
 *  gs=null), as well as for all well-behaving user programs (ds=es=
 *  user data space). This is NOT a bug, as any user program that changes
 *  es deserves to die if it isn't careful.
 */
//// 内存块复制。从源地址 src 处开始复制 n 个字节到目的地址 dest 处。 
 // 参数：dest - 复制的目的地址，src - 复制的源地址，n - 复制字节数。 
 // %0 - edi(目的地址 dest)，%1 - esi(源地址 src)，%2 - ecx(字节数 n)
#define memcpy(dest,src,n) ({ \
void * _res = dest; \
__asm__ ("cld;rep;movsb" \
	::"D" ((long)(_res)),"S" ((long)(src)),"c" ((long) (n)) \
	:"di","si","cx"); \
_res; \
})
