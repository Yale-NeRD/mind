#ifndef __FORK_DISAGGREGATION_H__
#define __FORK_DISAGGREGATION_H__

#include <linux/sched.h>

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define ERR_DISAGG_FORK_TIMEOUT 101
#define ERR_DISAGG_FORK_NO_PREV 102
#define ERR_DISAGG_FORK_THREAD 	103

struct fork_msg_struct {
	u32	pid;
	u32	tgid;
	u32 prev_pid;
	u32	prev_tgid;
	u32	clone_flags;
	char	comm[TASK_COMM_LEN];
} __packed;

struct fork_reply_struct {
	int			ret;		// error code
	u32			vma_count;	// number of copied vma
} __packed;

#endif