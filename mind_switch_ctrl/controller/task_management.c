#include "controller.h"
#include "memory_management.h"
#include "request_handler.h"
#include "cacheline_manager.h"
#include "cache_manager_thread.h"
#include "fault.h"
#include "list_and_hash.h"
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>

static struct hash_table mn_compute_hash[MAX_NUMBER_COMPUTE_NODE];
static struct list_node mn_utgid_list;
static pthread_spinlock_t task_lock;

/* hash related functions */
u32 generate_utgid(u16 sender_id, u16 tgid)
{
    u32 res = (sender_id << 16);
    return (res | (u32)tgid);
}

// mn: memory node
extern void init_mn_virtual_address_man(void);
int init_mn_man(void)
{
    int i = 0;

    // initialize compute node hash
    for (i = 0; i < MAX_NUMBER_COMPUTE_NODE; i++)
    {
        ht_create(&mn_compute_hash[i], 1 << MN_PID_HASH_BIT);
    }
    list_init(&mn_utgid_list);
    pthread_spin_init(&task_lock, PTHREAD_PROCESS_PRIVATE);

    init_mn_virtual_address_man();

    return 0;
}

void task_spin_lock(void)
{
    pthread_spin_lock(&task_lock);
}

void task_spin_unlock(void)
{
    pthread_spin_unlock(&task_lock);
}

/* Functions for handling (node id, tgid) -> (unique tgid) mapping */
struct task_struct *mn_get_task(u16 sender_id, u16 tgid)
{
    struct node_tgid_hash *hnode = NULL;

    if (sender_id >= MAX_NUMBER_COMPUTE_NODE)
    {
        // Out of range
        return NULL;
    }

    sender_id = 0;  // we will use only tgid
    hnode = ht_get(&mn_compute_hash[sender_id], hash_ftn_u8(tgid), tgid);
    if (hnode && hnode->utgid_node)
    {
        return hnode->utgid_node->tsk;
    }
    return NULL;
}

struct task_struct *mn_get_task_by_utgid(u32 utgid)
{
    struct list_node *tmp = mn_utgid_list.next;

    while (tmp && tmp != &mn_utgid_list)
    {
        if (tmp->data && (((struct unique_tgid_node *)tmp->data)->utgid == utgid))
        {
            return ((struct unique_tgid_node *)tmp->data)->tsk;
        }
        tmp = tmp->next;
    }
    return NULL;
}

static void fprint_termination_stat(struct task_struct *tsk)
{
    FILE *fp;
    char file_name[TASK_FILE_NAME] = {0};
    u64 anon_size = DEBUG_count_anon_vma(tsk->mm);
    sprintf(file_name, "%s%u.log", TASK_LOG_DIR, (unsigned int)tsk->tgid);
    fp = fopen(file_name, "a+");
    if (!fp)
    {
        fprintf(stderr, "Cannot open file: %s\n", file_name);
        return; // No file
    }

    // Anonymous mapping size
    fprintf(fp, "Allocated anonymous VMA size: %lu.%03lu KB\n", anon_size / 1000, anon_size % 1000);
    fprintf(fp, "Max address translation rules: %lu\n", tsk->mm->max_addr_trans_rule);
    fprintf(fp, "Max access control rules: %lu\n", tsk->mm->max_acc_ctrl_rule);

    if (fp)
        fclose(fp);
}

static void free_task_mm(struct task_struct *tsk)
{
    if (tsk)
    {
        if (tsk->mm)
        {
            fprint_termination_stat(tsk);
            mn_mmput(tsk->mm);
            tsk->mm = NULL;
        }
        free(tsk);
    }
}

static struct unique_tgid_node *get_utgid(u16 sender_id, u16 tgid)
{
    struct node_tgid_hash *hnode = NULL;

    if (sender_id >= MAX_NUMBER_COMPUTE_NODE)
    {
        // Out of range
        return NULL;
    }

    sender_id = 0;
    hnode = ht_get(&mn_compute_hash[sender_id], hash_ftn_u8(tgid), tgid);
    if (hnode && hnode->utgid_node)
    {
        return hnode->utgid_node;
    }
    return NULL;
}

struct file_info *mn_get_file(u16 sender_id, u16 tgid)
{
    (void)sender_id;
    (void)tgid;

    return NULL;
}

void increase_utgid_ref(u16 sender_id, u16 tgid)
{
    struct unique_tgid_node *utgid = get_utgid(0, tgid);
    if (utgid)
    {
        utgid->local_ref++;
    }
    (void)sender_id;
}

void write_vma_log(u32 sender_id, u32 tgid, struct mm_struct *mm)
{
    int i = 0;
    (void)sender_id;
    (void)tgid;

    if (mm)
    {
        char str_time[32] = {0};
        struct vm_area_struct *cur = mm->mmap;
        get_timestamp(str_time, 32);
        for (; cur; cur = cur->vm_next)
        {
            char str_data[256] = {0};
            sprintf(str_data, "%s, %d, 0x%lx, 0x%lx, 0x%lx, %d, %d\n",
                    str_time, DISSAGG_EXIT, cur->vm_start, cur->vm_end, cur->vm_flags, cur->vm_private_data ? 1 : 0, // actually accessed / allocated?
                    cur->vm_file ? 1 : 0);                                                                           // file mapping?
            // write_file(finfo, str_data); // no file for now
            DEBUG_print_one_vma(cur, i);
            i++;
        }
    }
}

