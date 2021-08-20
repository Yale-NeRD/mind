#ifndef __TNA_DISAGG_SWITCH_BFRT_H__
#define __TNA_DISAGG_SWITCH_BFRT_H__

#include <bf_rt/bf_rt_info.hpp>
#include <bf_rt/bf_rt_init.hpp>
#include <bf_rt/bf_rt_common.h>
#include <bf_rt/bf_rt_table_key.hpp>
#include <bf_rt/bf_rt_table_data.hpp>
#include <bf_rt/bf_rt_table.hpp>
#include <getopt.h>
#include <unistd.h>

namespace bfrt
{
  namespace tna_disagg_switch
  {
    // Size of C/C++ types should be ceiled value of P4
    // E.g., uint8_t for 1 ~ 8 bit wide fields

    // Structure definition to represent the key of the ipRoute table
    struct IpRouteKey
    {
        uint32_t ipDstAddr;
        // uint16_t vrf;
    };

    // Structure definition to represent the data of the ipRoute table for action
    // "route"
    struct IpRoute_routeData
    {
        // uint64_t srcMac;
        uint64_t dst_mac;
        uint16_t dst_port;
    };

    // Structure definition to represent the data of the ipRoute table for action
    // "nat"
    struct IpRoute_natData
    {
        uint32_t srcAddr;
        uint32_t dstAddr;
        uint16_t dst_port;
    };

    // Structure definition to represent the data of the ipRoute table
    struct IpRouteData
    {
        union
        {
            IpRoute_routeData route_data;
            IpRoute_natData nat_data;
        } data;
        // Based on the action_id, contents of the enum are interpreted
        bf_rt_id_t action_id;
    };

    struct EgressRouteKey
    {
      uint16_t dst_port;
    };

    struct EgressRouteData
    {
      uint32_t dstAddr;
      uint64_t dstMac;
    };

    struct EgressUpdateReqKey
    {
      uint32_t dstAddr;
      // uint16_t dst_port;
    };

    struct EgressUpdateReqData
    {
      uint32_t srcAddr;
    };

    struct EgressInvRouteKey
    {
      uint16_t dst_port;
      uint8_t inv_idx;
    };

    struct EgressInvRouteData
    {
      uint32_t dstAddr;
      uint64_t dstMac;
      uint32_t destQp;
      uint32_t rkey;
      uint64_t vaddr;
      uint16_t regIdx;
    };

    struct EgressAckTransKey
    {
      uint32_t destQp;
      uint32_t ipDstAddr;
    };

    struct EgressAckTransData
    {
      uint32_t destQp;
      uint32_t rkey;
      uint64_t vaddr;
      uint16_t regIdx;
    };

    // Cacheline
    struct CachelineKey
    {
      uint64_t vaddr;
      uint16_t vaddr_prefix;
    };

    struct CachelineData
    {
      uint32_t cacheIdx;
    };

    // Cachline registers
    struct CachelineRegKey
    {
      uint32_t cacheIdx;
    };

    struct CachelineRegData
    {
      std::vector<uint64_t> state;
      std::vector<uint64_t> updataing;
      std::vector<uint64_t> sharer;
      std::vector<uint64_t> dir_size;
      std::vector<uint64_t> dir_lock;
      std::vector<uint64_t> inv_cnt;
    };

    // Cache state transition keys and data
    struct CacheStateTransKey
    {
      uint8_t cur_state;
      uint8_t permission;
      uint8_t write_req;
    };

    struct CacheStateTransData
    {
      uint8_t next_state;
      uint8_t reset_sharer;
      uint8_t send_inval;
    };

    struct CacheStateSharerKey
    {
      uint32_t ipSrcAddr; // 32 bit
    };

    struct CacheStateSharerData
    {
      uint16_t sharer;
    };

    struct CacheOwnerCheckKey
    {
      uint16_t sharer_mask;
    };

    struct CacheEgSharerKey
    {
      uint32_t ipDstAddr; // 32 bit
    };

    struct CacheEgSharerData
    {
      uint16_t sharer;
    };

    // Structure for Roce Request and Ack requests
    // - keys
    struct RoceReqKey
    {
      uint32_t ipSrcAddr; // 32 bit
      uint32_t ipDstAddr; // 32 bit
      uint32_t roceDestQp; // 24 bit
    };

    struct RoceAckKey
    {
      uint32_t roceDestQp;  // 24 bit
      // uint32_t ipSrcAddr;   // 32 bit
    };

    struct RoceDummyAckKey
    {
      uint32_t ipSrcAddr;   // 32 bit
      uint32_t roceDestQp;  // 24 bit
    };

    struct RoceAckDestKey
    {
      uint32_t ipSrcAddr;   // 32 bit
      uint32_t roceDestQp;  // 24 bit
    };

    // - single action per table
    struct RoceReqData
    {
      uint32_t destQp;
      uint32_t rkey;
      uint64_t srcMac;
      uint32_t srcAddr;
      uint16_t regIdx;
      // uint16_t psnSlot;
      bf_rt_id_t action_id;
    };

