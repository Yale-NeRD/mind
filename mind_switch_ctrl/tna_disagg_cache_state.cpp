#include <bf_rt/bf_rt_info.hpp>
#include <bf_rt/bf_rt_init.hpp>
#include <bf_rt/bf_rt_common.h>
#include <bf_rt/bf_rt_table_key.hpp>
#include <bf_rt/bf_rt_table_data.hpp>
#include <bf_rt/bf_rt_table.hpp>

// Mirroring
#include <pipe_mgr/pipe_mgr_intf.h>
#include <pipe_mgr/pipe_mgr_mirror_intf.h>

#ifdef __cplusplus
extern "C"
{
#endif

// Multicast
#include <mc_mgr/mc_mgr_intf.h>
#ifdef __cplusplus
}
#endif

#include <getopt.h>
#include <unistd.h>
#include <atomic>
#include "bf_rt_utils.hpp"
#include "tna_disagg_bfrt.hpp"
#include "controller/debug.h"
#include "controller/cacheline_def.h"

#ifdef __cplusplus
extern "C"
{
#endif
#include <bf_switchd/bf_switchd.h>
#ifdef __cplusplus
}
#endif

/***********************************************************************************
 * This sample cpp application code is based on the P4 program
 * tna_disagg_switch.p4
 * Please refer to the P4 program and the generated bf-rt.json for information on
 * the tables contained in the P4 program, and the associated key and data
 * fields.
 **********************************************************************************/
extern "C" void bfrt_add_cachesharer(const char *ip_addr, uint16_t sharer);
extern "C" void bfrt_add_eg_cachesharer(const char *ip_addr, uint16_t sharer);

namespace bfrt
{
  namespace tna_disagg_switch
  {
    namespace{
      // == Cacheline match tables ==
      // 1) states
      const bfrt::BfRtTable *cacheStateTable = nullptr;
      // Key field ids
      bf_rt_id_t cachestate_cur_state_field_id = 0;
      bf_rt_id_t cachestate_permission_field_id = 0;
      bf_rt_id_t cachestate_write_req_field_id = 0;
      // Action Ids
      bf_rt_id_t cachestate_action_id = 0;
      // Data field for cachestate
      bf_rt_id_t cachestate_action_next_state_field_id = 0;
      bf_rt_id_t cachestate_action_reset_sharer_field_id = 0;
      bf_rt_id_t cachestate_action_send_invalidation_field_id = 0;
      // Key and Data object to hold actual values
      std::unique_ptr<bfrt::BfRtTableKey> bfrtCacheStateTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtCacheStateTableData;

      // 2) sharers
      const bfrt::BfRtTable *cacheSharerTable = nullptr;
      // Key field ids
      bf_rt_id_t cachesharer_src_ip_key_id = 0;
      // Action Ids
      bf_rt_id_t cachesharer_action_id = 0;
      // Data field for cacheline
      bf_rt_id_t cachesharer_action_sharer_field_id = 0;
      // Key and Data object to hold actual values
      std::unique_ptr<bfrt::BfRtTableKey> bfrtCacheSharerKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtCacheSharerData;

      // == Cacheline match table for egress ==
      const bfrt::BfRtTable *cacheEgSharerTable = nullptr;
      // Key field ids
      bf_rt_id_t cache_eg_sharer_dst_ip_key_id = 0;
      // Action Ids
      bf_rt_id_t cache_eg_sharer_action_id = 0;
      // Data field for cacheline
      bf_rt_id_t cache_eg_sharer_action_sharer_field_id = 0;
      // Key and Data object to hold actual values
      std::unique_ptr<bfrt::BfRtTableKey> bfrtCacheEgSharerKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtCacheEgSharerData;
    }

    // Cache state
    const uint8_t cache_state_success = CACHELINE_SUCCESS;
    const uint8_t cache_state_shared = CACHELINE_SHARED;
    const uint8_t cache_state_shared_data = CACHELINE_SHARED_DATA;
    const uint8_t cache_state_modified = CACHELINE_MODIFIED;
    const uint8_t cache_state_modified_data = CACHELINE_MODIFIED_DATA;
    const uint8_t cache_state_idle = CACHELINE_IDLE;
    const uint8_t cache_state_fail = CACHELINE_FAIL;
    const uint8_t cache_state_empty = CACHELINE_EMPTY;

