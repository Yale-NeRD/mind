#ifndef __MEMORY_REQUEST_DISAGGREGATION_H__
#define __MEMORY_REQUEST_DISAGGREGATION_H__

#include <linux/sched.h>

#ifndef __packed
#define __packed __attribute__((packed))
#endif

struct meminit_msg_struct {
	u32	pid;
	u32	tgid;
	u64 offset;
	u64 len;
} __packed;

struct meminit_reply_struct {
	int			ret;		// error code
} __packed;

struct memcpy_msg_struct {
	u32	pid;
	u32	tgid;
	u64 src_offset;
	u64 dst_offset;
	u64 len;
} __packed;

struct memcpy_reply_struct {
	int			ret;		// error code
} __packed;

#endif	/* __MEMORY_REQUEST_DISAGGREGATION_H__ */