    struct RoceAckData
    {
      uint32_t destQp;
      uint32_t dstAddr;
      uint32_t srcAddr;
      uint16_t regIdx;
      // uint16_t psnSlot;
      bf_rt_id_t action_id;
    };

    struct RoceDummyAckData
    {
      uint32_t destQp;
      uint64_t vaddr;
    };

    struct RoceAckDestData
    {
      uint32_t destQp;
      uint32_t dummyQPId;
    };

    struct SetQpIdxKey
    {
      uint32_t cpuQpId;   // 24 bit
      uint32_t ipSrcAddr; // 32 bit
    };

    struct SetQpIdxData
    {
      uint16_t globalQpId;
    };

    // for roce req to ack conversion
    struct SenderQpKey
    {
      uint32_t cpuQpId;   // 24 bit
      uint32_t ipSrcAddr; // 32 bit
    };

    struct SenderQpData
    {
      uint16_t memQpId;
    };

    // Structure for Address translation
    struct AddrTransKey
    {
      // uint32_t rkey;  // 32 bit
      uint64_t vaddr; // 48 bit
      uint16_t vaddr_prefix;
    };

    struct AddrTransData
    {
      uint32_t dstAddr;
      uint64_t vaddrToDmaAddr;
      bf_rt_id_t action_id;
    };

    struct AddrExceptTransKey
    {
      uint64_t vaddr;
      uint16_t vaddr_prefix;
    };

    struct AddrExceptTransData
    {
      uint32_t dstAddr;
      uint64_t vaddrToDmaAddr;
      uint8_t permission;
      bf_rt_id_t action_id;
    };

    const bf_dev_port_t dev_cpu_port = 192;
    const bf_dev_port_t dev_recirc_port = 68;

    int get_comp_port(int idx);
    const char *get_comp_ip(int idx);
    const char *get_comp_mac(int idx);
    int get_mem_port(int idx);
    const char *get_mem_ip(int idx);
    const char *get_mem_mac(int idx);
  } // namespace tna_disagg_switch

  namespace tna_disagg_switch
  {
    // This function does the initial setUp of getting bfrtInfo object associated
    // with the P4 program from which all other required objects are obtained
    void setUp();
    void setUp_cache_unmatched_mirror(void);

    // This function does the initial set up of getting key field-ids, action-ids
    // and data field ids associated with the ipRoute table. This is done once
    // during init time.
    void tableSetUp();
    // Initialization from tables
    void tableSetUp_cacheline(const bfrt::BfRtInfo *bfrtInfo);
    void tableSetUp_cache_state(const bfrt::BfRtInfo *bfrtInfo);
    void table_insert_static_state_transitions(void);

    // Helper functions
    void get_table_helper(std::string table_name, const bfrt::BfRtTable **bfrtTable);
    void get_act_id_helper(const bfrt::BfRtTable *bfrtTable, std::string act_name, bf_rt_id_t *act_id);
    void get_key_id_helper(const bfrt::BfRtTable *bfrtTable, std::string key_name, bf_rt_id_t *key_id);
    void get_data_id_helper(const bfrt::BfRtTable *bfrtTable, std::string data_name,
                            bf_rt_id_t action_id, bf_rt_id_t *data_id);
    void get_data_id_helper(const bfrt::BfRtTable *bfrtTable, std::string data_name,
                            bf_rt_id_t *data_id);
    void get_key_data_obj_helper(const bfrt::BfRtTable *bfrtTable,
                                 std::unique_ptr<bfrt::BfRtTableKey> *table_key,
                                 std::unique_ptr<bfrt::BfRtTableData> *table_data);

    bf_rt_target_t get_dev_target(void);
    std::shared_ptr<bfrt::BfRtSession> get_bfrt_session(void);
    std::atomic<unsigned long> *get_num_except_trans_rules(void);
    std::atomic<unsigned long> *get_num_except_trans_pages(void);
    std::atomic<unsigned long> *get_num_except_trans_huge_pages(void);
    std::atomic<unsigned long> *get_num_addr_trans_rules(void);

    void call_table_entry_add_modify(
        std::unique_ptr<bfrt::BfRtTableKey> &_bfrtTableKey,
        std::unique_ptr<bfrt::BfRtTableData> &_bfrtTableData,
        const bfrt::BfRtTable *tar_table,
        const bool &add);

    void call_table_entry_get(
        std::unique_ptr<bfrt::BfRtTableKey> &_bfrtTableKey,
        std::unique_ptr<bfrt::BfRtTableData> &_bfrtTableData,
        const bfrt::BfRtTable *tar_table);
    bf_status_t call_table_entry_get_return_error(
        std::unique_ptr<bfrt::BfRtTableKey> &_bfrtTableKey,
        std::unique_ptr<bfrt::BfRtTableData> &_bfrtTableData,
        const bfrt::BfRtTable *tar_table);

    /*******************************************************************************
     * Utility functions associated with "ipRoute" table in the P4 program.
     ******************************************************************************/

    // This function sets the passed in ip_dst and vrf value into the key object
    // passed using the setValue methods on the key object
    void ipRoute_key_setup(const IpRouteKey &ipRoute_key,
                           bfrt::BfRtTableKey *table_key);

