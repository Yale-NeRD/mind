#ifndef __CACHELINE_DEF_H__
#define __CACHELINE_DEF_H__
enum
{
    CACHELINE_SUCCESS = 0xf,
    CACHELINE_SHARED = 6,
    CACHELINE_SHARED_DATA = 2,
    CACHELINE_SHARED_LOCK = 0xb,
    CACHELINE_MODIFIED = 7,
    CACHELINE_MODIFIED_DATA = 9,
    CACHELINE_MODIFIED_LOCK = 0xc,
    CACHELINE_IDLE = 8,
    CACHELINE_FAIL = 0x0,
    CACHELINE_EMPTY = 0xe,
};

#define CACHELINE_UNLOCKED (0)
#define CACHELINE_LOCKED (1)

#endif