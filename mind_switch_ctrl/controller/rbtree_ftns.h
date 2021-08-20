#ifndef __MN_RBTREE_FTNS_MODULE_H__
#define __MN_RBTREE_FTNS_MODULE_H__

typedef unsigned long pgoff_t;

struct vm_area_struct;
struct mm_struct;

struct rb_node
{
	unsigned long __rb_parent_color;
	struct rb_node *rb_left, *rb_right;
	struct vm_area_struct *vma;
};

struct rb_root
{
	struct rb_node *rb_node;
};

struct rb_augment_callbacks
{
	void (*propagate)(struct rb_node *node, struct rb_node *stop);
	void (*copy)(struct rb_node *old, struct rb_node *new);
	void (*rotate)(struct rb_node *old, struct rb_node *new);
};

// MACRO for RB-Tree
#define RB_RED 0
#define RB_BLACK 1
#define RB_EMPTY_ROOT(root) (((root)->rb_node) == NULL)

#define RB_EMPTY_NODE(node) \
	((node)->__rb_parent_color == (unsigned long)(node))

#define __rb_parent(pc) ((struct rb_node *)(pc & ~3))
#define rb_parent(r) ((struct rb_node *)((r)->__rb_parent_color & ~3))
#define rb_entry(rb, X, Y) (rb->vma)

#define barrier() asm volatile("": : :"memory");  // barrier

// rb_node->vma can be replaced with general type such as void* in the future
#define RB_DECLARE_CALLBACKS(rbstatic, rbname, rbstruct, rbfield,       \
							 rbtype, rbaugmented, rbcompute)            \
	static inline void                                                  \
		rbname##_propagate(struct rb_node *rb, struct rb_node *stop)    \
	{                                                                   \
		while (rb != stop)                                              \
		{                                                               \
			rbstruct *node = (rbstruct *)rb->vma;                       \
			rbtype augmented = rbcompute(node);                         \
			if (node->rbaugmented == augmented)                         \
				break;                                                  \
			node->rbaugmented = augmented;                              \
			rb = rb_parent(&node->rbfield);                             \
		}                                                               \
	}                                                                   \
	static inline void                                                  \
		rbname##_copy(struct rb_node *rb_old, struct rb_node *rb_new)   \
	{                                                                   \
		rbstruct *old = (rbstruct *)rb_old->vma;                        \
		rbstruct *new = (rbstruct *)rb_new->vma;                        \
		new->rbaugmented = old->rbaugmented;                            \
	}                                                                   \
	static void                                                         \
		rbname##_rotate(struct rb_node *rb_old, struct rb_node *rb_new) \
	{                                                                   \
		rbstruct *old = (rbstruct *)rb_old->vma;                        \
		rbstruct *new = (rbstruct *)rb_new->vma;                        \
		new->rbaugmented = old->rbaugmented;                            \
		old->rbaugmented = rbcompute(old);                              \
	}                                                                   \
	rbstatic const struct rb_augment_callbacks rbname = {               \
		.propagate = rbname##_propagate,                                \
		.copy = rbname##_copy,                                          \
		.rotate = rbname##_rotate};

// === Functions in RB-Tree Ftns === //
void __vma_link_rb(struct mm_struct *mm, struct vm_area_struct *vma,
			  struct rb_node **rb_link, struct rb_node *rb_parent);

unsigned long count_vma_pages_range(struct mm_struct *mm,
		unsigned long addr, unsigned long end);

void vma_link(struct mm_struct *mm, struct vm_area_struct *vma,
		struct vm_area_struct *prev, struct rb_node **rb_link,
		struct rb_node *rb_parent);

struct vm_area_struct *mn_find_vma(struct mm_struct *mm, unsigned long addr);

int mn_find_vma_links(struct mm_struct *mm, unsigned long addr,
		unsigned long end, struct vm_area_struct **pprev,
		struct rb_node ***rb_link, struct rb_node **rb_parent);

struct vm_area_struct *mn_vma_merge(struct mm_struct *mm,
			struct vm_area_struct *prev, unsigned long addr,
			unsigned long end, unsigned long vm_flags,
			unsigned long *file, pgoff_t pgoff);
			//, struct mempolicy *policy);

int __vma_adjust(struct vm_area_struct *vma, unsigned long start,
	unsigned long end, pgoff_t pgoff, struct vm_area_struct *insert,
	struct vm_area_struct *expand);

int decompose_vma_for_alignment(struct mm_struct *mm, struct vm_area_struct *vma);

struct vm_area_struct *_existing_vma_merge(struct mm_struct *mm,
										   struct vm_area_struct *vma);

int __mn_split_vma(struct mm_struct *mm, struct vm_area_struct *vma,
				   unsigned long addr, int new_below);

void detach_vmas_to_be_unmapped(struct mm_struct *mm, struct vm_area_struct *vma,
		struct vm_area_struct *prev, unsigned long end);

// void mn_remove_vmas(struct mm_struct *mm);	//moved to memory_management.h

void remove_vma_list(struct mm_struct *mm, struct vm_area_struct *vma);

// === Functions in RB-Tree LIB === //
struct rb_node *rb_first(const struct rb_root *root);

struct rb_node *rb_next(const struct rb_node *node);

void rb_insert_augmented(struct rb_node *node, struct rb_root *root,
						 const struct rb_augment_callbacks *augment);

void rb_erase_augmented(struct rb_node *node, struct rb_root *root,
						const struct rb_augment_callbacks *augment);

void rb_link_node(struct rb_node *node, struct rb_node *parent,
				  struct rb_node **rb_link);

#endif  /* __MN_RBTREE_FTNS_MODULE_H__ */
