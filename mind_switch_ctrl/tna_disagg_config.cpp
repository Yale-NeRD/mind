#include "tna_disagg_bfrt.hpp"
#include "controller/config.h"

namespace bfrt
{
  namespace tna_disagg_switch
  {
    const int ports_comp[BFRT_NUM_COMPUTE_NODE] = {4, 12, 20, 28, 36, 44, 52, 60,
                                                   24, 16, 8, 0};
    const char *ips_comp[BFRT_NUM_COMPUTE_NODE] = {"10.10.10.201", "10.10.10.202", "10.10.10.203", "10.10.10.204",
                                                   "10.10.10.205", "10.10.10.206", "10.10.10.207", "10.10.10.208",
                                                   "10.10.10.209", "10.10.10.210", "10.10.10.211", "10.10.10.212"};
    const char *macs_comp[BFRT_NUM_COMPUTE_NODE] = {"04:3f:72:a2:b4:a2", "04:3f:72:a2:b4:a3", "04:3f:72:a2:b5:f2", "04:3f:72:a2:b5:f3",
                                                    "0c:42:a1:41:8b:5a", "0c:42:a1:41:8b:5b", "04:3f:72:a2:b0:12", "04:3f:72:a2:b0:13",
                                                    "0c:42:a1:41:8a:92", "0c:42:a1:41:8a:93", "04:3f:72:a2:b4:3a", "04:3f:72:a2:b4:3b"};
    const int ports_mem[BFRT_NUM_MEMORY_NODE] = {56, 40, 48, 32};
    const char *ips_mem[BFRT_NUM_MEMORY_NODE] = {"10.10.10.221", "10.10.10.222",
                                                 "10.10.10.223", "10.10.10.224"};
    const char *macs_mem[BFRT_NUM_MEMORY_NODE] = {"04:3f:72:a2:b7:3a", "04:3f:72:a2:c5:32",
                                                  "04:3f:72:a2:b7:3b", "04:3f:72:a2:c5:33"};

    int get_comp_port(int idx)
    {
      return ports_comp[idx];
    }

    const char *get_comp_ip(int idx)
    {
      return ips_comp[idx];
    }

    const char *get_comp_mac(int idx)
    {
      return macs_comp[idx];
    }

    int get_mem_port(int idx)
    {
      return ports_mem[idx];
    }

    const char *get_mem_ip(int idx)
    {
      return ips_mem[idx];
    }

    const char *get_mem_mac(int idx)
    {
      return macs_mem[idx];
    }
  } // namespace tna_disagg_switch
} // namespace bfrt
