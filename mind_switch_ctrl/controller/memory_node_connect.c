#include "controller.h"
#include "memory_management.h"
#include <math.h>

static void* __sw_zeroed_page = NULL;

void init_mn_virtual_address_man(void)
{
    __sw_zeroed_page = malloc(PAGE_SIZE);
    memset(__sw_zeroed_page, 0, PAGE_SIZE);
}

static int is_in_local_address_range(int node_idx, unsigned long addr)
{
    if (addr >= MN_VA_MIN_ADDR + (unsigned long)(MN_VA_PARTITION_BASE * node_idx) &&
        addr < (unsigned long)(node_idx + 1) * MN_VA_PARTITION_BASE)
        return 1;

    return 0;
}

static int get_mn_node_idx(int node_id)
{
    int num_mn = 0;
    struct mn_status **mn_list = get_memory_node_status(&num_mn);
    int i = 0;

    // Load balancing based mapping
    for (i = 0; i < num_mn; i++)
    {
        if (mn_list[i]->node_id == node_id)
        {
            return i;
        }
    }
    return -1;
}

int get_best_node(unsigned long size)
{
    int num_mn = 0;
    struct mn_status **mn_list = get_memory_node_status(&num_mn);
    int i = 0;
    unsigned long min_load = ULONG_MAX;
    int min_node = -1;
    (void)size;

    // Load balancing based mapping
    for (i = 0; i < num_mn; i++)
    {
        if (mn_list[i]->alloc_size < min_load)
        {
            min_node = i;
            min_load = mn_list[i]->alloc_size;
        }
    }
    // Return the index among the mn_list (not node id)
    return min_node;
}

static int add_mn_map_to_list(struct memory_node_mapping *mnmap, struct list_node *alloc_vma_list)
{
    // Here, we do not check there is enough space for this new mapping
    if (!alloc_vma_list->next ||
        alloc_vma_list->next == alloc_vma_list)
    {
        // TODO: this would be (kind of) atomic or acquire lock first
        pr_vma("Add to the list\n");
        list_insert_after(alloc_vma_list, mnmap);
        mnmap->node = alloc_vma_list->next;
        return 0;
    }
    else
    {
        struct memory_node_mapping *mn_map_ptr =
            (struct memory_node_mapping*)(alloc_vma_list->next->data);
        if (!mn_map_ptr)
            return -1; //error

        while (mn_map_ptr)
        {
            // pr_vma("Search MN map: 0x%lx, next:0x%lx, head:0x%lx\n",
            //        (unsigned long)mn_map_ptr, (unsigned long)mn_map_ptr->node, 
            //        (unsigned long)alloc_vma_list);
            if (mn_map_ptr->base_addr > mnmap->base_addr)
            {
                struct list_node *tmp = mn_map_ptr->node->prev;
                list_insert_after(tmp, mnmap);
                mnmap->node = tmp->next;
                return 0;
            }

            // Check next. If it is the end of the list, and add mnmap at the end
            if (!mn_map_ptr->node->next ||
                mn_map_ptr->node->next == alloc_vma_list)
            {
                list_insert_after(mn_map_ptr->node, mnmap);
                mnmap->node = mn_map_ptr->node->next;
                return 0;
            }

            // Move to the next node
            mn_map_ptr = (struct memory_node_mapping *)mn_map_ptr->node->next->data;
        }
    }
    return -1;
}

inline unsigned long get_pow_of_two_req_size(unsigned long size)
{
    if (size > DISAGG_VMA_MAX_SIZE)
    {
        return ((unsigned long)ceil(size / DISAGG_VMA_MAX_SIZE)) * DISAGG_VMA_MAX_SIZE;
    }else{
        return (unsigned long)pow(2, ceil(log2(size)));
    }
}

// Check the address and size is aligned properly
// - Return 1 for allowed, return 0 for poorly aligned
static int is_aligned_pow_of_two(unsigned long addr, unsigned long size)
{
    unsigned long required_size = get_pow_of_two_req_size(size);
    return (addr % required_size == 0) ? 1 : 0;
}

unsigned int get_prefix_from_size(unsigned long size)
{
    return MN_VA_PID_BITS + (MN_VA_PID_SHIFT - (uint16_t)ceil(log2(size)));
}

