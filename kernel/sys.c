/*
 *  linux/kernel/sys.c
 *
 *  (C) 1991  Linus Torvalds
 */
// sys.c 程序主要包含有各个系统调用功能的实现函数。其中，若返回值为-ENOSYS，则表示本版的 linux
// 还没有实现该功能，可以参考目前的代码来了解它们的实现方法。

#include <errno.h>

#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <asm/segment.h>
#include <sys/times.h>
#include <sys/utsname.h>

int sys_ftime()
{
	return -ENOSYS;
}

int sys_break()
{
	return -ENOSYS;
}

int sys_ptrace()
{
	return -ENOSYS;
}

int sys_stty()
{
	return -ENOSYS;
}

int sys_gtty()
{
	return -ENOSYS;
}

int sys_rename()
{
	return -ENOSYS;
}

int sys_prof()
{
	return -ENOSYS;
}

// 设置当前任务的实际以及/或者有效组 ID（gid）。
int sys_setregid(int rgid, int egid)
{
	if (rgid>0) {
		if ((current->gid == rgid) || 
		    suser())
			current->gid = rgid;
		else
			return(-EPERM);
	}
	if (egid>0) {
		if ((current->gid == egid) ||
		    (current->egid == egid) ||
		    (current->sgid == egid) ||
		    suser())
			current->egid = egid;
		else
			return(-EPERM);
	}
	return 0;
}

// 设置进程组号(gid)。
int sys_setgid(int gid)
{
	return(sys_setregid(gid, gid));
}

int sys_acct()
{
	return -ENOSYS;
}

int sys_phys()
{
	return -ENOSYS;
}

int sys_lock()
{
	return -ENOSYS;
}

int sys_mpx()
{
	return -ENOSYS;
}

int sys_ulimit()
{
	return -ENOSYS;
}

//如果 tloc 不为 null，则时间值也存储在那里
int sys_time(long * tloc)
{
	int i;

	i = CURRENT_TIME;
	if (tloc) {
		verify_area(tloc,4);
		put_fs_long(i,(unsigned long *)tloc);  // 也放入用户数据段 tloc 处
	}
	return i;
}

/*
 * Unprivileged users may change the real user id to the effective uid
 * or vice versa.    ????
 */
int sys_setreuid(int ruid, int euid)
{
	int old_ruid = current->uid;
	
	if (ruid>0) {
		if ((current->euid==ruid) ||
                    (old_ruid == ruid) ||
		    suser())
			current->uid = ruid;
		else
			return(-EPERM);
	}
	if (euid>0) {
		if ((old_ruid == euid) ||
                    (current->euid == euid) ||
		    suser())
			current->euid = euid;
		else {
			current->uid = old_ruid;
			return(-EPERM);
		}
	}
	return 0;
}

// 设置任务用户号(uid)。如果任务没有超级用户特权，它可以使用 setuid()将其有效 uid 
 // （effective uid）设置成其保留 uid(saved uid)或其实际 uid(real uid)。如果任务有 
 // 超级用户特权，则实际 uid、有效 uid 和保留 uid 都被设置成参数指定的 uid。
int sys_setuid(int uid)
{
	return(sys_setreuid(uid, uid));
}

// 设置系统时间和日期。参数 tptr 是从 1970 年 1 月 1 日 00:00:00 GMT 开始计时的时间值（秒）。 
 // 调用进程必须具有超级用户权限。
int sys_stime(long * tptr)
{
	if (!suser())    //super user
		return -EPERM;
	startup_time = get_fs_long((unsigned long *)tptr) - jiffies/HZ;     //开始时间为负值
	return 0;
}

//// 获取当前任务时间
int sys_times(struct tms * tbuf)
{
	if (tbuf) {
		verify_area(tbuf,sizeof *tbuf);
		put_fs_long(current->utime,(unsigned long *)&tbuf->tms_utime);
		put_fs_long(current->stime,(unsigned long *)&tbuf->tms_stime);
		put_fs_long(current->cutime,(unsigned long *)&tbuf->tms_cutime);
		put_fs_long(current->cstime,(unsigned long *)&tbuf->tms_cstime);
	}
	return jiffies;
}


// 设置数据段
int sys_brk(unsigned long end_data_seg)
{
	if (end_data_seg >= current->end_code &&
	    end_data_seg < current->start_stack - 16384)
		current->brk = end_data_seg;               //// 设置新数据段结尾值
	return current->brk;
}

/*
 * This needs some heave checking ...
 * I just haven't get the stomach for it. I also don't fully
 * understand sessions/pgrp etc. Let somebody who does explain it.
 */
int sys_setpgid(int pid, int pgid)
{
	int i;

	if (!pid)
		pid = current->pid;
	if (!pgid)
		pgid = current->pid;
	for (i=0 ; i<NR_TASKS ; i++)
		if (task[i] && task[i]->pid==pid) {
			if (task[i]->leader)
				return -EPERM;
			if (task[i]->session != current->session)
				return -EPERM;
			task[i]->pgrp = pgid;
			return 0;
		}
	return -ESRCH;
}

// 返回当前进程的组号
int sys_getpgrp(void)
{
	return current->pgrp;
}

// 创建一个会话(session)（即设置其 leader=1），并且设置其会话=其组号=其进程号
int sys_setsid(void)
{
	if (current->leader && !suser())
		return -EPERM;
	current->leader = 1;
	current->session = current->pgrp = current->pid;
	current->tty = -1;
	return current->pgrp;
}

// 获取系统信息。其中 utsname 结构包含 5 个字段，分别是：本版本操作系统的名称、网络节点名称、 
 // 当前发行级别、版本级别和硬件类型名称。
int sys_uname(struct utsname * name)
{
	static struct utsname thisname = {
		"linux .0","nodename","release ","version ","machine "
	};
	int i;

	if (!name) return -ERROR;
	verify_area(name,sizeof *name);
	for(i=0;i<sizeof *name;i++)        // 将 thisname 中的信息逐字节复制到用户缓冲区(name)中
		put_fs_byte(((char *) &thisname)[i],i+(char *) name);
	return 0;
}

// 设置当前进程创建文件属性屏蔽码为 mask & 0777。并返回原屏蔽码, 0777 = 0001 1111 1111
int sys_umask(int mask)
{
	int old = current->umask;

	current->umask = mask & 0777;
	return (old);
}
