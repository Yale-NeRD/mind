#ifndef __CTRL_CONFIG_H__
#define __CTRL_CONFIG_H__

#define __TEST__

// Ownership based directory pre-population and make local copy
#define CACHE_DIR_PRE_OPT
#define CACHE_DIR_PRE_POP_IDLE
/* WE DISABLED OWNERSHIP OPTIMIZATION */
// #define CACHE_OWNERSHIP_OPT

#define BFRT_NUM_COMPUTE_NODE 12
#define BFRT_NUM_CONN_COMPUTE_NODE 8
#define BFRT_NUM_MEMORY_NODE 4

// Dependencies
#ifdef CACHE_OWNERSHIP_OPT
#ifndef CACHE_DIR_PRE_OPT
#define CACHE_DIR_PRE_OPT
#endif
#endif

#include "include/disagg/cache_config.h"
#ifndef REGION_SIZE_64KB
#define REGION_SIZE_64KB (4)
#endif

#define INITIAL_REGION_INDEX REGION_SIZE_16KB
#define INITIAL_REGION_SHIFT (REGION_SIZE_BASE + INITIAL_REGION_INDEX)
#define INITIAL_REGION_SIZE (1 << INITIAL_REGION_SHIFT)

#define DYN_CACHE_COEFF (1.0)
#define DYN_CACHE_TAR_DIR (30000)   // 30K
#define DYN_CACHE_HIGH_PRESURE (0.8)
#define DYN_CACHE_HIGH_PRESURE_FREE (1.0 - DYN_CACHE_HIGH_PRESURE)
#define DYN_CACHE_LOW_PRESURE_INV_THRESHOLD (100) // relaxed threshold for merge
#define DYN_CACHE_SLEEP_CYCLE (1) // in second (approximated value)

/* UNCOMMENT FOLLOWINGS TO DISABLE BOUNDED SPLITTING */
// #define DYN_CACHE_DISABLE_MANAGER
// #define DYN_CACHE_DISABLE_MERGE
// #define DYN_CACHE_DISABLE_SPLIT
// #define DYN_CACHE_DISABLE_LOCK  // this switch will control conditional branch to generate locking: turn this on for removing any checking and locking

/* FLAGS FOR TESTING */
// #define __CTRL_RUN_BASE_TEST__
// #define __CTRL_RUN_BFRT_TEST__

#endif