    // RoCE read with permission
    const uint8_t cache_no_perm = 0x0;
    const uint8_t cache_read_req = 0x1;
    const uint8_t cache_write = 0x2;      // RoCE write is always with cache_write (0x2)
    const uint8_t cache_write_req = 0x3;  // read (0x1) | write (0x2)
    const uint8_t cache_passthrough = cache_no_perm; // data push for non-target address (eviction/invalidation)
    const uint8_t cache_dum_data = cache_read_req; // data push for non-target address (eviction/invalidation)
    const uint8_t cache_tar_data = cache_write; // data push for non-target address (eviction/invalidation)
    const uint8_t cache_fin_ack = cache_write_req;
    const uint8_t cache_evict = 0x4;
    const uint8_t cache_reset = cache_evict;
  
    // Cache request (is write?)
    const uint8_t cache_request = 0;
    const uint8_t cache_datapush = 1;
    
    // Sharer reset modes
    const uint8_t cache_add_sharer = 0x0;
    const uint8_t cache_reset_sharer = 0x1;
    const uint8_t cache_del_sharer = 0x2;
    const uint8_t cache_same_sharer = 0x3;
    const uint8_t cache_verify_sharer_and_skip = 0x4;
    
    // Sharer invalidation modes
    const uint8_t cache_no_inval = 0x0;
    const uint8_t cache_single_inval = 0x1;
    const uint8_t cache_multi_inval = 0x2;
    const uint8_t cache_forward_data_cpu = 0x3;
    const uint8_t cache_forward_data_mem = 0x4;
    const uint8_t cache_return_write_ack = 0x5;
    const uint8_t cache_return_read_nack = 0x6;  // logically same
    const uint8_t cache_send_ack = 0x0 << 4;
    const uint8_t cache_send_nack = 0x1 << 4;
    const uint8_t cache_no_ack = 0x2 << 4;
    const uint8_t cache_skip_ack = 0x3 << 4;
    const uint8_t cache_forward_mem = 0x0 << 6;
    const uint8_t cache_drop_to_mem = 0x1 << 6;

    // Initial setup functions
    void tableSetUp_cache_state(const bfrt::BfRtInfo *bfrtInfo)
    {
      (void)bfrtInfo;
      // == Cachestate match table ==//
      get_table_helper("SwitchIngress.cacheDirectoryPre2", &cacheStateTable);
      // action id
      get_act_id_helper(cacheStateTable, "SwitchIngress.get_next_state_and_sharers", &cachestate_action_id);
      // key field
      get_key_id_helper(cacheStateTable, "hdr.recirc_data.cur_state", &cachestate_cur_state_field_id);
      get_key_id_helper(cacheStateTable, "ig_md.perm", &cachestate_permission_field_id);
      get_key_id_helper(cacheStateTable, "ig_md.do_roce_req_w", &cachestate_write_req_field_id);
      // data field
      get_data_id_helper(cacheStateTable, "next_state", cachestate_action_id, &cachestate_action_next_state_field_id);
      get_data_id_helper(cacheStateTable, "reset_sharer", cachestate_action_id, &cachestate_action_reset_sharer_field_id);
      get_data_id_helper(cacheStateTable, "send_inval", cachestate_action_id, &cachestate_action_send_invalidation_field_id);
      // Allocate key and data
      get_key_data_obj_helper(cacheStateTable, &bfrtCacheStateTableKey, &bfrtCacheStateTableData);

      // == Cachesharer match table ==//
      get_table_helper("SwitchIngress.getRequesterNode", &cacheSharerTable);
      // action id
      get_act_id_helper(cacheSharerTable, "SwitchIngress.get_current_node_as_sharer", &cachesharer_action_id);
      // key field
      get_key_id_helper(cacheSharerTable, "hdr.ipv4.src_addr", &cachesharer_src_ip_key_id);
      // data field
      get_data_id_helper(cacheSharerTable, "cur_node", cachesharer_action_id, &cachesharer_action_sharer_field_id);      
      // Allocate key and data
      get_key_data_obj_helper(cacheSharerTable, &bfrtCacheSharerKey, &bfrtCacheSharerData);

      // == Cachesharer match table for egress ==//
      get_table_helper("SwitchEgress.getDestNodeMask", &cacheEgSharerTable);
      // action id
      get_act_id_helper(cacheEgSharerTable, "SwitchEgress.get_dst_node_as_sharer", &cache_eg_sharer_action_id);
      // key field
      get_key_id_helper(cacheEgSharerTable, "hdr.ipv4.dst_addr", &cache_eg_sharer_dst_ip_key_id);
      // data field
      get_data_id_helper(cacheEgSharerTable, "cur_node", cache_eg_sharer_action_id, &cache_eg_sharer_action_sharer_field_id);      
      // Allocate key and data
      get_key_data_obj_helper(cacheEgSharerTable, &bfrtCacheEgSharerKey, &bfrtCacheEgSharerData);

      // Dummy
      parse_mac("aa:bb:cc:dd:ee:ff");
    }

