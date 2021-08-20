/*
 * mm/mmap_disagg.c
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/backing-dev.h>
#include <linux/mm.h>
#include <linux/vmacache.h>
#include <linux/shm.h>
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <linux/swap.h>
#include <linux/syscalls.h>
#include <linux/capability.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/personality.h>
#include <linux/security.h>
#include <linux/hugetlb.h>
#include <linux/shmem_fs.h>
#include <linux/profile.h>
#include <linux/export.h>
#include <linux/mount.h>
#include <linux/mempolicy.h>
#include <linux/rmap.h>
#include <linux/mmu_notifier.h>
#include <linux/mmdebug.h>
#include <linux/perf_event.h>
#include <linux/audit.h>
#include <linux/khugepaged.h>
#include <linux/uprobes.h>
#include <linux/rbtree_augmented.h>
#include <linux/notifier.h>
#include <linux/memory.h>
#include <linux/printk.h>
#include <linux/userfaultfd_k.h>
#include <linux/moduleparam.h>
#include <linux/pkeys.h>
#include <linux/oom.h>

#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/tlb.h>
#include <asm/mmu_context.h>
#include <asm/page_types.h>

#include "internal.h"
#include <disagg/config.h>
#include <disagg/exec_disagg.h>
#include <disagg/network_disagg.h>
#include <disagg/mmap_disagg.h>
#include <disagg/cnthread_disagg.h>
#include <disagg/print_disagg.h>

#ifndef arch_mmap_check
#define arch_mmap_check(addr, len, flags)	(0)
#endif

void DEBUG_print_vma(struct mm_struct *mm)
{
	int i = 0;
	struct vm_area_struct *ln, *rn, *cur = mm->mmap;

	for(;cur;cur = cur->vm_next)
	{
		ln = cur->vm_rb.rb_left ? rb_entry(cur->vm_rb.rb_left, struct vm_area_struct, vm_rb) : 0;
		rn = cur->vm_rb.rb_right ? rb_entry(cur->vm_rb.rb_right, struct vm_area_struct, vm_rb) : 0;
		pr_info("  *[%d, %p] addr: 0x%lx - 0x%lx [pR/W: %d/%d], l: %p, r: %p\n",
				i, cur, cur->vm_start, cur->vm_end, 
				cur ? ((cur->vm_flags & VM_READ) ? 1 : 0) : -1,
				cur ? ((cur->vm_flags & VM_WRITE) ? 1 : 0) : -1,
				ln, rn);
		i++;
	}
}


static unsigned long send_mmap_to_mn(struct task_struct *tsk, unsigned long addr,
		unsigned long len, unsigned long prot, unsigned long flags, 
		unsigned long  vm_flags, unsigned long pgoff, struct file *file,
		int *ownership)
{
    struct mmap_msg_struct payload;
    struct mmap_reply_struct *reply;
    unsigned long ret_addr = (unsigned long)NULL;
	int ret = -1;

    reply = kmalloc(sizeof(struct mmap_reply_struct), GFP_KERNEL);
	if (!reply)
        return -ENOMEM;

retry_mmap:
	memset(reply, 0, sizeof(struct mmap_reply_struct));

	payload.pid = tsk->pid;
	payload.tgid = tsk->tgid;
    payload.addr = addr;
	payload.len = len;
	payload.prot = prot;
	payload.flags = flags;
	payload.vm_flags = vm_flags;
	payload.pgoff = pgoff;
	payload.file_id = (unsigned long)file;

	// Check the size of the received data
	ret = send_msg_to_memory(DISSAGG_MMAP, &payload, sizeof(payload),
							 reply, sizeof(*reply));
	pr_syscall(KERN_DEFAULT "MMAP - Data from CTRL [%d]: ret: %ld, addr: 0x%lx [0x%llx], owner: %d\n",
			   ret, reply->ret, reply->addr, *(long long unsigned *)(reply), !reply->ret ? 1 : 0);

	if (ret < 0)
	{
		msleep(250);
		goto retry_mmap;
	}else{
		//reply->ret = 0: success, cacheline populated, 1: success, cacheline not populated
		if (!reply->ret && ownership)
		{
			*ownership = 1;
		}
		ret_addr = reply->addr;   // set error or addr
	}
    kfree(reply);
    return ret_addr;
}

unsigned long do_disagg_mmap(struct task_struct *tsk,
            unsigned long addr, unsigned long len, unsigned long prot,
			unsigned long flags, vm_flags_t vm_flags, unsigned long pgoff, 
			struct file *file)
{
	// DEBUG: before mapping, sync with the memory node
	// cn_copy_vma_to_mn(tsk, DISSAGG_COPY_VMA);

	// send mmap request to memory node
	return send_mmap_to_mn(tsk, addr, len, prot, flags,
						  	  (unsigned long)vm_flags, pgoff, file, NULL);
}
EXPORT_SYMBOL(do_disagg_mmap); // for unit test in RoceModule

unsigned long do_disagg_mmap_owner(struct task_struct *tsk,
            unsigned long addr, unsigned long len, unsigned long prot,
			unsigned long flags, vm_flags_t vm_flags, unsigned long pgoff,
			struct file *file, int *ownership)
{
	return send_mmap_to_mn(tsk, addr, len, prot, flags,
						   (unsigned long)vm_flags, pgoff, file, ownership);
}

/*
 * Disaggregated brk
 */
