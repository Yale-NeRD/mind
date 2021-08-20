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

extern int CheckRedZone(void);

int main()
{
    int checkRedZone = CheckRedZone();
    if (checkRedZone != 0)
    {
        printf("Detected redzone overrun at byte %d\n", checkRedZone);
        return 1;
    }
    return 0;
}