    void add_cachestate_rule(uint8_t cur_state, uint8_t perm, uint8_t write_req,
                             uint8_t next_state, uint8_t reset_sharer, uint8_t send_inval);
    static void _add_cachestate_rule(uint8_t cur_state, uint8_t perm, uint8_t write_req,
                                     uint8_t next_state, uint8_t reset_sharer, uint8_t send_inval)
    {
      add_cachestate_rule(cur_state, perm, write_req, next_state, reset_sharer, send_inval);

      pr_cache("State rules: (state: %u, perm: %u, w_req: %u) -> (state: %u, rst: %u, ival: 0x%x)\n",
               (unsigned int)cur_state, (unsigned int)perm, (unsigned int)write_req,
               (unsigned int)next_state, (unsigned int)reset_sharer, (unsigned int)send_inval);
    }

    void table_insert_static_state_transitions(void)
    {
      // I -> S, M
      _add_cachestate_rule(cache_state_idle, cache_read_req, cache_request,
                           cache_state_shared, cache_reset_sharer, cache_no_inval);
      _add_cachestate_rule(cache_state_idle, cache_write_req, cache_request,
                           cache_state_modified, cache_reset_sharer, cache_no_inval);
      // I -> I (cache_dum_data, cache_tar_data)
      _add_cachestate_rule(cache_state_idle, cache_dum_data, cache_datapush,
                           cache_state_idle, cache_same_sharer,
                           cache_no_inval | cache_skip_ack | cache_drop_to_mem);
      _add_cachestate_rule(cache_state_idle, cache_tar_data, cache_datapush,
                           cache_state_idle, cache_same_sharer,
                           cache_no_inval | cache_skip_ack | cache_drop_to_mem);
      // I -> I (intial data push - passthrough)
      _add_cachestate_rule(cache_state_idle, cache_passthrough, cache_datapush,
                           cache_state_idle, cache_same_sharer, cache_no_inval | cache_no_ack);
      // I -> I (in cache region Fin acks)
      _add_cachestate_rule(cache_state_idle, cache_evict, cache_datapush,
                           cache_state_idle, cache_same_sharer,
                           cache_return_write_ack | cache_skip_ack | cache_drop_to_mem);
      // Reordered/Retransmitted Fin ACK: no action needed (just RDMA ack back to the sender)
      _add_cachestate_rule(cache_state_idle, cache_fin_ack, cache_datapush,
                           cache_state_idle, cache_same_sharer,
                           cache_return_write_ack | cache_skip_ack | cache_drop_to_mem);
      // S -> S (shared req)
      _add_cachestate_rule(cache_state_shared, cache_read_req, cache_request,
                           cache_state_shared, cache_add_sharer, cache_no_inval);
      // S -> M^D (modified request) - BUT will be M if the requester is the only owner of S
      _add_cachestate_rule(cache_state_shared, cache_write_req, cache_request,
                           cache_state_modified_data, cache_verify_sharer_and_skip | cache_add_sharer,
                           cache_multi_inval | cache_no_ack);
      // S -> S or I (data push)
      // Eviction ACK
      _add_cachestate_rule(cache_state_shared, cache_evict, cache_datapush,
                           cache_state_shared, cache_del_sharer,
                           cache_return_write_ack | cache_skip_ack | cache_drop_to_mem);
      // Retransmitted Fin ACK: no action needed (just RDMA ack back to the sender)
      _add_cachestate_rule(cache_state_shared, cache_fin_ack, cache_datapush,
                           cache_state_shared, cache_same_sharer,
                           cache_return_write_ack | cache_skip_ack | cache_drop_to_mem);

      // Data
      _add_cachestate_rule(cache_state_shared, cache_passthrough, cache_datapush,
                           cache_state_shared, cache_same_sharer, cache_no_inval | cache_no_ack);

      // M -> M^D (modified request), M -> S^D (shared request)
      _add_cachestate_rule(cache_state_modified, cache_write_req, cache_request,
                           cache_state_modified_data, cache_verify_sharer_and_skip | cache_add_sharer,
                           cache_single_inval | cache_no_ack | cache_drop_to_mem);
      _add_cachestate_rule(cache_state_modified, cache_read_req, cache_request,
                           cache_state_shared_data, cache_verify_sharer_and_skip | cache_add_sharer,
                           cache_single_inval | cache_no_ack | cache_drop_to_mem);
      // M -> I (data)
      // Eviction ACK
      _add_cachestate_rule(cache_state_modified, cache_evict, cache_datapush,
                           cache_state_idle, cache_del_sharer,
                           cache_return_write_ack | cache_skip_ack | cache_drop_to_mem);
      // Retransmitted Fin ACK: no action needed (just RDMA ack back to the sender)
      _add_cachestate_rule(cache_state_modified, cache_fin_ack, cache_datapush,
                           cache_state_modified, cache_same_sharer,
                           cache_return_write_ack | cache_skip_ack | cache_drop_to_mem);
      // Data
      _add_cachestate_rule(cache_state_modified, cache_passthrough, cache_datapush,
                           cache_state_modified, cache_same_sharer, cache_no_inval | cache_no_ack);

      // S^D -> S^D (data push, target data)
      // A1. Forwarding data
      _add_cachestate_rule(cache_state_shared_data, cache_tar_data, cache_datapush,
                           cache_state_shared_data, cache_same_sharer,
                           cache_forward_data_cpu | cache_skip_ack); // directly forward to the cpu
      // A2. Forwarding data - dummy version - req (from owner) to req (from requester) conversion
      _add_cachestate_rule(cache_state_shared_data, cache_dum_data, cache_datapush,
                           cache_state_shared_data, cache_same_sharer,
                           cache_forward_data_mem | cache_skip_ack | cache_drop_to_mem); // need to get data from memory
      // B. S^D -> S^D (invalidation but not the target data = passthrough)
      _add_cachestate_rule(cache_state_shared_data, cache_passthrough, cache_datapush,
                           cache_state_shared_data, cache_same_sharer, cache_no_inval | cache_no_ack);
      _add_cachestate_rule(cache_state_shared_data, cache_evict, cache_datapush,
                           cache_state_shared_data, cache_same_sharer,
                           cache_return_write_ack | cache_skip_ack | cache_drop_to_mem);
      // C. S^D -> S (invalidation finished ack: change state and drop the packet)
      _add_cachestate_rule(cache_state_shared_data, cache_fin_ack, cache_datapush,
                           cache_state_shared, cache_same_sharer, 
                           cache_return_write_ack | cache_skip_ack | cache_drop_to_mem);
      // S^D -> S^D (NACKs)
      _add_cachestate_rule(cache_state_shared_data, cache_write_req, cache_request,
                           cache_state_shared_data, cache_same_sharer,
                           //  cache_no_inval | cache_send_nack);
                           cache_return_read_nack | cache_send_nack | cache_drop_to_mem);
      _add_cachestate_rule(cache_state_shared_data, cache_read_req, cache_request,
                           cache_state_shared_data, cache_same_sharer,
                           //  cache_no_inval | cache_send_nack);
                           cache_return_read_nack | cache_send_nack | cache_drop_to_mem);

      // M^D -> M^D (data push, target data)
      // A1. Forwarding data
      _add_cachestate_rule(cache_state_modified_data, cache_tar_data, cache_datapush,
                           cache_state_modified_data, cache_same_sharer,
                           cache_forward_data_cpu | cache_skip_ack);
      // A2. Forwarding data - dummy version - req (from owner) to req (from requester) conversion
      _add_cachestate_rule(cache_state_modified_data, cache_dum_data, cache_datapush,
                           cache_state_modified_data, cache_same_sharer,
                           cache_forward_data_mem | cache_skip_ack | cache_drop_to_mem);
      // B. M^D -> M^D (invalidation but not the target data = passthrough)
      _add_cachestate_rule(cache_state_modified_data, cache_passthrough, cache_datapush,
                           cache_state_modified_data, cache_same_sharer, cache_no_inval | cache_no_ack);
      _add_cachestate_rule(cache_state_modified_data, cache_evict, cache_datapush,
                           cache_state_modified_data, cache_same_sharer, // it should send final ACK for eviction
                           cache_return_write_ack | cache_skip_ack | cache_drop_to_mem);
      // C. M^D -> M - BUT will be M if the switch detect the last ACK
      _add_cachestate_rule(cache_state_modified_data, cache_fin_ack, cache_datapush,
                           cache_state_modified_data, cache_del_sharer,
                           cache_return_write_ack | cache_skip_ack | cache_drop_to_mem); // if still request in sharer list
      // M^D -> M^D (NACKs)
      _add_cachestate_rule(cache_state_modified_data, cache_write_req, cache_request,
                           cache_state_modified_data, cache_same_sharer,
                           //  cache_no_inval | cache_send_nack);
                           cache_return_read_nack | cache_send_nack | cache_drop_to_mem);
      _add_cachestate_rule(cache_state_modified_data, cache_read_req, cache_request,
                           cache_state_modified_data, cache_same_sharer,
                           //  cache_no_inval | cache_send_nack);
                           cache_return_read_nack | cache_send_nack | cache_drop_to_mem);

      // C. edge cases: reordered data and ack
      _add_cachestate_rule(cache_state_shared, cache_tar_data, cache_datapush,
                           cache_state_shared, cache_same_sharer,
                           cache_forward_data_cpu | cache_skip_ack);
      _add_cachestate_rule(cache_state_shared, cache_dum_data, cache_datapush,
                           cache_state_shared, cache_same_sharer,
                           cache_forward_data_mem | cache_skip_ack | cache_drop_to_mem);
      _add_cachestate_rule(cache_state_modified, cache_tar_data, cache_datapush,
                           cache_state_modified, cache_same_sharer,
                           cache_forward_data_cpu | cache_skip_ack);
      _add_cachestate_rule(cache_state_modified, cache_dum_data, cache_datapush,
                           cache_state_modified, cache_same_sharer,
                           cache_forward_data_mem | cache_skip_ack | cache_drop_to_mem);
    }

