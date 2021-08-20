/*
 * Header file of fork and disaggregated fork functions
 */
#include <linux/fork.h>
#include <disagg/fork_disagg.h>
#include <disagg/network_disagg.h>
#include <disagg/exec_disagg.h>
#include <disagg/print_disagg.h>
#include <linux/exec.h>

static int send_copy_mm(struct task_struct *tsk, unsigned long clone_flags)
{
    struct fork_msg_struct payload;
    struct fork_reply_struct *reply;
    int ret;

    reply = kmalloc(sizeof(struct fork_reply_struct), GFP_KERNEL);
    if (!reply)
        return -ENOMEM;

    payload.pid = tsk->pid;
    payload.tgid = tsk->tgid;
    payload.prev_pid = current->pid;
    payload.prev_tgid = current->tgid;
    payload.clone_flags = clone_flags;
    memcpy(payload.comm, tsk->comm, TASK_COMM_LEN);

    ret = send_msg_to_memory(DISSAGG_FORK, &payload, sizeof(payload),
                             reply, sizeof(*reply));
    printk(KERN_DEFAULT "Fork - Data from CTRL [%d]: ret: %d, vma_cnt: %u [0x%llx]\n",
           ret, reply->ret, reply->vma_count, *(long long unsigned *)(reply));

    if (reply->ret)       // only 0 is success
        ret = reply->ret; // set error

    kfree(reply);
    return ret; // error for ret < 0
}

/*
 * Currently this function forward request to memory node 
 * while it just use the local functions
 */
static atomic_t test_program_thread_cnt;
atomic_t *get_test_program_thread_cnt(void)
{
    return &test_program_thread_cnt;
}

void init_test_program_thread_cnt(void)
{
    atomic_set(&test_program_thread_cnt, 0);
}

int disagg_fork(unsigned long clone_flags, struct task_struct *tsk)
{
    /*
     * There will be no sync in here, because copy from
     * existing mm_struct should not be a problem since they
     * only have page table of the local/static/read-only data
     */
    int err = 0;
    int cnt = atomic_inc_return(get_test_program_thread_cnt());
    if (tsk->is_test)
        pr_syscall("FORK: cnt[%d]\n", cnt);
retry_send_msg:
    if (tsk->is_test && cnt > 1)
    {
        err = 0;
    }
    else
    {
        err = send_copy_mm(tsk, 0);
    }

    if (err < 0)
    {
        if (err == -ERR_DISAGG_FORK_NO_PREV)
        {
            // send exec sync
            printk(KERN_ERR "FORK - no existing proc: send exec sync: %s\n", tsk->comm);

            // Erase all the write-able: we need to hold mmap_sem
            down_write(&tsk->mm->mmap_sem);
            err = cn_notify_exec(tsk);
            up_write(&tsk->mm->mmap_sem);
        }
        else if (err == -ERR_DISAGG_FORK_THREAD)
        {
            printk(KERN_WARNING "New thread added, %s\n", tsk->comm);
        }
        else
        {
            printk(KERN_ERR "Cannot send copy_mm() to memory (%d), %s\n",
                   err, tsk->comm);
            msleep(250);
            goto retry_send_msg;
        }
    }
    else
    {
        // Erase all the write-able: we need to hold mmap_sem
        down_write(&tsk->mm->mmap_sem);
        err = cn_notify_fork(tsk);
        up_write(&tsk->mm->mmap_sem);
    }
    return err;
}

int disagg_fork_report_only(struct task_struct *tsk)
{
    atomic_inc(get_test_program_thread_cnt());
    return send_copy_mm(tsk, 0);
}

// This is the test version of the function
int disagg_fork_for_test(struct task_struct *tsk)
{
    /* 
     * There will be no sync in here, because copy from
     * existing mm_struct should not be a problem since they
     * only have page table of the local/static/read-only data
     */
    int err = 0;
    err = send_copy_mm(tsk, 0);
    if (err < 0)
    {
        if (err == -ERR_DISAGG_FORK_NO_PREV)
        {
            err = 0;
        }
        else if (err == -ERR_DISAGG_FORK_THREAD)
        {
            printk(KERN_WARNING "New thread added, %s\n", tsk->comm);
        }
        else
        {
            printk(KERN_ERR "Cannot send copy_mm() to memory (%d), %s\n",
                   err, tsk->comm);
        }
    }
    return err;
}
// for unit test in RoceModule
// (not test this function, but we need process for testing memory mappings)
EXPORT_SYMBOL(disagg_fork_for_test);
