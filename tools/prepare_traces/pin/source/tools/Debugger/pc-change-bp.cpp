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

#include <iostream>

extern "C" int One();
extern "C" int GetValue(int (*)());


int main()
{
    int value = GetValue(One);
    std::cout << "Value is " << std::dec << value << std::endl;
    return 0;
}