    /*******************************************************************************
     * Utility functions for "cacheDirectoryPre2"
     ******************************************************************************/
    void cachestate_key_setup(const CacheStateTransKey &state_key,
                                bfrt::BfRtTableKey *table_key)
    {
      auto bf_status =
          table_key->setValue(cachestate_cur_state_field_id,
                              static_cast<uint64_t>(state_key.cur_state));
      assert(bf_status == BF_SUCCESS);

      bf_status =
          table_key->setValue(cachestate_permission_field_id,
                              static_cast<uint64_t>(state_key.permission));
      assert(bf_status == BF_SUCCESS);

      bf_status =
          table_key->setValue(cachestate_write_req_field_id,
                              static_cast<uint64_t>(state_key.write_req));
      assert(bf_status == BF_SUCCESS);
      return;
    }

    void
    cachestate_data_setup(const CacheStateTransData &state_data,
                          bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(cachestate_action_next_state_field_id,
                               static_cast<uint64_t>(state_data.next_state));
      assert(bf_status == BF_SUCCESS);

      bf_status =
          table_data->setValue(cachestate_action_reset_sharer_field_id,
                               static_cast<uint64_t>(state_data.reset_sharer));
      assert(bf_status == BF_SUCCESS);

      bf_status =
          table_data->setValue(cachestate_action_send_invalidation_field_id,
                               static_cast<uint64_t>(state_data.send_inval));
      assert(bf_status == BF_SUCCESS);
    }

