#ifndef _ERRNO_H
#define _ERRNO_H
// 错误类型

/*
 * ok, as I hadn't got any other source of information about
 * possible error numbers, I was forced to use the same numbers
 * as minix.
 * Hopefully these are posix or something. I wouldn't know (and posix
 * isn't telling me - they want $$$ for their f***ing standard).
 *
 * We don't use the _SIGN cludge of minix, so kernel returns must
 * see to the sign by themselves.
 *
 * NOTE! Remember to change strerror() if you change this file!
 */

extern int errno;

#define ERROR		99    // 一般错误
#define EPERM		 1    // 操作没有得到许可， not permitted
#define ENOENT		 2    // 文件或目录不存在， no content
#define ESRCH		 3    // 指定的进程不存在， no such process
#define EINTR		 4    // 终端的函数调用， interrupt
#define EIO		 5        // IO error
#define ENXIO		 6    // 指定设备或地址不存在， no such io
#define E2BIG		 7    // 参数列表太长， too big
#define ENOEXEC		 8    // 执行程序格式错误， no exec    
#define EBADF		 9    // 文件句柄错误， bad file descriptor
#define ECHILD		10    // 子进程不存在， child
#define EAGAIN		11    // 资源暂时不可用， try again
#define ENOMEM		12    // 内存不足， no memory
#define EACCES		13    // 没有许可权限， access
#define EFAULT		14    // 地址错误， bad address（fault）
#define ENOTBLK		15    // 不是块设备文件， not block device
#define EBUSY		16    // 资源正忙， busy
#define EEXIST		17    // 文件已经存在， file already exist
#define EXDEV		18    // 非法链接， cross device link
#define ENODEV		19    // 设备不存在， no device
#define ENOTDIR		20    // 不是目录文件， not directory
#define EISDIR		21    // 是目录文件， is directory
#define EINVAL		22    // 参数无效， invalid parameter
#define ENFILE		23    // 系统打开文件数太多， File table overflow
#define EMFILE		24    // 打开文件数太多， too many open files
#define ENOTTY		25    // 不恰当的IO控制操作， not a tty device
#define ETXTBSY		26    // 文件正在使用， txt file busy
#define EFBIG		27    // 文件太大， file too big
#define ENOSPC		28    // 设备已满， no space
#define ESPIPE		29    // 无效的文件指针重定位， illegal seek
#define EROFS		30    // 文件系统只读， read only file system
#define EMLINK		31    // 链接太多， too many links
#define EPIPE		32    // 管道错, broken pipe
#define EDOM		33    // 域出错， math argument out of domain
#define ERANGE		34    // 结果太大， math result not representable
#define EDEADLK		35    // 资源死锁， dead lock 
#define ENAMETOOLONG	36     // 文件名太长
#define ENOLCK		37    // 没有锁可用， no record lock avaliable
#define ENOSYS		38    // 功能没有实现， function not implimented(not in system)
#define ENOTEMPTY	39    // 目录不空, not empty

#endif