uint64_t get_full_virtual_address(uint16_t tgid, unsigned long addr)
{
    uint64_t full_addr = (uint64_t)tgid << MN_VA_PID_SHIFT; // first 16 bits
    full_addr += addr & (~MN_VA_PID_BIT_MASK);              // last 48 bits
    return full_addr;
}

static int is_available_address(unsigned long addr, unsigned long size, struct list_node *alloc_vma_list, unsigned long addr_max)
{
    unsigned long addr_end;
    addr &= MN_VA_OFFSET_BIT_MASK;
    addr_end = addr + size;

    if (addr_end >= addr_max || addr < MN_VA_MIN_ADDR || !is_aligned_pow_of_two(addr, size))
        return 0;

    if (!alloc_vma_list->next ||
        alloc_vma_list->next == alloc_vma_list)
    {
        return 1;
    }
    else
    {
        struct memory_node_mapping *mn_map_ptr =
            (struct memory_node_mapping *)(alloc_vma_list->next->data);

        if ((!mn_map_ptr))
            return 0; // Error

        while (mn_map_ptr)
        {
            // Filter out intersections
            if ((mn_map_ptr->base_addr < addr_end) &&
                (mn_map_ptr->base_addr + mn_map_ptr->size > addr))
            {
                return 0; // not available
            }

            // If it is the end of the list or no further comparison is not needed
            if (!mn_map_ptr->node->next ||
                mn_map_ptr->node->next == alloc_vma_list ||
                mn_map_ptr->base_addr >= addr_end)
            {
                return 1;
            }

            // Move to the next node
            mn_map_ptr = (struct memory_node_mapping *)mn_map_ptr->node->next->data;
        }
    }
    return 0;
}

u64 get_next_pow_of_two_addr(u64 start, u64 size)
{
    u64 remain = start % size;
    return (remain == 0) ? start : start + (size - remain);
}

// TODO: make it as best fit (instead of first fit)
// mn_node_idx: index of the mn node (not the memory node id)
static unsigned long get_best_available_offset(int mn_node_idx, unsigned long size)
{
    int num_mn = 0;
    struct mn_status *mn_ptr = get_memory_node_status(&num_mn)[mn_node_idx];
    u64 addr = 0; // local address (i.e., offset)
    unsigned long req_size = get_pow_of_two_req_size(size);

    if (!mn_ptr->alloc_vma_list.next ||
        mn_ptr->alloc_vma_list.next == &mn_ptr->alloc_vma_list)
    {
        addr = MN_VA_MIN_ADDR;
    }
    else
    {
        struct memory_node_mapping *mn_map_ptr =
            (struct memory_node_mapping *)(mn_ptr->alloc_vma_list.next->data);
        unsigned long prev_addr = MN_VA_MIN_ADDR;

        if (!mn_map_ptr)
        {
            return 0;
        }

        while (mn_map_ptr)
        {
            pr_vma("Check PA [0x%lx - 0x%lx] | prev [0x%lx] for len [0x%lx]\n",
                   mn_map_ptr->base_addr, mn_map_ptr->base_addr + mn_map_ptr->size,
                   prev_addr, req_size);
            if (mn_map_ptr->base_addr - prev_addr >= req_size)
            {
                if (mn_map_ptr->base_addr - get_next_pow_of_two_addr(prev_addr, req_size) >= req_size)
                {
                    addr = prev_addr;
                    break;
                }
            }

            // Check next. If it is the end of the list, and add the current at the end
            if (!mn_map_ptr->node->next ||
                mn_map_ptr->node->next == &mn_ptr->alloc_vma_list)
            {
                addr = get_next_pow_of_two_addr(mn_map_ptr->base_addr + mn_map_ptr->size, req_size);
                break;
            }

            // Move to the next node
            prev_addr = mn_map_ptr->base_addr + mn_map_ptr->size;
            mn_map_ptr = (struct memory_node_mapping *)mn_map_ptr->node->next->data;
        }
    }

    if (mn_ptr->node_info->size < addr + req_size) //out of memory
    {
        fprintf(stderr, "Offset/Addr: 0x%lx, MN Size: 0x%lx\n", addr, mn_ptr->node_info->size);
        return 0;
    }

    return addr;
}

