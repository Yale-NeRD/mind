#include "network_server.h"
#include "memory_management.h"
#include "request_handler.h"

// Dummy init and clear functions
int init_mn_man(void){
    return 0;
}

int clear_mn_man(void){
     return 0;
}

#if 0
static void get_timestamp(char* buf, unsigned int max_size)
{
    struct timeval t;
    struct tm broken;

    if (max_size < 32 || !buf)
        return;

    do_gettimeofday(&t);
    time_to_tm(t.tv_sec, 0, &broken);
    sprintf(buf, "%d:%d:%d:%06ld", 
            broken.tm_hour, broken.tm_min, 
            broken.tm_sec, t.tv_usec);
}
#endif