    void cachestate_entry_add_modify(const CacheStateTransKey &state_key,
                                     const CacheStateTransData &state_data,
                                     const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      cacheStateTable->keyReset(bfrtCacheStateTableKey.get());
      cacheStateTable->dataReset(cachestate_action_id, bfrtCacheStateTableData.get());

      // Fill in the Key and Data object
      cachestate_key_setup(state_key, bfrtCacheStateTableKey.get());
      cachestate_data_setup(state_data, bfrtCacheStateTableData.get());

      call_table_entry_add_modify(
          bfrtCacheStateTableKey, bfrtCacheStateTableData, cacheStateTable, add);
    }

    void
    cachestate_entry_get(const CacheStateTransKey &state_key, CacheStateTransData *state_data)
    {
      bf_status_t status = BF_SUCCESS;

      // Reset key and data before use
      cacheStateTable->keyReset(bfrtCacheStateTableKey.get());
      cacheStateTable->dataReset(cachestate_action_id, bfrtCacheStateTableData.get());

      cachestate_key_setup(state_key, bfrtCacheStateTableKey.get());
      status = call_table_entry_get_return_error(
          bfrtCacheStateTableKey, bfrtCacheStateTableData, cacheStateTable);
      assert(status == BF_SUCCESS);
      
      uint64_t edata;
      status = bfrtCacheStateTableData->getValue(cachestate_action_next_state_field_id, &edata);
      state_data->next_state = static_cast<uint8_t>(edata);
      assert(status == BF_SUCCESS);

      status = bfrtCacheStateTableData->getValue(cachestate_action_reset_sharer_field_id, &edata);
      state_data->reset_sharer = static_cast<uint8_t>(edata);
      assert(status == BF_SUCCESS);

      status = bfrtCacheStateTableData->getValue(cachestate_action_send_invalidation_field_id, &edata);
      state_data->send_inval = static_cast<uint8_t>(edata);
      assert(status == BF_SUCCESS);
    }