/*
 *  Zero memory ranges using preallocated zeroed buffer. 
 *  As the buffer only used for fill the zeroes (i.e., read only)
 *  no locking mechanism is required here.
 */
int zeros_mem_range(int mn_node_idx, unsigned long offset, unsigned long size)
{
    int num_mn = 0;
    struct mn_status *mn_ptr = get_memory_node_status(&num_mn)[mn_node_idx];
    return send_meminit_request(mn_ptr->node_info, offset, size);
}

static unsigned long allocate_best_available_offset(int mn_node_idx, unsigned long size)
{
    unsigned long offset;
    
    offset = get_best_available_offset(mn_node_idx, size);
    if ((!offset))
    {
        return 0;
    }

    if ((zeros_mem_range(mn_node_idx, offset, size)))
    {
        return 0;
    }

    return offset;
}

int get_node_idx(int mn_node_id, unsigned long size)
{
    int mn_node_idx = -1;

    // Find best memory node
    if (mn_node_id < 0)
    {
        mn_node_idx = get_best_node(size);
    }
    else
    {
        // Get the node index (if it was provided)
        mn_node_idx = get_mn_node_idx(mn_node_id);
    }

    return mn_node_idx;
}

static unsigned long __base_offset_for_debug = 0x0;
unsigned long
get_available_virtual_address_lock(unsigned long size, spinlock_t **mn_lock_ptr)
{
    int num_mn = 0;
    struct mn_status **mn_list = get_memory_node_status(&num_mn);
    unsigned long offset;
    int mn_node_idx = get_node_idx(-1, size);

    if ((mn_node_idx < 0))
    {
        printf("Cannot find memory node index for node idx [%d]\n", mn_node_idx);
        return 0;
    }

    if (!mn_lock_ptr)
        return 0;

    *mn_lock_ptr = &mn_list[mn_node_idx]->alloc_lock;
    pthread_spin_lock(*mn_lock_ptr);

    offset = get_best_available_offset(mn_node_idx, size);
    if ((!offset))
    {
        return 0;
    }

    return __base_offset_for_debug + ((MN_VA_PARTITION_BASE * mn_node_idx) | offset);
}

void add_new_addr_trans_rule(unsigned long addr, unsigned long size,
                             struct memory_node_mapping *mnmap, unsigned int tgid,
                             unsigned int permission, struct vm_area_struct *vma)
{
    // Size should be power of 2 (allocation granuarality fix)
    // First 16 bits for PID, 48 - size that should be matched together
    // = 64 - log_2(size)
    uint16_t prefix = get_prefix_from_size(size);
    char *mem_node_ip = get_memory_node_ip(mnmap->node_id);
    uint64_t full_addr = get_full_virtual_address(tgid, addr);
    struct mm_struct *mm = vma->vm_mm;
    if (!mem_node_ip)
    {
        error_and_exit("Cannot get IP of memory node", __func__, __LINE__);
    }
    else
    {
        /*
         *  Here we do the pre-calculation for the address translation from
         *  Full address (=PID + VMA base) + VMA offset 
         *  -> mapped DMA address (=DMA base + mapped VMA address + VMA offset)
         *  So the difference is -Full address + (DMA base + mapped VMA address)
         */
        bfrt_add_addr_except_trans(full_addr, prefix, mem_node_ip,
                                   mnmap->mn_stat->node_info->base_addr // base address of memory node
                                       + mnmap->base_addr               // base address of mapped VMA
                                       - full_addr,                     // conversion from full_addr
                                   permission, !vma->is_test);
        free(mem_node_ip);

        // Update rule statistics
        mm->acc_ctrl_rule ++;
        if (mm->acc_ctrl_rule > mm->max_acc_ctrl_rule)
            mm->max_acc_ctrl_rule = mm->acc_ctrl_rule;

        if (!vma->is_aligned)
        {
            mm->addr_trans_rule++;
            if (mm->addr_trans_rule > mm->max_addr_trans_rule)
                mm->max_addr_trans_rule = mm->addr_trans_rule;
        }else{
            // printf("AT: %lu, AC: %lu\n", mm->max_addr_trans_rule, mm->max_acc_ctrl_rule);
        }
    }
}

