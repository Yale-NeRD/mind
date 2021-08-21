# please merge this file with Makefile.am provided by Intel under
# '$SDE/pkgsrc/bf-drivers/bf_switchd/bfrt_examples'
bin_PROGRAMS = tna_disagg_switch
path_to_mind_linux = /home/sslee/mind/mind_linux

tna_disagg_switch_SOURCES = tna_disagg_switch.cpp\
                            tna_disagg_bfrt.cpp\
                            tna_disagg_config.cpp\
                            tna_disagg_api.cpp\
                            tna_disagg_cacheline.cpp\
                            tna_disagg_cache_state.cpp\
                            controller/controller.c\
                            controller/cacheline_manager.c\
                            controller/cache_manager_thread.c\
                            controller/ht_body.c\
                            controller/task_management.c\
                            controller/request_handler.c\
                            controller/memory_management.c\
                            controller/mmap_ftns.c\
                            controller/mmap_unmapped_area.c\
                            controller/rbtree_lib.c\
                            controller/rbtree_ftns.c\
                            controller/memory_node_connect.c\
                            controller/controller_tests.c\
                            controller/debug.c
tna_disagg_switch_CFLAGS = $(common_cflags)
tna_disagg_switch_CXXFLAGS = $(common_cxxflags)
tna_disagg_switch_CPPFLAGS = $(common_cppflags)
tna_disagg_switch_LDADD = $(common_libs)
tna_disagg_switch_LDFLAGS = $(common_ldflags)