static unsigned long send_brk_to_mn(struct task_struct *tsk, unsigned long addr)
{
    struct brk_msg_struct payload;
    struct brk_reply_struct *reply;
    unsigned long ret_addr = (unsigned long)NULL;
	int ret = -1;

    reply = kmalloc(sizeof(struct brk_reply_struct), GFP_KERNEL);
	if (!reply)
        return -ENOMEM;

    payload.pid = tsk->pid;
	payload.tgid = tsk->tgid;
    payload.addr = addr;

	ret = send_msg_to_memory(DISSAGG_BRK, &payload, sizeof(payload),
							 reply, sizeof(*reply));
	pr_syscall(KERN_DEFAULT "BRK - Data from CTRL [%d]: ret: %d, addr: 0x%lx [0x%llx]\n",
			   ret, reply->ret, reply->addr, *(long long unsigned *)(reply));

	// Check the size of the received data
	if (ret >=0 && !reply->ret)
	{
		ret_addr = reply->addr;   // if success, set it as the updated addr
	}

    kfree(reply);
    return ret_addr;
}

unsigned long disagg_brk(struct task_struct *tsk, unsigned long brk)
{
	// DEBUG: before mapping, sync with the memory node
	// cn_copy_vma_to_mn(tsk, DISSAGG_COPY_VMA);

	// send request to remote memory
	return send_brk_to_mn(tsk, brk);
}

/*
 * Disaggregated munmap
 */
static int send_munmap_to_mn(struct task_struct *tsk, unsigned long addr,
							 unsigned long len)
{
    struct munmap_msg_struct payload;
    struct munmap_reply_struct *reply;
    int ret = -1;
	
	reply = kmalloc(sizeof(struct munmap_reply_struct), GFP_KERNEL);
	if (!reply)
        return -ENOMEM;

    payload.pid = tsk->pid;
	payload.tgid = tsk->tgid;
	payload.addr = addr;
	payload.len = len;
	
	ret = send_msg_to_memory(DISSAGG_MUNMAP, &payload, sizeof(payload),
							 reply, sizeof(*reply));
	pr_syscall(KERN_DEFAULT "MUNMAP - Data from CTRL [%d]: ret: %d [0x%llx]\n",
		   ret, reply->ret, *(long long unsigned *)(reply));

	// Check the size of the received data
	if (ret >=0 && !reply->ret)
		ret = reply->ret;
	else
		ret = -1;	// error case

    kfree(reply);
    return ret;
}

int disagg_munmap(struct task_struct *tsk, unsigned long start, size_t len)
{
	int res = -1;
	res = send_munmap_to_mn(tsk, start, (unsigned long)len);
	if (!res)
	{
		unsigned long offset;
		start &= PAGE_MASK;
		for (offset = 0; offset < len; offset += PAGE_SIZE)
		{
			cnthread_delete_from_list(tsk->tgid, start + offset);
		}
	}
	return res;
}

/*
 * Disaggregated mremap
 */
static unsigned long send_mremap_to_mn(struct task_struct *tsk, 
			unsigned long addr, unsigned long old_len,
			unsigned long new_len, unsigned long flags,
			unsigned long new_addr)
{
    struct mremap_msg_struct payload;
    struct mremap_reply_struct *reply;
	unsigned long ret_addr = 0;
	int ret = -1;
	
	reply = kzalloc(sizeof(struct mremap_reply_struct), GFP_KERNEL);
	if (!reply)
        return -ENOMEM;

    payload.pid = tsk->pid;
	payload.tgid = tsk->tgid;
	payload.addr = addr;
	payload.old_len = old_len;
	payload.new_len = new_len;
	payload.flags = flags;
	payload.new_addr = new_addr;
	
	// Check the size of the received data
	ret = send_msg_to_memory(DISSAGG_MREMAP, &payload, sizeof(payload),
							 reply, sizeof(*reply));
	pr_syscall(KERN_DEFAULT "MREMAP - Data from CTRL [%d]: ret: %d, addr: 0x%lx [0x%llx]\n",
			   ret, reply->ret, reply->new_addr, *(long long unsigned *)(reply));

	if (ret >=0 && !reply->ret)    // Only 0 is success
		ret_addr = reply->new_addr;

    kfree(reply);
    return ret_addr;
}

unsigned long disagg_mremap(struct task_struct *tsk, 
			unsigned long addr, unsigned long old_len,
			unsigned long new_len, unsigned long flags,
			unsigned long new_addr)
{
	return send_mremap_to_mn(tsk, addr, old_len, new_len, flags, new_addr);
}

__always_inline int TEST_is_target_vma(unsigned long vm_start, unsigned long vm_end)
{
	return ((vm_end - vm_start) >= TEST_INIT_ALLOC_SIZE) ? 1 : 0;
}

__always_inline int TEST_is_sub_target_vma(unsigned long vm_start, unsigned long vm_end)
{
	return ((vm_end - vm_start) == TEST_SUB_REGION_ALLOC_SIZE) ? 1 : 0;
}

__always_inline int TEST_is_meta_vma(unsigned long vm_start, unsigned long vm_end)
{
	return ((vm_end - vm_start) == TEST_META_ALLOC_SIZE) ? 1 : 0;
}

__always_inline int TEST_is_test_vma(unsigned long vm_start, unsigned long vm_end)
{
	return (TEST_is_target_vma(vm_start, vm_end) || TEST_is_sub_target_vma(vm_start, vm_end) 
			|| TEST_is_meta_vma(vm_start, vm_end));
}