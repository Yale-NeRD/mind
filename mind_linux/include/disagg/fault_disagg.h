#ifndef __FAULT_DISAGGREGATION_H__
#define __FAULT_DISAGGREGATION_H__

#ifndef BF_CONTORLLER
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/types.h>
#endif

#ifndef __packed
#define __packed __attribute__((packed))
#endif

// Return values
#define DISAGG_FAULT_NONE			0	// 
#define DISAGG_FAULT_WRITE			1	// write permission OK
#define DISAGG_FAULT_READ			2	// read permission OK

// Errors
#define DISAGG_FAULT_ERR_NO_VMA		90	// No vma
#define DISAGG_FAULT_ERR_PERM		91	// No permission
#define DISAGG_FAULT_ERR_NO_MEM		92	// Not enough memory to process request
#define DISAGG_FAULT_ERR_NO_ANON	93	// VMA is not anonmous
#define DISAGG_FAULT_ERR_LOCK		94	// locking was failed

// Speicial & debug
#define DISAGG_FAULT_DEBUG_DATA_MISS	99	// 

struct fault_msg_struct {
	u32	pid;
	u32	tgid;
	// mm related params
	unsigned long address;
	unsigned long error_code;   /* error code from page fault handler */
    unsigned int flags;
	char	comm[TASK_COMM_LEN];
} __packed;

struct fault_reply_struct {
	int				ret;		// error code
	unsigned long	address;
	unsigned long	tgid;
	unsigned long 	pid;
	unsigned long	vm_start;
	unsigned long	vm_end;
	unsigned long	vm_flags;
	// they should be the last variables - dynamic length
	u32 			data_size;
	void	    	*data;
	char			*ack_buf;
} __packed;	// __packed __aligned(8);

struct fault_data_struct {
	u32				pid;
	u32				tgid;
	unsigned long 	address;
	u32				req_qp;
	// data_size will be a length: [address, address + data_size)
	// they should be the last variables - dynamic length
	u32 			data_size;	// will be 4KB
	void	    	*data;
} __packed;

#ifndef BF_CONTORLLER
// pte_t *find_pte(unsigned long address);
pte_t *find_pte_target(struct mm_struct *mm, unsigned long address);
pte_t *find_pte_target_lock(struct mm_struct *mm, unsigned long address, spinlock_t **ptl_ptr);
#endif

// only for debug
#endif /* __FAULT_DISAGGREGATION_H__ */