    void cachestate_entry_delete(const CacheStateTransKey &state_key)
    {
      // Reset key before use
      cacheStateTable->keyReset(bfrtCacheStateTableKey.get());
      cachestate_key_setup(state_key, bfrtCacheStateTableKey.get());

      std::shared_ptr<bfrt::BfRtSession> session = get_bfrt_session();
      auto status = cacheStateTable->tableEntryDel(
          *session, get_dev_target(), *bfrtCacheStateTableKey);
      assert(status == BF_SUCCESS);
      session->sessionCompleteOperations();
      return;
    }

    void get_cachestate_rule(uint8_t cur_state, uint8_t perm, uint8_t write_req,
                             uint8_t &next_state, uint8_t &reset_sharer, uint8_t &send_inval)
    {
      // Allocate initial data for servers
      CacheStateTransKey c_key;
      CacheStateTransData c_data;

      c_key.cur_state = cur_state;
      c_key.permission = perm;
      c_key.write_req = write_req;
      cachestate_entry_get(c_key, &c_data);

      next_state = c_data.next_state;
      reset_sharer = c_data.reset_sharer;
      send_inval = c_data.send_inval;
    }

    void add_cachestate_rule(uint8_t cur_state, uint8_t perm, uint8_t write_req,
                             uint8_t next_state, uint8_t reset_sharer, uint8_t send_inval)
    {
      // Allocate initial data for servers
      CacheStateTransKey c_key;
      CacheStateTransData c_data;

      c_key.cur_state = cur_state;
      c_key.permission = perm;
      c_key.write_req = write_req;
      c_data.next_state = next_state;
      c_data.reset_sharer = reset_sharer;
      c_data.send_inval = send_inval;
      cachestate_entry_add_modify(c_key, c_data, true);
    }

    void del_cachestate_rule(uint8_t cur_state, uint8_t perm, uint8_t write_req)
    {
      CacheStateTransKey c_key;

      c_key.cur_state = cur_state;
      c_key.permission = perm;
      c_key.write_req = write_req;
      cachestate_entry_delete(c_key);
    }

    /*******************************************************************************
     * Utility functions associated with "getRequesterNode"
     ******************************************************************************/
    void
    cachesharer_key_setup(const CacheStateSharerKey &cachesharer_key,
                          bfrt::BfRtTableKey *table_key)
    {
      auto bf_status =
          table_key->setValue(cachesharer_src_ip_key_id,
                              static_cast<uint64_t>(cachesharer_key.ipSrcAddr));
      assert(bf_status == BF_SUCCESS);
      return;
    }

    void
    cachesharer_data_setup(const CacheStateSharerData &cachesharer_data,
                           bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(cachesharer_action_sharer_field_id,
                               static_cast<uint64_t>(cachesharer_data.sharer));
      assert(bf_status == BF_SUCCESS);
    }

    void cachesharer_entry_add_modify(const CacheStateSharerKey &cachesharer_key,
                                      const CacheStateSharerData &cachesharer_data,
                                      const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      cacheSharerTable->keyReset(bfrtCacheSharerKey.get());
      cacheSharerTable->dataReset(cachesharer_action_id, bfrtCacheSharerData.get());

      // Fill in the Key and Data object
      cachesharer_key_setup(cachesharer_key, bfrtCacheSharerKey.get());
      cachesharer_data_setup(cachesharer_data, bfrtCacheSharerData.get());

      call_table_entry_add_modify(
          bfrtCacheSharerKey, bfrtCacheSharerData, cacheSharerTable, add);
    }

    void cachesharer_entry_get(const CacheStateSharerKey &cachesharer_key,
                               CacheStateSharerData *cachesharer_data)
    {
      // Reset key and data before use
      cacheSharerTable->keyReset(bfrtCacheSharerKey.get());
      cacheSharerTable->dataReset(cachesharer_action_id, bfrtCacheSharerData.get());

      cachesharer_key_setup(cachesharer_key, bfrtCacheSharerKey.get());
      call_table_entry_get(bfrtCacheSharerKey, bfrtCacheSharerData, cacheSharerTable);

      bf_status_t status = BF_SUCCESS;
      uint64_t edata;
      status = bfrtCacheSharerData->getValue(cachesharer_action_sharer_field_id, &edata);
      cachesharer_data->sharer = static_cast<uint16_t>(edata);
      assert(status == BF_SUCCESS);
    }

