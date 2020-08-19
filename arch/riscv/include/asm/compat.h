/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __ASM_COMPAT_H
#define __ASM_COMPAT_H

#include <asm-generic/compat.h>

#ifdef CONFIG_COMPAT

/*
 * Architecture specific compatibility types
 */
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/ptrace.h>

#define COMPAT_USER_HZ		100
#define COMPAT_UTS_MACHINE	"riscv32\0"

typedef u32		__compat_uid_t;
typedef u32		__compat_gid_t;
typedef u32		__compat_uid32_t;
typedef u32		__compat_gid32_t;
typedef u32		compat_mode_t;
typedef u32		compat_dev_t;
typedef s32		compat_ipc_pid_t;
typedef u32		compat_caddr_t;
typedef s64		compat_s64;
typedef u64		compat_u64;

// Doesn't exist, but must be declared here to silence warnings.
struct compat_stat {
	compat_dev_t	st_dev;
	compat_ino_t	st_ino;
	compat_mode_t	st_mode;
	compat_uint_t	st_nlink;
	__compat_uid_t	st_uid;
	__compat_gid_t	st_gid;
	compat_dev_t	st_rdev;
	compat_off_t	st_size;
	compat_off_t	st_blksize;
	compat_int_t	__pad2;
	compat_off_t	st_blocks;
	old_time32_t	st_atime;
	compat_ulong_t	st_atime_nsec;
	old_time32_t	st_mtime;
	compat_ulong_t	st_mtime_nsec;
	old_time32_t	st_ctime;
	compat_ulong_t	st_ctime_nsec;
	compat_ulong_t	__unused4[2];
};

// Doesn't exist, but must be declared here to silence warnings.
struct compat_statfs {
	int		f_type;
	int		f_bsize;
	int		f_blocks;
	int		f_bfree;
	int		f_bavail;
	int		f_files;
	int		f_ffree;
	__kernel_fsid_t	f_fsid;
	int		f_namelen;
	int		f_frsize;
	int		f_flags;
	int		f_spare[4];
};

struct compat_flock {
	short		l_type;
	short		l_whence;
	compat_off_t	l_start;
	compat_off_t	l_len;
	compat_pid_t	l_pid;
};

#define F_GETLK64	12	/*  using 'struct flock64' */
#define F_SETLK64	13
#define F_SETLKW64	14

struct compat_flock64 {
	short		l_type;
	short		l_whence;
	compat_loff_t	l_start;
	compat_loff_t	l_len;
	compat_pid_t	l_pid;
};

#define COMPAT_RLIM_INFINITY		0xffffffff

#define _COMPAT_NSIG		64
#define _COMPAT_NSIG_BPW	32

typedef u32		compat_sigset_word;

#define COMPAT_OFF_T_MAX	0x7fffffff

static inline void __user *arch_compat_alloc_user_space(long len)
{
	return (void __user *)current_user_stack_pointer() - len;
}

struct compat_ipc64_perm {
	compat_key_t key;
	__compat_uid32_t uid;
	__compat_gid32_t gid;
	__compat_uid32_t cuid;
	__compat_gid32_t cgid;
	compat_mode_t mode;
	unsigned short seq;
	unsigned short __pad2;
	compat_ulong_t unused1;
	compat_ulong_t unused2;
};

struct compat_semid64_ds {
	struct compat_ipc64_perm sem_perm;
	compat_ulong_t sem_otime;
	compat_ulong_t sem_otime_high;
	compat_ulong_t sem_ctime;
	compat_ulong_t sem_ctime_high;
	compat_ulong_t sem_nsems;
	compat_ulong_t __unused3;
	compat_ulong_t __unused4;
};

struct compat_msqid64_ds {
	struct compat_ipc64_perm msg_perm;
	compat_ulong_t msg_stime;
	compat_ulong_t msg_stime_high;
	compat_ulong_t msg_rtime;
	compat_ulong_t msg_rtime_high;
	compat_ulong_t msg_ctime;
	compat_ulong_t msg_ctime_high;
	compat_ulong_t msg_cbytes;
	compat_ulong_t msg_qnum;
	compat_ulong_t msg_qbytes;
	compat_pid_t   msg_lspid;
	compat_pid_t   msg_lrpid;
	compat_ulong_t __unused4;
	compat_ulong_t __unused5;
};

struct compat_shmid64_ds {
	struct compat_ipc64_perm shm_perm;
	compat_size_t  shm_segsz;
	compat_ulong_t shm_atime;
	compat_ulong_t shm_atime_high;
	compat_ulong_t shm_dtime;
	compat_ulong_t shm_dtime_high;
	compat_ulong_t shm_ctime;
	compat_ulong_t shm_ctime_high;
	compat_pid_t   shm_cpid;
	compat_pid_t   shm_lpid;
	compat_ulong_t shm_nattch;
	compat_ulong_t __unused4;
	compat_ulong_t __unused5;
};


#ifdef CONFIG_TANGO_BT
static inline int is_tango_compat_thread(struct thread_info *thread)
{
	return thread->tango_syscall;
}
static inline int is_tango_compat_task(void)
{
	return is_tango_compat_thread(current_thread_info());
}
#endif

static inline int is_compat_thread(struct thread_info *thread)
{
#ifdef CONFIG_TANGO_BT
	return is_tango_compat_thread(thread);
#else
	return 0;
#endif
}

static inline int is_compat_task(void)
{
	return is_compat_thread(current_thread_info());
}

#endif /* CONFIG_COMPAT */
#endif /* __ASM_COMPAT_H */