void modify_addr_trans_rule(unsigned long addr, unsigned long size,
                            struct memory_node_mapping *mnmap, unsigned int tgid,
                            unsigned int permission)
{
    uint16_t prefix = get_prefix_from_size(size);
    char *mem_node_ip = get_memory_node_ip(mnmap->node_id);
    uint64_t full_addr = get_full_virtual_address(tgid, addr);
    if (!mem_node_ip)
    {
        error_and_exit("Cannot get IP of memory node", __func__, __LINE__);
    }
    else
    {
        bfrt_modify_addr_except_trans(full_addr, prefix, mem_node_ip,
                                      mnmap->mn_stat->node_info->base_addr // base address of memory node
                                          + mnmap->base_addr               // base address of mapped VMA
                                          - full_addr,                     // conversion from full_addr
                                      permission);
        free(mem_node_ip);
    }
}

void del_addr_trans_rule(uint64_t addr, unsigned long size, struct vm_area_struct *vma)
{
    struct mm_struct *mm = vma->vm_mm;

    bfrt_del_addrExceptTrans_rule(addr, get_prefix_from_size(size), !vma->is_test);

    // Update rule statistics
    mm->acc_ctrl_rule--;
    if (!vma->is_aligned && mm->addr_trans_rule > 0)
    {
        mm->addr_trans_rule--;
    }
}

/*
 * Find next available memory node id and virtual address for the new vm
 * mn_node_id: id of the mn_node (not index among mn_list)
 */
int get_virtual_address(int mn_node_id, unsigned long addr, unsigned long size,
                        unsigned int tgid, struct memory_node_mapping *mnmap,
                        unsigned int permission, struct vm_area_struct *vma)
{
    int num_mn = 0;
    struct mn_status **mn_list = get_memory_node_status(&num_mn);
    int mn_node_idx = -1;
    int locked = 0;
    int res = -1;

    if (!mnmap)
    {
        return -1;
    }

    mn_node_idx = get_node_idx(mn_node_id, size);

    if ((mn_node_idx < 0))
    {
        printf("Cannot find memory node index for node [%d] idx [%d]\n", mn_node_id, mn_node_idx);
        return -1;
    }
    mn_node_id = mn_list[mn_node_idx]->node_id; // Update id for mn_node_id < 0
    mnmap->node_id = mn_list[mn_node_idx]->node_id; // Used for actual RDMA tx
    mnmap->mn_stat = mn_list[mn_node_idx];

    // Do we already have a lock for this?
    if (pthread_spin_trylock(&mn_list[mn_node_idx]->alloc_lock))
    {
        // Unable to acquire lock (=already locked, so we can continue)
    }else{
        // If success, then it is locked now, so we are responsible to unlock
        locked = 1;
    }

    // Check the given address and set up appropriate base_addr
    if (is_in_local_address_range(mn_node_idx, addr) &&
        is_available_address(addr, size, &mnmap->mn_stat->alloc_vma_list,
                             (unsigned long)(mnmap->mn_stat->node_info->size)))
    {
        mnmap->base_addr = addr & (MN_VA_OFFSET_BIT_MASK);
        if ((mnmap->base_addr && zeros_mem_range(mn_node_idx, mnmap->base_addr, size)))
        {
            // error case
            mnmap->base_addr = 0;
        }
        // Use default partition (no additional address translation rule needed)
        vma->is_aligned = 1;
        // printf("Assigned in alignment: 0x%lx\n", addr);
    }
    else
    {
        // Initialized inside allocate_best_available_offset()
        mnmap->base_addr = allocate_best_available_offset(mn_node_idx, size);
        vma->is_aligned = 0;
    }

    if (!mnmap->base_addr)
    {
        printf("Cannot find virtual address for node idx [%d] size [0x%lx]\n",
               mn_node_idx, size);
        goto release_lock_and_exit; // fail, if it is NULL
    }
    else
    {
        // Add new translation rule
        add_new_addr_trans_rule(addr, size, mnmap, tgid, permission, vma);
    }

    mnmap->size = size;
    pr_vma("Allocated offset: VA[0x%lx] -> m_idx[%d] addr[0x%lx] size [0x%lx]\n",
           addr, mn_node_idx, mnmap->base_addr, size);

    // Add to the list
    if (add_mn_map_to_list(mnmap, &mnmap->mn_stat->alloc_vma_list))
        goto release_lock_and_exit;

    mnmap->mn_stat->alloc_size += size;
    res = 0; // return 0 if succeed

release_lock_and_exit:
    if (locked) // if locked by itself
        pthread_spin_unlock(&mn_list[mn_node_idx]->alloc_lock);

    return res;
}

