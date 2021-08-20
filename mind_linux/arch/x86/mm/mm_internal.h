/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __X86_MM_INTERNAL_H
#define __X86_MM_INTERNAL_H

void *alloc_low_pages(unsigned int num);
static inline void *alloc_low_page(void)
{
	return alloc_low_pages(1);
}

void early_ioremap_page_table_range_init(void);

unsigned long kernel_physical_mapping_init(unsigned long start,
					     unsigned long end,
					     unsigned long page_size_mask);
void zone_sizes_init(void);

extern int after_bootmem;

void update_cache_mode_entry(unsigned entry, enum page_cache_mode cache);

#ifdef CONFIG_COMPUTE_NODE
void
do_disagg_page_fault(struct task_struct *tsk, struct pt_regs *regs, 
		unsigned long error_code, unsigned long address, unsigned int flags);
void print_pgfault_error(struct task_struct *tsk, unsigned long error_code,
						 unsigned long address, struct vm_area_struct *vma);
#endif /* CONFIG_COMPUTE_NODE */

#endif	/* __X86_MM_INTERNAL_H */
