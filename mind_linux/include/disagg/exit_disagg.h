#ifndef __EXIT_DISAGGREGATION_H__
#define __EXIT_DISAGGREGATION_H__

#include <linux/sched.h>

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define ERR_DISAGG_EXIT_NO_TASK		1

struct exit_msg_struct {
	u32	pid;
	u32	tgid;
} __packed;

struct exit_reply_struct {
	int			ret;		// error code
} __packed;

#endif	/* __EXIT_DISAGGREGATION_H__ */