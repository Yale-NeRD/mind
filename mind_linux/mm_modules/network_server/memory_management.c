#include "network_server.h"
#include "memory_management.h"
#include "request_handler.h"
// #include "rbtree_ftns.h"

static int disagg_computing_node_id = DISAGG_MEMORY_NODE_ID;

// From network_disagg.h
int get_local_node_id(void)
{
    return disagg_computing_node_id;
}

int set_local_node_id(int new_id)
{
	disagg_computing_node_id = new_id;
	return 0;
}
