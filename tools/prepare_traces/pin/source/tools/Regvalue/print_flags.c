/*
 * Copyright 2002-2020 Intel Corporation.
 * 
 * This software is provided to you as Sample Source Code as defined in the accompanying
 * End User License Agreement for the Intel(R) Software Development Products ("Agreement")
 * section 1.L.
 * 
 * This software and the related documents are provided as is, with no express or implied
 * warranties, other than those that are expressly stated in the License.
 */

#include <stdio.h>
#include <string.h>

void* getFlags(void*);

int main()
{
    union
    {
        unsigned long long ull;
        void* voidp;
    } flags;
    
    memset(&flags, 0, sizeof(flags));
    
    // Call getFlags which in turn sets the flags register to 7,
    // call modifyFlags(), and return the value of the flags register
    // returned from modifyFlags()
    flags.voidp = getFlags((void*)7);
    printf("Flags: %llx\n", flags.ull);

    return 0;
}
