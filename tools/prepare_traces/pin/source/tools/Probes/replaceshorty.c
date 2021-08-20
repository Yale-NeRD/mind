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

/*
 * this application calls a user-written assembly routine which contains
 * a specific code pattern.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void shorty();

int main( int argc, char * argv[] )
{
    char * buffer;

    buffer = (char *)malloc( 64 );
    strcpy( buffer, "abc" );
    printf("%s\n", buffer );

    shorty();
    printf("returned from shorty & do_nothing.\n");

    free( buffer );
    return 0;
}
