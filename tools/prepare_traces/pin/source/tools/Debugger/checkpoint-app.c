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


int A[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
int Sum = 0;

static void Done();
void (* volatile pfDone)() = &Done;

int main()
{
    int i;

    printf("Hello world\n");
    for (i = 0;  i < 10;  i++)
        Sum += A[i];

    printf("Sum is %d\n", Sum);
    fflush(stdout);
    pfDone();
    return 0;
}

static void Done()
{
}
