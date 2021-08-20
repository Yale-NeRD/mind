#ifndef __MMAP_DISAGGREGATION_H__
#define __MMAP_DISAGGREGATION_H__

#include <linux/sched.h>

#ifndef __packed
#define __packed __attribute__((packed))
#endif

#define VM_CACHE_OWNER 0x04000000

struct mmap_msg_struct {
	u32	pid;
	u32	tgid;
	unsigned long addr;
    unsigned long len;
    unsigned long prot;
    unsigned long flags;
    unsigned long vm_flags;
    unsigned long pgoff;
    unsigned long file_id;
} __packed;

struct mmap_reply_struct {
    unsigned long   addr; 
	long		    ret;		// error code
} __packed;

struct brk_msg_struct {
	u32	pid;
	u32	tgid;
	unsigned long addr;
} __packed;

struct brk_reply_struct {
    unsigned long   addr; 
	int			    ret;		// error code
} __packed;

struct munmap_msg_struct {
	u32	pid;
	u32	tgid;
	unsigned long addr;
    unsigned long len;
} __packed;

struct munmap_reply_struct {
	int			    ret;		// error code
} __packed;

struct mremap_msg_struct {
	u32	pid;
	u32	tgid;
	unsigned long addr;
    unsigned long old_len;
    unsigned long new_len;
    unsigned long flags;
    unsigned long new_addr;
} __packed;

struct mremap_reply_struct {
	int			  ret;		// error code
    unsigned long new_addr;
} __packed;
#endif /* __MMAP_DISAGGREGATION_H__ */
