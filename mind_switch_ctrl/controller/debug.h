#define __CTRL_PRINT_RULE__

/* Turn ON/OFF log messages here */
// #define __CTRL_PRINT_VMA__
// #define __CTRL_PRINT_RBTREE__
// #define __CTRL_PRINT_CACHE__
// #define __CTRL_PRINT_CACHE_VERBOSE__
#define __CTRL_PRINT_DYN_CACHE__
// #define __CTRL_PRINT_DYN_CACHE_VERBOSE__
#define __CTRL_PRINT_OTHERS__

#ifdef __CTRL_PRINT_VMA__
#define pr_vma(...) printf(__VA_ARGS__)
#else
#define pr_vma(...) \
    do              \
    {               \
    } while (0);
#endif

#ifdef __CTRL_PRINT_RBTREE__
#define pr_rbtree(...) printf(__VA_ARGS__)
#else
#define pr_rbtree(...) \
    do                 \
    {                  \
    } while (0);
#endif

#ifdef __CTRL_PRINT_RULE__
#define pr_rule(...) printf(__VA_ARGS__)
#else
#define pr_rule(...) \
    do               \
    {                \
    } while (0);
#endif

#ifdef __CTRL_PRINT_CACHE__
#define pr_cache(...) printf(__VA_ARGS__)
#else
#define pr_cache(...) \
    do                \
    {                 \
    } while (0);
#endif

#ifdef __CTRL_PRINT_CACHE_VERBOSE__
#define pr_cache_v(...) printf(__VA_ARGS__)
#else
#define pr_cache_v(...) \
    do                \
    {                 \
    } while (0);
#endif

#ifdef __CTRL_PRINT_DYN_CACHE__
#define pr_dyn_cache(...) printf(__VA_ARGS__)
#else
#define pr_dyn_cache(...) \
    do                \
    {                 \
    } while (0);
#endif

#ifdef __CTRL_PRINT_DYN_CACHE_VERBOSE__
#define pr_dyn_cache_v(...) printf(__VA_ARGS__)
#else
#define pr_dyn_cache_v(...) \
    do                \
    {                 \
    } while (0);
#endif

#ifdef __CTRL_PRINT_OTHERS__
#define pr_others(...) printf(__VA_ARGS__)
#else
#define pr_others(...) \
    do                \
    {                 \
    } while (0);
#endif

// Debug functions to test current status
#define REG_SYNC_1TO2 1
#define REG_SYNC_2TO1 2
#define REG_RST_ON_UPDATE 7

uint64_t read_cache_dir_for_test(uint16_t tgid, unsigned long addr, int direction);
void get_timestamp(char *buf, unsigned int max_size);

#define TIME_SEC_IN_US 1000000