    void cachesharer_reg_delete(const CacheStateSharerKey &cachesharer_key)
    {
      // Reset key before use
      cacheSharerTable->keyReset(bfrtCacheSharerKey.get());
      cachesharer_key_setup(cachesharer_key, bfrtCacheSharerKey.get());

      std::shared_ptr<bfrt::BfRtSession> session = get_bfrt_session();
      auto status = cacheSharerTable->tableEntryDel(
          *session, get_dev_target(), *bfrtCacheSharerKey);
      assert(status == BF_SUCCESS);
      session->sessionCompleteOperations();
      return;
    }

    void get_cachesharer_rule(uint32_t src_ip, uint16_t &sharer)
    {
      CacheStateSharerKey c_key;
      CacheStateSharerData c_data;

      c_key.ipSrcAddr = src_ip;
      cachesharer_entry_get(c_key, &c_data);
      sharer = c_data.sharer;
    }

    void add_cachesharer_rule(uint32_t src_ip, uint16_t sharer)
    {
      // Allocate initial data for servers
      CacheStateSharerKey c_key;
      CacheStateSharerData c_data;

      c_key.ipSrcAddr = src_ip;
      c_data.sharer = sharer;
      cachesharer_entry_add_modify(c_key, c_data, true);
    }

    void del_cachesharer_rule(uint32_t src_ip)
    {
      CacheStateSharerKey c_key;

      c_key.ipSrcAddr = src_ip;
      cachesharer_reg_delete(c_key);
    }

    /*******************************************************************************
     * Utility functions associated with "getDestNodeMask"
     ******************************************************************************/
    void
    cache_eg_sharer_key_setup(const CacheEgSharerKey &cachesharer_key,
                              bfrt::BfRtTableKey *table_key)
    {
      auto bf_status =
          table_key->setValue(cache_eg_sharer_dst_ip_key_id,
                              static_cast<uint64_t>(cachesharer_key.ipDstAddr));
      assert(bf_status == BF_SUCCESS);
      return;
    }

    void
    cache_eg_sharer_data_setup(const CacheEgSharerData &cachesharer_data,
                               bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(cache_eg_sharer_action_sharer_field_id,
                               static_cast<uint64_t>(cachesharer_data.sharer));
      assert(bf_status == BF_SUCCESS);
    }

    void cache_eg_sharer_entry_add_modify(const CacheEgSharerKey &cachesharer_key,
                                          const CacheEgSharerData &cachesharer_data,
                                          const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      cacheEgSharerTable->keyReset(bfrtCacheEgSharerKey.get());
      cacheEgSharerTable->dataReset(cache_eg_sharer_action_id, bfrtCacheEgSharerData.get());

      // Fill in the Key and Data object
      cache_eg_sharer_key_setup(cachesharer_key, bfrtCacheEgSharerKey.get());
      cache_eg_sharer_data_setup(cachesharer_data, bfrtCacheEgSharerData.get());

      call_table_entry_add_modify(
          bfrtCacheEgSharerKey, bfrtCacheEgSharerData, cacheEgSharerTable, add);
    }

    void cache_eg_sharer_entry_get(const CacheEgSharerKey &cachesharer_key,
                                   CacheEgSharerData *cachesharer_data)
    {
      // Reset key and data before use
      cacheEgSharerTable->keyReset(bfrtCacheEgSharerKey.get());
      cacheEgSharerTable->dataReset(cache_eg_sharer_action_id, bfrtCacheEgSharerData.get());

      cache_eg_sharer_key_setup(cachesharer_key, bfrtCacheEgSharerKey.get());
      call_table_entry_get(bfrtCacheEgSharerKey, bfrtCacheEgSharerData, cacheEgSharerTable);

      bf_status_t status = BF_SUCCESS;
      uint64_t edata;
      status = bfrtCacheEgSharerData->getValue(cache_eg_sharer_action_sharer_field_id, &edata);
      cachesharer_data->sharer = static_cast<uint16_t>(edata);
      assert(status == BF_SUCCESS);
    }

