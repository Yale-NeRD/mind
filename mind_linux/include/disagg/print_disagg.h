
#ifndef __PRINT_DISAGGREGATION_H__
#define __PRINT_DISAGGREGATION_H__

// LIST OF DEBUGGING FLAGS WE CAN ENABLE
#ifdef CONFIG_COMPUTE_NODE
// #define PRINT_CACHE_COHERENCE
// #define PRINT_RDMA_TRANSMISSION
// #define PRINT_PAGE_FAULT
// #define PRINT_SYSCALLS
// #define PRINT_SWITCH_STATUS
// #define CONFIG_PROFILING_POINTS
#endif

#ifdef PRINT_CACHE_COHERENCE
#define pr_cache(...) printk(KERN_DEFAULT __VA_ARGS__)
#else
#define pr_cache(...) \
    do                \
    {                 \
    } while (0);
#endif

#ifdef PRINT_RDMA_TRANSMISSION
#define pr_rdma(...) printk(KERN_DEFAULT __VA_ARGS__)
#else
#define pr_rdma(...) \
    do               \
    {                \
    } while (0);
#endif

#ifdef PRINT_PAGE_FAULT
#define pr_pgfault(...) printk(KERN_DEFAULT __VA_ARGS__)
#else
#define pr_pgfault(...) \
    do                  \
    {                   \
    } while (0);
#endif

#ifdef PRINT_SYSCALLS
#define pr_syscall(...) printk(KERN_DEFAULT __VA_ARGS__)
#else
#define pr_syscall(...) \
    do                  \
    {                   \
    } while (0);
#endif

#endif