    // This function sets the passed in "route" action data  into the
    // data object associated with the ipRoute table
    void ipRoute_data_setup_for_route(const IpRoute_routeData &ipRoute_data,
                                      bfrt::BfRtTableData *table_data);

    // This functiona sets the passed in "nat" acton data into the
    // data object associated with the ipRoute table and "nat" action within the
    // ipRoute table
    void ipRoute_data_setup_for_nat(const IpRoute_natData &ipRoute_data,
                                    bfrt::BfRtTableData *table_data);

    // This function adds or modifies an entry in the ipRoute table with "route"
    // action. The workflow is similar for either table entry add or modify
    void ipRoute_entry_add_modify_with_route(const IpRouteKey &ipRoute_key,
                                             const IpRoute_routeData &ipRoute_data,
                                             const bool &add);

    // This function adds or modifies an entry in the ipRoute table with "nat"
    // action. The workflow is similar for either table entry add or modify
    void ipRoute_entry_add_modify_with_nat(const IpRouteKey &ipRoute_key,
                                           const IpRoute_natData &ipRoute_data,
                                           const bool &add);

    // This function process the entry obtained by a get call for a "route" action
    // and populates the IpRoute_routeData structure
    void ipRoute_process_route_entry_get(const bfrt::BfRtTableData &data,
                                         IpRoute_routeData *route_data);

    // This function process the entry obtained by a get call for a "nat" action
    // and populates the IpRoute_natData structure
    void ipRoute_process_nat_entry_get(const bfrt::BfRtTableData &data,
                                       IpRoute_natData *nat_data);

    // This function processes the entry obtained by a get call. Based on the action
    // id the data object is intepreted.
    void ipRoute_process_entry_get(const bfrt::BfRtTableData &data,
                                   IpRouteData *ipRoute_data);

    // This function reads an entry specified by the ipRoute_key, and fills in the
    // passedin IpRoute object
    void ipRoute_entry_get(const IpRouteKey &ipRoute_key, IpRouteData *data);

    // This function deletes an entry specified by the ipRoute_key
    void ipRoute_entry_delete(const IpRouteKey &ipRoute_key);

    void table_insert_init_data();

    // Function to iterate over all the entries in the table
    void table_iterate();

    /*******************************************************************************
     * Utility functions used by the exposed APIs
     ******************************************************************************/
    void add_cacheline_rule(uint64_t vaddr, uint16_t vaddr_prefix, uint32_t c_idx);
    void del_cacheline_rule(uint64_t vaddr, uint16_t vaddr_prefix);

    void get_cacheline_reg_rule(uint32_t cache_idx, uint16_t &state, uint16_t &sharer);
    void add_cacheline_reg_rule(uint32_t cache_idx, uint64_t dir_init);
    void del_cacheline_reg_rule(uint32_t cache_idx);
    
    void add_roceReq_rule(const std::string src_ip_addr, const std::string dst_ip_addr,
                          uint32_t qp, uint32_t new_qp, uint32_t rkey, uint16_t reg_idx);
    void add_roceAck_rule(uint32_t qp, // const std::string src_ip_addr,
                          uint32_t new_qp, const std::string new_ip_addr,
                          uint16_t reg_idx);
    void add_roceDummyAck_rule(uint32_t qp, const std::string src_ip_addr,
                               uint32_t new_qp, uint64_t vaddr);
    void add_roceAckDest_rule(uint32_t qp, const std::string src_ip_addr,
                              uint32_t dest_qp_id, uint32_t dummy_qp_id);
    void add_setQpIdx_rule(uint32_t qp, const std::string src_ip_addr,
                           uint16_t qp_id);
    void add_senderQp_rule(uint32_t cpu_qp_id, const std::string src_ip_addr,
                           uint16_t mem_qp_id);

    void add_addrTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix,
                            const std::string dst_ip_addr, uint64_t va_to_dma);
    void del_addrTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix);
    void add_modify_addrExceptTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix,
                                         const std::string dst_ip_addr, uint64_t va_to_dma,
                                         uint8_t permission, int is_add);
    void del_addrExceptTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix);
    // Invalidation generation
    void add_egressInvRoute_rule(int nid, int inv_idx, uint32_t qp, uint32_t rkey, uint64_t vaddr, uint16_t reg_idx);
    // Ack translation rule in egress
    void add_egressAckTrans_rule(const std::string dst_ip_addr, uint32_t qp, uint32_t new_qp,
                                 uint32_t rkey, uint64_t vaddr, uint16_t reg_idx);
    void memory_barrier(void);
  } // namespace tna_disagg_switch
} // namespace bfrt

#ifndef _mem_barrier_
#define _mem_barrier_() asm volatile("" ::: "memory");
#endif

extern "C" void print_bfrt_addr_trans_rule_counters(void);
// Logging
void set_datetime_filename(void);
FILE *open_datetime_file(void);
FILE *get_datetime_filep(void);
void close_datetime_file(void);
#endif