    void cache_eg_sharer_reg_delete(const CacheEgSharerKey &cachesharer_key)
    {
      // Reset key before use
      cacheEgSharerTable->keyReset(bfrtCacheEgSharerKey.get());
      cache_eg_sharer_key_setup(cachesharer_key, bfrtCacheEgSharerKey.get());

      std::shared_ptr<bfrt::BfRtSession> session = get_bfrt_session();
      auto status = cacheEgSharerTable->tableEntryDel(
          *session, get_dev_target(), *bfrtCacheEgSharerKey);
      assert(status == BF_SUCCESS);
      session->sessionCompleteOperations();
      return;
    }

    void get_cache_eg_sharer_rule(uint32_t dst_ip, uint16_t &sharer)
    {
      CacheEgSharerKey c_key;
      CacheEgSharerData c_data;

      c_key.ipDstAddr = dst_ip;
      cache_eg_sharer_entry_get(c_key, &c_data);
      sharer = c_data.sharer;
    }

    void add_cache_eg_sharer_rule(uint32_t dst_ip, uint16_t sharer)
    {
      // Allocate initial data for servers
      CacheEgSharerKey c_key;
      CacheEgSharerData c_data;

      c_key.ipDstAddr = dst_ip;
      c_data.sharer = sharer;
      cache_eg_sharer_entry_add_modify(c_key, c_data, true);
    }

    void del_cache_eg_sharer_rule(uint32_t dst_ip)
    {
      CacheEgSharerKey c_key;

      c_key.ipDstAddr = dst_ip;
      cache_eg_sharer_reg_delete(c_key);
    }
  } // namespace tna_disagg_switch
} // namespace bfrt

// Exposed APIs
// Cacheline/directory
extern "C" void bfrt_add_cachestate(uint8_t cur_state, uint8_t perm, uint8_t write_req,
                                    uint8_t next_state, uint8_t reset_sharer, uint8_t send_inval);
void bfrt_add_cachestate(uint8_t cur_state, uint8_t perm, uint8_t write_req,
                         uint8_t next_state, uint8_t reset_sharer, uint8_t send_inval)
{
  bfrt::tna_disagg_switch::add_cachestate_rule(cur_state, perm, write_req,
                                               next_state, reset_sharer, send_inval);
}

extern "C" void bfrt_get_cachestate(uint8_t cur_state, uint8_t perm, uint8_t write_req,
                                   uint8_t *next_state, uint8_t *reset_sharer, uint8_t *send_inval);
void bfrt_get_cachestate(uint8_t cur_state, uint8_t perm, uint8_t write_req,
                         uint8_t *next_state, uint8_t *reset_sharer, uint8_t *send_inval)
{
  uint8_t n_st, rst, inval;
  bfrt::tna_disagg_switch::get_cachestate_rule(cur_state, perm, write_req, n_st, rst, inval);
  (*next_state) = n_st;
  (*reset_sharer) = rst;
  (*send_inval) = inval;
}

void bfrt_add_cachesharer(const char *ip_addr, uint16_t sharer)
{
  std::string ip_addr_str = ip_addr;
  uint32_t src_ip = parse_ipv4(ip_addr_str);
  bfrt::tna_disagg_switch::add_cachesharer_rule(src_ip, sharer);

  pr_rule("Sharer mask [static]: %s -> 0x%x\n", ip_addr, (unsigned int)sharer);
}

extern "C" void bfrt_del_cachesharer(const char* ip_addr);
void bfrt_del_cachesharer(const char *ip_addr)
{
  std::string ip_addr_str = ip_addr;
  uint32_t src_ip = parse_ipv4(ip_addr_str);
  bfrt::tna_disagg_switch::del_cachesharer_rule(src_ip);
}

extern "C" void bfrt_get_cachesharer(const char *ip_addr, uint16_t *sharer);
void bfrt_get_cachesharer(const char *ip_addr, uint16_t *sharer)
{
  std::string ip_addr_str = ip_addr;
  uint32_t src_ip = parse_ipv4(ip_addr_str);
  uint16_t _sharer;
  bfrt::tna_disagg_switch::get_cachesharer_rule(src_ip, _sharer);
  (*sharer) = _sharer;
}

void bfrt_add_eg_cachesharer(const char *ip_addr, uint16_t sharer)
{
  std::string ip_addr_str = ip_addr;
  uint32_t dst_ip = parse_ipv4(ip_addr_str);
  bfrt::tna_disagg_switch::add_cache_eg_sharer_rule(dst_ip, sharer);

  pr_rule("Sharer egress mask [static]: %s -> 0x%x\n", ip_addr, (unsigned int)sharer);
}