static inline u64 generate_full_addr(u16 tgid, unsigned long addr)
{
    u64 full_addr = (u64)tgid << MN_VA_PID_SHIFT; // First 16 bits
    full_addr += addr & (~MN_VA_PID_BIT_MASK);    // Last 48 bits
    return full_addr;
}

static void clear_cacheline(struct task_struct *tsk)
{
    struct vm_area_struct *vma;
    unsigned long addr;
    if (tsk && tsk->mm && tsk->mm->mmap)
    {
        vma = tsk->mm->mmap;
        cacheman_run_lock();	// To prevent confliction with cache manager thread
        while (vma)
        {
            for (addr = vma->vm_start; addr < vma->vm_end; addr += CACHELINE_MIN_SIZE)
            {
                try_clear_cache_dir(generate_full_addr(tsk->tgid, addr));
            }
            pr_cache("Cleared cache for [0x%lx - 0x%lx], next vma: 0x%lx\n",
                     vma->vm_start, vma->vm_end, (unsigned long)vma->vm_next);
            vma = vma->vm_next;
        }
        cacheman_run_unlock();
    }
}

extern u8 hash_ftn(u16 tgid);

// Delete given task from the hash and list
// We need to check there are other nodes using the same task_struct now
int mn_delete_task(u16 sender_id, u16 tgid)
{
    struct node_tgid_hash *hnode = NULL;
    struct hlist *tmp = NULL;

    if (sender_id >= MAX_NUMBER_COMPUTE_NODE)
    {
        // Out of range
        return -1;
    }

    sender_id = 0;
    tmp = ht_get_list_head(&mn_compute_hash[sender_id], hash_ftn_u8(tgid));
    while (tmp)
    {
        hnode = (struct node_tgid_hash *)tmp->value;

        // Find target nodes
        if ((tmp->key == tgid) && hnode && (hnode->tgid == tgid) && hnode->utgid_node)
        {
            /* 
             * Delete from the list first, then free the object
             * Reference/pointers should be clear as NULL
             */
            hnode->utgid_node->local_ref--;

            // Assumeing serialized access, we do not care about the parallel access
            // or atomic access
            if (hnode->utgid_node->local_ref > 0)
            {
                printf("Decreased reference for - sid: %u, tgid: %u, ref: %d\n",
                       sender_id, tgid, hnode->utgid_node->local_ref);
                break;
            }

            // Print vma log
            // write_vma_log((u16)sender_id, (u16)tgid, hnode->utgid_node->tsk->mm);

            if (hnode->utgid_node->tsk)
            {
                // Cache dirs
                clear_cacheline(hnode->utgid_node->tsk);
                free_task_mm(hnode->utgid_node->tsk);
                hnode->utgid_node->tsk = NULL;
            }

            // Utgid
            list_delete_utgid(&mn_utgid_list, hnode->utgid_node);
            free(hnode->utgid_node);
            hnode->utgid_node = NULL;

            // Hash
            ht_free_node(&mn_compute_hash[sender_id], hash_ftn_u8(tgid), tmp);
            free(hnode);

            printf("Removed task node - sid: %u, tgid: %u\n", sender_id, tgid);
            break;
        }
        tmp = tmp->next;
    }
    return 0;
}

int clear_mn_man(void)
{
    int i = 0;
    struct unique_tgid_node *utgid_ptr;

    // Clear utgid list
    while (mn_utgid_list.next && mn_utgid_list.next != &mn_utgid_list)
    {
        // We do not care about the referece counter here
        utgid_ptr = mn_utgid_list.next->data;
        list_delete_node(&mn_utgid_list, mn_utgid_list.next); // remove the first node
        free_task_mm(utgid_ptr->tsk);
        free(utgid_ptr);
    }
    printf("UTGID lists cleared\n");

    for (i = 0; i < MAX_NUMBER_COMPUTE_NODE; i++)
    {
        ht_free(&mn_compute_hash[i]); // It frees hash node and its value (i.e., node_tgid_hash)
    }
    printf("Hash lists cleared\n");
    return 0;
}

int mn_insert_new_task_mm(u16 sender_id, u16 tgid, struct task_struct *tsk)
{
    u32 utgid;
    struct unique_tgid_node *tgnode;
    struct node_tgid_hash *hnode;

    if (!tsk)
    {
        fprintf(stderr, "Cannot insert NULL into the list\n");
        return -1;
    }

    sender_id = 0; // We will use only tgid

    // Here we assume that given sender_id, tgid is not existing (already checked)
    utgid = generate_utgid(sender_id, tgid);

    // Make utgid record
    tgnode = malloc(sizeof(struct unique_tgid_node));
    tgnode->utgid = utgid;
    tgnode->tsk = tsk;
    tgnode->local_ref = 1; // This is the first reference
    list_insert_at_head(&mn_utgid_list, tgnode); // simply at the head

    // Make (sender_id, tgid) record
    hnode = malloc(sizeof(struct node_tgid_hash));
    hnode->node_id = sender_id;
    hnode->tgid = tgid;
    hnode->utgid_node = tgnode;
    ht_put(&mn_compute_hash[sender_id], hash_ftn_u8(tgid), tgid, hnode);

    //NOTE: we used kmalloc instead of kzalloc because we imediately initialized the values
    //TODO: error of OOM

    return 0;
}

int mn_link_to_task_mm(u16 sender_id, u16 tgid, u32 utgid)
{
    (void)utgid;
    // TODO: add new node for sender_id, tgid
    // TODO: find utgid and add link from newly added node and the found utgid_node
    // TODO: If it was not connected before, increase &mm->mm_users, and ref counter
    increase_utgid_ref(sender_id, tgid);
    return 0;
}