/*
 * Read and write data to the memory node
 * INPUT
 * mnmap: struct memory_node_mapping and va from process (CN)
 * offset: offset from the base address of the given VMA (vm_start)
 * size: size of data to be read/written
 * buf: location of the data to be read/written
 * OUTPUT: writen/read size in bytes
 */
int read_data_from_mn(struct memory_node_mapping *mnmap, unsigned long offset, unsigned long size, void *buf)
{
    (void)mnmap;
    (void)offset;
    (void)size;
    (void)buf;
    BUG();
}

int write_data_to_mn(struct memory_node_mapping *mnmap, unsigned long offset, unsigned long size, void *buf)
{
    (void)mnmap;
    (void)offset;
    (void)size;
    (void)buf;
    BUG();
}

int send_meminit_request(struct dest_info *mnmap,
                         unsigned long offset,
                         unsigned long len)
{
    void *msg = NULL;
    struct meminit_msg_struct *request;
    struct meminit_reply_struct reply;
    int ret = -1;

    // Make header and attach payload
    unsigned long len_payload = sizeof(struct meminit_msg_struct);
    unsigned long tot_len = len_payload + sizeof(struct mem_header);
    msg = malloc(tot_len);
    if (!msg)
    {
        return -ENOMEM;
    }

    request = get_payload_ptr(msg);
    request->offset = (u64)offset;
    request->len = (u64)len;
    reply.ret = -1; // Init

    ret = send_msg_to_mem(DISAGG_MEM_INIT, msg, tot_len, mnmap->sk,
                          (void *)&reply, sizeof(struct meminit_reply_struct));

    if (ret >= (int)sizeof(struct meminit_reply_struct))
    {
        if (reply.ret)
        {
            printf("Meminit init with zero 0x%lx (0x%lx)\n", offset, len);
            printf("Meminit ACK from memory node id[%d] recv[%d] ret[%d]\n",
                   mnmap->node_id, ret, reply.ret);
        }
        ret = reply.ret;
    }
    else
    {
        printf("Meminit: Cannot recv ack from memory node [%d]\n", ret);
        ret = -1;
    }

    if (msg)
        free(msg);
    return ret;
}

int send_memcpy_request(struct dest_info *mnmap,
                               unsigned long old_off, unsigned long new_off,
                               unsigned long copy_len)
{
    void *msg = NULL;
    struct memcpy_msg_struct *request;
    struct memcpy_reply_struct reply;
    int ret = -1;

    // Make header and attach payload
    unsigned long len_payload = sizeof(struct memcpy_msg_struct);
    unsigned long tot_len = len_payload + sizeof(struct mem_header);
    msg = malloc(tot_len);
    if (!msg)
    {
        return -ENOMEM;
    }

    request = get_payload_ptr(msg);
    request->src_offset = (u64)old_off;
    request->dst_offset = (u64)new_off;
    request->len = (u64)copy_len;
    reply.ret = -1; // Init

    ret = send_msg_to_mem(DISAGG_MEM_COPY, msg, tot_len, mnmap->sk,
                          (void *)&reply, sizeof(struct memcpy_reply_struct));

    if (ret >= (int)sizeof(struct memcpy_reply_struct))
    {
        pr_vma("Memcpy from 0x%lx <- 0x%lx (0x%lx)\n",
               new_off, old_off, copy_len);
        pr_vma("Memcpy ACK from memory node id[%d] recv[%d] ret[%d]\n",
               mnmap->node_id, ret, reply.ret);
        ret = 0;
    }
    else
    {
        printf("Memcpy: Cannot recv ack from memory node [%d]\n", ret);
        ret = -1;
    }

    if (msg)
        free(msg);
    return ret;
}
