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

#include <pthread.h>
#include <getopt.h>
#include <unistd.h>
#include <atomic>
#include "tna_disagg_bfrt.hpp"
#include "controller/debug.h"
#include "controller/config.h"

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

namespace bfrt
{
  namespace tna_disagg_switch
  {
    namespace{
      // Cacheline match table
      const bfrt::BfRtTable *cachelineTable = nullptr;
      // Key field ids
      bf_rt_id_t cacheline_vaddr_field_id = 0;
      // Action Ids
      bf_rt_id_t cacheline_action_id = 0;
      // Data field for cacheline
      bf_rt_id_t cacheline_action_cache_idx_field_id = 0;
      // Key and Data object to hold actual values
      std::unique_ptr<bfrt::BfRtTableKey> bfrtCachelineTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtCachelineTableData;

      // Cacheline match table registers
      const bfrt::BfRtTable *cachelineStateRegTable = nullptr;
      const bfrt::BfRtTable *cachelineSharerRegTable = nullptr;
      // Key and data field ids for state register
      bf_rt_id_t cacheline_state_reg_key_id = 0;
      bf_rt_id_t cacheline_reg_data_update_id = 0;
      bf_rt_id_t cacheline_reg_data_state_id = 0;
      // Key and data field ids for sharer register
      bf_rt_id_t cacheline_sharer_reg_key_id = 0;
      bf_rt_id_t cacheline_reg_data_sharer_id = 0;
      // Key and Data object to hold actual values
      std::unique_ptr<bfrt::BfRtTableKey> bfrtCachelineStateRegKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtCachelineStateRegData;
      std::unique_ptr<bfrt::BfRtTableKey> bfrtCachelineSharerRegKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtCachelineSharerRegData;

      // Directory size, locking flag, invalidation counter registers
      const bfrt::BfRtTable *cacheDirSizeRegTable = nullptr;
      const bfrt::BfRtTable *cacheDirLockRegTable = nullptr;
      const bfrt::BfRtTable *cacheInvCntRegTable = nullptr;
      // Key and data field ids for directory size register
      bf_rt_id_t cacheline_dir_size_reg_key_id = 0;
      bf_rt_id_t cacheline_dir_size_reg_data_id = 0;
      // Key and data field ids for directory lock register
      bf_rt_id_t cacheline_dir_lock_reg_key_id = 0;
      bf_rt_id_t cacheline_dir_lock_reg_data_id = 0;
      // Key and data field ids for invalidation counter register
      bf_rt_id_t cacheline_inv_cnt_reg_key_id = 0;
      bf_rt_id_t cacheline_inv_cnt_reg_data_id = 0;
      // Key and Data object to hold actual values
      std::unique_ptr<bfrt::BfRtTableKey> bfrtCachelineDirSizeKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtCachelineDirSizeData;
      std::unique_ptr<bfrt::BfRtTableKey> bfrtCachelineDirLockKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtCachelineDirLockData;
      std::unique_ptr<bfrt::BfRtTableKey> bfrtCachelineInvCntKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtCachelineInvCntData;
    }

    static pthread_spinlock_t cacheline_bfrt_lock;

    static void cacheline_init(void)
    {
        pthread_spin_init(&cacheline_bfrt_lock, PTHREAD_PROCESS_PRIVATE);
    }

    void tableSetUp_cacheline(const bfrt::BfRtInfo *bfrtInfo)
    {
      (void)bfrtInfo;
      cacheline_init();

      // == Cacheline match table ==//
      get_table_helper("SwitchIngress.findCacheSlot", &cachelineTable);
      // action id
      get_act_id_helper(cachelineTable, "SwitchIngress.cache_found", &cacheline_action_id);
      // key field
      get_key_id_helper(cachelineTable, "hdr.roce_r.vaddr", &cacheline_vaddr_field_id);
      // data field
      get_data_id_helper(cachelineTable, "cache_idx", cacheline_action_id, &cacheline_action_cache_idx_field_id);
      // Allocate key and data
      get_key_data_obj_helper(cachelineTable, &bfrtCachelineTableKey, &bfrtCachelineTableData);

      // == Cacheline match table registers ==//
      get_table_helper("SwitchIngress.cache_dir_state_reg", &cachelineStateRegTable);
      get_table_helper("SwitchIngress.cache_dir_sharer_reg", &cachelineSharerRegTable);
      // key field
      get_key_id_helper(cachelineStateRegTable, "$REGISTER_INDEX", &cacheline_state_reg_key_id);
      get_key_id_helper(cachelineSharerRegTable, "$REGISTER_INDEX", &cacheline_sharer_reg_key_id);
      // data field
      get_data_id_helper(cachelineStateRegTable, "SwitchIngress.cache_dir_state_reg.state", &cacheline_reg_data_state_id);
      get_data_id_helper(cachelineStateRegTable, "SwitchIngress.cache_dir_state_reg.on_update", &cacheline_reg_data_update_id);
      get_data_id_helper(cachelineSharerRegTable, "SwitchIngress.cache_dir_sharer_reg.f1", &cacheline_reg_data_sharer_id);
      // Allocate key and data
      get_key_data_obj_helper(cachelineStateRegTable, &bfrtCachelineStateRegKey, &bfrtCachelineStateRegData);
      get_key_data_obj_helper(cachelineSharerRegTable, &bfrtCachelineSharerRegKey, &bfrtCachelineSharerRegData);

      // == Cacheline directory related registers ==//
      get_table_helper("SwitchIngress.cache_dir_size", &cacheDirSizeRegTable);
      get_table_helper("SwitchIngress.cache_dir_lock_reg", &cacheDirLockRegTable);
      get_table_helper("SwitchIngress.cache_dir_inv_cnt", &cacheInvCntRegTable);
      // key field
      get_key_id_helper(cacheDirSizeRegTable, "$REGISTER_INDEX", &cacheline_dir_size_reg_key_id);
      get_key_id_helper(cacheDirLockRegTable, "$REGISTER_INDEX", &cacheline_dir_lock_reg_key_id);
      get_key_id_helper(cacheInvCntRegTable, "$REGISTER_INDEX", &cacheline_inv_cnt_reg_key_id);
      // data field
      get_data_id_helper(cacheDirSizeRegTable, "SwitchIngress.cache_dir_size.f1", &cacheline_dir_size_reg_data_id);
      get_data_id_helper(cacheDirLockRegTable, "SwitchIngress.cache_dir_lock_reg.f1", &cacheline_dir_lock_reg_data_id);
      get_data_id_helper(cacheInvCntRegTable, "SwitchIngress.cache_dir_inv_cnt.f1", &cacheline_inv_cnt_reg_data_id);
      // Allocate key and data
      get_key_data_obj_helper(cacheDirSizeRegTable, &bfrtCachelineDirSizeKey, &bfrtCachelineDirSizeData);
      get_key_data_obj_helper(cacheDirLockRegTable, &bfrtCachelineDirLockKey, &bfrtCachelineDirLockData);
      get_key_data_obj_helper(cacheInvCntRegTable, &bfrtCachelineInvCntKey, &bfrtCachelineInvCntData);
    }

    static void setUP_default_mirror(bf_dev_target_t &dev_tgt_c, bf_rt_id_t bf_pipe_session,
                                     bf_dev_port_t eg_port, bf_mirror_id_t mir_sess_id)
    {
      bf_mirror_session_info_t mir_sess_info;
      memset(&mir_sess_info, 0, sizeof(mir_sess_info));
      mir_sess_info.mirror_type = BF_MIRROR_TYPE_NORM;
      mir_sess_info.dir = BF_DIR_INGRESS;
      mir_sess_info.ucast_egress_port = eg_port;
      mir_sess_info.ucast_egress_port_v = true;
      mir_sess_info.max_pkt_len = 9216;
      mir_sess_info.copy_to_cpu = false;
      mir_sess_info.icos_for_copy_to_cpu = false;
      bf_status_t bf_status = bf_mirror_session_set(bf_pipe_session, dev_tgt_c, mir_sess_id, &mir_sess_info, true);
      assert(bf_status == BF_SUCCESS);
    }

    // Initial setup functions
    void setUp_cache_unmatched_mirror(void)
    {
      // Get and setup mirror session
      bf_dev_target_t dev_tgt_c;
      bf_rt_target_t dev_tgt = get_dev_target();
      std::shared_ptr<bfrt::BfRtSession> session = get_bfrt_session();
      auto bf_pipe_session = session->sessHandleGet();
      dev_tgt_c.dev_pipe_id = dev_tgt.pipe_id;
      dev_tgt_c.device_id = dev_tgt.dev_id;

      setUP_default_mirror(dev_tgt_c, bf_pipe_session, dev_cpu_port, 1);
      setUP_default_mirror(dev_tgt_c, bf_pipe_session, dev_recirc_port, 3);

      // Get and setup multicast session: use rid=0xffff, mgid=0
      auto bf_mc_session = session->preSessHandleGet(); //bf_mc_session_hdl_t
      bf_mc_grp_id_t mgid = 0;
      bf_mc_mgrp_hdl_t mgrp_hdl; // will be returned from bf_mc_mgrp_create()
      bf_status_t bf_status = bf_mc_mgrp_create(bf_mc_session, dev_tgt.dev_id, mgid, &mgrp_hdl);
      assert(bf_status == BF_SUCCESS);

      // Create L1 node and assign it to the mc_group
      bf_mc_rid_t rid = 0xffff;
      bf_mc_port_map_t port_map;
      BF_MC_PORT_MAP_INIT(port_map);
      for (int i = 0; i < BFRT_NUM_CONN_COMPUTE_NODE; i++)
        BF_MC_PORT_MAP_SET(port_map, get_comp_port(i));  // Send invalidation to computing nodes
      bf_mc_lag_map_t lag_map;
      BF_MC_LAG_MAP_INIT(lag_map);
      bf_mc_node_hdl_t node_hdl; // Will be returned
      _mem_barrier_();
      bf_status = bf_mc_node_create(bf_mc_session, dev_tgt.dev_id, rid, port_map, lag_map, &node_hdl);
      assert(bf_status == BF_SUCCESS);

      //Assign node to the mc_group: level1_exclusion_id is not needed here = 0
      bf_status = bf_mc_associate_node(bf_mc_session, dev_tgt.dev_id, mgrp_hdl, node_hdl, false, 0);
      assert(bf_status == BF_SUCCESS);
    }

    /*******************************************************************************
     * Utility functions associated with "findCacheSlot"
     ******************************************************************************/
    void cacheline_key_setup(const CachelineKey &cacheline_key,
                             bfrt::BfRtTableKey *table_key)
    {
#ifndef __use_sram__
      // == TCAM based version == //
      auto bf_status =
          table_key->setValueLpm(cacheline_vaddr_field_id,
                                 static_cast<uint64_t>(cacheline_key.vaddr),
                                 static_cast<uint16_t>(cacheline_key.vaddr_prefix));
      pr_cache("Directory Reg: addr [0x%lx/%u]\n",
               (unsigned long)cacheline_key.vaddr, (unsigned int)cacheline_key.vaddr_prefix);
#else
      // == SRAM based version == //
      //
      uint64_t aligned_vaddr = (cacheline_key.vaddr & ~((1 << (64 - cacheline_key.vaddr_prefix)) - 1));
      auto bf_status =
          table_key->setValue(cacheline_vaddr_field_id, aligned_vaddr);
      pr_cache("Directory Reg: addr [0x%lx]\n", (unsigned long)aligned_vaddr);
#endif
      assert(bf_status == BF_SUCCESS);
      return;
    }

    void
    cacheline_data_setup(const CachelineData &cacheline_data,
                         bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(cacheline_action_cache_idx_field_id,
                               static_cast<uint64_t>(cacheline_data.cacheIdx));
      assert(bf_status == BF_SUCCESS);
    }

    void cacheline_entry_add_modify(const CachelineKey &cacheline_key,
                                    const CachelineData &cacheline_data,
                                    const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      cachelineTable->keyReset(bfrtCachelineTableKey.get());
      cachelineTable->dataReset(cacheline_action_id, bfrtCachelineTableData.get());

      // Fill in the Key and Data object
      cacheline_key_setup(cacheline_key, bfrtCachelineTableKey.get());
      cacheline_data_setup(cacheline_data, bfrtCachelineTableData.get());

      call_table_entry_add_modify(
          bfrtCachelineTableKey, bfrtCachelineTableData, cachelineTable, add);
    }

    bf_status_t cacheline_entry_get(const CachelineKey &cacheline_key, CachelineData *data)
    {
      bf_status_t status = BF_SUCCESS;

      // Reset key and data before use
      cachelineTable->keyReset(bfrtCachelineTableKey.get());
      cachelineTable->dataReset(bfrtCachelineTableData.get());

      cacheline_key_setup(cacheline_key, bfrtCachelineTableKey.get());
      status = call_table_entry_get_return_error(
          bfrtCachelineTableKey, bfrtCachelineTableData, cachelineTable);

      if (status == BF_SUCCESS)
      {
        uint64_t edata;
        status = bfrtCachelineTableData->getValue(cacheline_action_cache_idx_field_id, &edata);
        data->cacheIdx = static_cast<uint32_t>(edata);
      }
      return status;
    }

    void cacheline_entry_delete(const CachelineKey &cacheline_key)
    {
      // Reset key before use
      cachelineTable->keyReset(bfrtCachelineTableKey.get());
      cacheline_key_setup(cacheline_key, bfrtCachelineTableKey.get());

      std::shared_ptr<bfrt::BfRtSession> session = get_bfrt_session();
      auto status = cachelineTable->tableEntryDel(
          *session, get_dev_target(), *bfrtCachelineTableKey);
      assert(status == BF_SUCCESS);
      session->sessionCompleteOperations();
      return;
    }

    bf_status_t get_cacheline_rule(uint64_t vaddr, uint16_t vaddr_prefix, uint32_t &c_idx)
    {
      // Allocate initial data for servers
      CachelineKey c_key;
      CachelineData c_data;
      bf_status_t ret;

      c_key.vaddr = vaddr;
      c_key.vaddr_prefix = vaddr_prefix;
      c_data.cacheIdx = 0;
      ret = cacheline_entry_get(c_key, &c_data);
      c_idx = c_data.cacheIdx;
      return ret;
    }

    void add_cacheline_rule(uint64_t vaddr, uint16_t vaddr_prefix, uint32_t c_idx)
    {
      // Allocate initial data for servers
      CachelineKey c_key;
      CachelineData c_data;

      c_key.vaddr = vaddr;
      c_key.vaddr_prefix = vaddr_prefix;
      c_data.cacheIdx = c_idx;
      cacheline_entry_add_modify(c_key, c_data, true);
    }

    void del_cacheline_rule(uint64_t vaddr, uint16_t vaddr_prefix)
    {
      CachelineKey c_key;

      c_key.vaddr = vaddr;
      c_key.vaddr_prefix = vaddr_prefix;
      cacheline_entry_delete(c_key);
    }

    /*******************************************************************************
     * Utility functions associated with cachline register arrays
     ******************************************************************************/
    // 1) Key
    void cacheline_state_reg_key_setup(const CachelineRegKey &cacheline_key,
                                       bfrt::BfRtTableKey *table_key)
    {
      auto bf_status =
          table_key->setValue(cacheline_state_reg_key_id,
                                 static_cast<uint64_t>(cacheline_key.cacheIdx));
      assert(bf_status == BF_SUCCESS);
    }

    void cacheline_sharer_reg_key_setup(const CachelineRegKey &cacheline_key,
                                        bfrt::BfRtTableKey *table_key)
    {
      auto bf_status =
          table_key->setValue(cacheline_sharer_reg_key_id,
                              static_cast<uint64_t>(cacheline_key.cacheIdx));
      assert(bf_status == BF_SUCCESS);
    }

    void cacheline_dir_size_reg_key_setup(const CachelineRegKey &cacheline_key,
                                             bfrt::BfRtTableKey *table_key)
    {
      auto bf_status =
          table_key->setValue(cacheline_dir_size_reg_key_id,
                              static_cast<uint64_t>(cacheline_key.cacheIdx));
      assert(bf_status == BF_SUCCESS);
    }

    void cacheline_dir_lock_reg_key_setup(const CachelineRegKey &cacheline_key,
                                             bfrt::BfRtTableKey *table_key)
    {
      auto bf_status =
          table_key->setValue(cacheline_dir_lock_reg_key_id,
                              static_cast<uint64_t>(cacheline_key.cacheIdx));
      assert(bf_status == BF_SUCCESS);
    }

    void cacheline_inv_cnt_reg_key_setup(const CachelineRegKey &cacheline_key,
                                             bfrt::BfRtTableKey *table_key)
    {
      auto bf_status =
          table_key->setValue(cacheline_inv_cnt_reg_key_id,
                              static_cast<uint64_t>(cacheline_key.cacheIdx));
      assert(bf_status == BF_SUCCESS);
    }

    // 2) Data
    void
    cacheline_state_reg_data_setup(const CachelineRegData &cacheline_data,
                                   bfrt::BfRtTableData *table_data)
    {
      // Set the first value of the vector
      uint64_t state = 0, update = 0;
      if (cacheline_data.state.size() > 0)
      {
        state = cacheline_data.state[0];
      }
      if (cacheline_data.updataing.size() > 0)
      {
        update = cacheline_data.updataing[0];
      }

      auto bf_status = table_data->setValue(cacheline_reg_data_state_id, state);
      assert(bf_status == BF_SUCCESS);

      bf_status = table_data->setValue(cacheline_reg_data_update_id, update);
      assert(bf_status == BF_SUCCESS);
    }

    void
    cacheline_state_on_update_reg_data_setup(const CachelineRegData &cacheline_data,
                                             bfrt::BfRtTableData *table_data)
    {
      // Set the first value of the vector
      uint64_t update = 0;
      if (cacheline_data.updataing.size() > 0)
      {
        update = cacheline_data.updataing[0];
      }

      auto bf_status = table_data->setValue(cacheline_reg_data_update_id, update);
      assert(bf_status == BF_SUCCESS);
    }

    void
    cacheline_sharer_reg_data_setup(const CachelineRegData &cacheline_data,
                                    bfrt::BfRtTableData *table_data)
    {
      // Set the first value of the vector
      uint64_t sharer = 0;
      if (cacheline_data.sharer.size() > 0)
      {
        sharer = cacheline_data.sharer[0];
      }
      auto bf_status = table_data->setValue(cacheline_reg_data_sharer_id, sharer);
      assert(bf_status == BF_SUCCESS);
    }

    void
    cacheline_dir_size_reg_data_setup(const CachelineRegData &cacheline_data,
                                      bfrt::BfRtTableData *table_data)
    {
      uint64_t dir_size = 0;
      if (cacheline_data.dir_size.size() > 0)
      {
        dir_size = cacheline_data.dir_size[0];
      }
      auto bf_status = table_data->setValue(cacheline_dir_size_reg_data_id, dir_size);
      assert(bf_status == BF_SUCCESS);
    }

    void
    cacheline_dir_lock_reg_data_setup(const CachelineRegData &cacheline_data,
                                    bfrt::BfRtTableData *table_data)
    {
      uint64_t dir_lock = 0;
      if (cacheline_data.dir_lock.size() > 0)
      {
        dir_lock = cacheline_data.dir_lock[0];
      }
      auto bf_status = table_data->setValue(cacheline_dir_lock_reg_data_id, dir_lock);
      assert(bf_status == BF_SUCCESS);
    }

    void
    cacheline_inv_cnt_reg_data_setup(const CachelineRegData &cacheline_data,
                                     bfrt::BfRtTableData *table_data)
    {
      uint64_t inv_cnt = 0;
      if (cacheline_data.inv_cnt.size() > 0)
      {
        inv_cnt = cacheline_data.inv_cnt[0];
      }
      auto bf_status = table_data->setValue(cacheline_inv_cnt_reg_data_id, inv_cnt);
      assert(bf_status == BF_SUCCESS);
    }

    // 3) Get registers
    static void cacheline_state_reg_get(const CachelineRegKey &cacheline_key, CachelineRegData *data)
    {
      // Reset key and data before use
      cachelineStateRegTable->keyReset(bfrtCachelineStateRegKey.get());
      cachelineStateRegTable->dataReset(bfrtCachelineStateRegData.get());

      cacheline_state_reg_key_setup(cacheline_key, bfrtCachelineStateRegKey.get());
      call_table_entry_get(bfrtCachelineStateRegKey, bfrtCachelineStateRegData, cachelineStateRegTable);

      bf_status_t status = BF_SUCCESS;
      status = bfrtCachelineStateRegData->getValue(cacheline_reg_data_state_id, &data->state);
      assert(status == BF_SUCCESS);
      status = bfrtCachelineStateRegData->getValue(cacheline_reg_data_update_id, &data->updataing);
      assert(status == BF_SUCCESS);
    }

    static void cacheline_sharer_reg_get(const CachelineRegKey &cacheline_key, CachelineRegData *data)
    {
      // Reset key and data before use
      cachelineSharerRegTable->keyReset(bfrtCachelineSharerRegKey.get());
      cachelineSharerRegTable->dataReset(bfrtCachelineSharerRegData.get());

      cacheline_sharer_reg_key_setup(cacheline_key, bfrtCachelineSharerRegKey.get());
      call_table_entry_get(bfrtCachelineSharerRegKey, bfrtCachelineSharerRegData, cachelineSharerRegTable);
      bf_status_t status = BF_SUCCESS;
      status = bfrtCachelineSharerRegData->getValue(cacheline_reg_data_sharer_id, &data->sharer);
      assert(status == BF_SUCCESS);
    }

    static void cacheline_dir_size_reg_get(const CachelineRegKey &cacheline_key, CachelineRegData *data)
    {
      // Reset key and data before use
      cacheDirSizeRegTable->keyReset(bfrtCachelineDirSizeKey.get());
      cacheDirSizeRegTable->dataReset(bfrtCachelineDirSizeData.get());

      cacheline_dir_size_reg_key_setup(cacheline_key, bfrtCachelineDirSizeKey.get());
      call_table_entry_get(bfrtCachelineDirSizeKey, bfrtCachelineDirSizeData, cacheDirSizeRegTable);
      bf_status_t status = BF_SUCCESS;
      status = bfrtCachelineDirSizeData->getValue(cacheline_dir_size_reg_data_id, &data->dir_size);
      assert(status == BF_SUCCESS);
    }

    static void cacheline_dir_lock_reg_get(const CachelineRegKey &cacheline_key, CachelineRegData *data)
    {
      // Reset key and data before use
      cacheDirLockRegTable->keyReset(bfrtCachelineDirLockKey.get());
      cacheDirLockRegTable->dataReset(bfrtCachelineDirLockData.get());

      cacheline_dir_lock_reg_key_setup(cacheline_key, bfrtCachelineDirLockKey.get());
      call_table_entry_get(bfrtCachelineDirLockKey, bfrtCachelineDirLockData, cacheDirLockRegTable);
      bf_status_t status = BF_SUCCESS;
      status = bfrtCachelineDirLockData->getValue(cacheline_dir_lock_reg_data_id, &data->dir_lock);
      assert(status == BF_SUCCESS);
    }

    static void cacheline_inv_cnt_reg_get(const CachelineRegKey &cacheline_key, CachelineRegData *data)
    {
      // Reset key and data before use
      cacheInvCntRegTable->keyReset(bfrtCachelineInvCntKey.get());
      cacheInvCntRegTable->dataReset(bfrtCachelineInvCntData.get());

      cacheline_inv_cnt_reg_key_setup(cacheline_key, bfrtCachelineInvCntKey.get());
      call_table_entry_get(bfrtCachelineInvCntKey, bfrtCachelineInvCntData, cacheInvCntRegTable);
      bf_status_t status = BF_SUCCESS;
      status = bfrtCachelineInvCntData->getValue(cacheline_inv_cnt_reg_data_id, &data->inv_cnt);
      assert(status == BF_SUCCESS);
    }

    void cacheline_reg_get(const CachelineRegKey &cacheline_key, CachelineRegData *data)
    {
      cacheline_state_reg_get(cacheline_key, data);
      cacheline_sharer_reg_get(cacheline_key, data);
      cacheline_dir_size_reg_get(cacheline_key, data);
      cacheline_dir_lock_reg_get(cacheline_key, data);
      cacheline_inv_cnt_reg_get(cacheline_key, data);
    }

    void cacheline_reg_get_minimal(const CachelineRegKey &cacheline_key, CachelineRegData *data)
    {
      cacheline_state_reg_get(cacheline_key, data);
      cacheline_sharer_reg_get(cacheline_key, data);
      cacheline_dir_size_reg_get(cacheline_key, data);
    }

    // 4) add/set registers
    static void
    cacheline_state_reg_add(const CachelineRegKey &cacheline_key,
                            const CachelineRegData &cacheline_data,
                            const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      cachelineStateRegTable->keyReset(bfrtCachelineStateRegKey.get());
      cachelineStateRegTable->dataReset(bfrtCachelineStateRegData.get());

      // Fill in the Key and Data object
      cacheline_state_reg_key_setup(cacheline_key, bfrtCachelineStateRegKey.get());
      cacheline_state_reg_data_setup(cacheline_data, bfrtCachelineStateRegData.get());
      memory_barrier();
      call_table_entry_add_modify(
          bfrtCachelineStateRegKey, bfrtCachelineStateRegData, 
          cachelineStateRegTable, add);
    }

    static void
    cacheline_state_on_update_reg_reset(const CachelineRegKey &cacheline_key,
                            const CachelineRegData &cacheline_data)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      cachelineStateRegTable->keyReset(bfrtCachelineStateRegKey.get());
      cachelineStateRegTable->dataReset(bfrtCachelineStateRegData.get());

      // Fill in the Key and Data object
      cacheline_state_reg_key_setup(cacheline_key, bfrtCachelineStateRegKey.get());
      cacheline_state_on_update_reg_data_setup(cacheline_data, bfrtCachelineStateRegData.get());
      memory_barrier();
      call_table_entry_add_modify(
          bfrtCachelineStateRegKey, bfrtCachelineStateRegData, 
          cachelineStateRegTable, false);
    }

    static void
    cacheline_sharer_reg_add(const CachelineRegKey &cacheline_key,
                             const CachelineRegData &cacheline_data,
                             const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      cachelineSharerRegTable->keyReset(bfrtCachelineSharerRegKey.get());
      cachelineSharerRegTable->dataReset(bfrtCachelineSharerRegData.get());

      // Fill in the Key and Data object
      cacheline_sharer_reg_key_setup(cacheline_key, bfrtCachelineSharerRegKey.get());
      cacheline_sharer_reg_data_setup(cacheline_data, bfrtCachelineSharerRegData.get());
      memory_barrier();
      call_table_entry_add_modify(
          bfrtCachelineSharerRegKey, bfrtCachelineSharerRegData, 
          cachelineSharerRegTable, add);
    }

    static void
    cacheline_dir_size_reg_add(const CachelineRegKey &cacheline_key,
                               const CachelineRegData &cacheline_data,
                               const bool &add)
    {
      cacheDirSizeRegTable->keyReset(bfrtCachelineDirSizeKey.get());
      cacheDirSizeRegTable->dataReset(bfrtCachelineDirSizeData.get());
      cacheline_dir_size_reg_key_setup(cacheline_key, bfrtCachelineDirSizeKey.get());
      cacheline_dir_size_reg_data_setup(cacheline_data, bfrtCachelineDirSizeData.get());
      memory_barrier();
      call_table_entry_add_modify(bfrtCachelineDirSizeKey, bfrtCachelineDirSizeData,
                                  cacheDirSizeRegTable, add);
    }

    static void
    cacheline_dir_lock_reg_add(const CachelineRegKey &cacheline_key,
                               const CachelineRegData &cacheline_data,
                               const bool &add)
    {
      cacheDirLockRegTable->keyReset(bfrtCachelineDirLockKey.get());
      cacheDirLockRegTable->dataReset(bfrtCachelineDirLockData.get());
      cacheline_dir_lock_reg_key_setup(cacheline_key, bfrtCachelineDirLockKey.get());
      cacheline_dir_lock_reg_data_setup(cacheline_data, bfrtCachelineDirLockData.get());
      memory_barrier();
      call_table_entry_add_modify(bfrtCachelineDirLockKey, bfrtCachelineDirLockData,
                                  cacheDirLockRegTable, add);
    }

    static void
    cacheline_inv_cnt_reg_add(const CachelineRegKey &cacheline_key,
                              const CachelineRegData &cacheline_data,
                              const bool &add)
    {
      cacheInvCntRegTable->keyReset(bfrtCachelineInvCntKey.get());
      cacheInvCntRegTable->dataReset(bfrtCachelineInvCntData.get());
      cacheline_inv_cnt_reg_key_setup(cacheline_key, bfrtCachelineInvCntKey.get());
      cacheline_inv_cnt_reg_data_setup(cacheline_data, bfrtCachelineInvCntData.get());
      memory_barrier();
      call_table_entry_add_modify(bfrtCachelineInvCntKey, bfrtCachelineInvCntData,
                                  cacheInvCntRegTable, add);
    }

    void cacheline_reg_add(const CachelineRegKey &cacheline_key,
                           const CachelineRegData &cacheline_data,
                           const bool &add)
    {
      cacheline_state_reg_add(cacheline_key, cacheline_data, add);
      cacheline_sharer_reg_add(cacheline_key, cacheline_data, add);
      cacheline_dir_size_reg_add(cacheline_key, cacheline_data, add);
      cacheline_dir_lock_reg_add(cacheline_key, cacheline_data, add);
      cacheline_inv_cnt_reg_add(cacheline_key, cacheline_data, add);
    }

    void cacheline_reg_delete(const CachelineRegKey &cacheline_key)
    {
      // UNUSED
      (void)cacheline_key;
    }

    void get_cacheline_reg_rule(uint32_t cache_idx, uint16_t *state, uint16_t *st_update, uint16_t *sharer, 
                                uint16_t *dir_size, uint16_t *dir_lock, uint32_t *inv_cnt)
    {
      CachelineRegKey c_key;
      CachelineRegData c_data;

      c_key.cacheIdx = cache_idx;
      pthread_spin_lock(&cacheline_bfrt_lock);
      if (!dir_lock || !inv_cnt)
        cacheline_reg_get_minimal(c_key, &c_data);
      else
        cacheline_reg_get(c_key, &c_data);
      pthread_spin_unlock(&cacheline_bfrt_lock);

      // We only get data from the first and second pipelines
      if (c_data.state.size() > 0)
      {
        state[0] = c_data.state[0];
      }

      if (c_data.sharer.size() > 0)
      {
        sharer[0] = c_data.sharer[0];
      }

      if (c_data.dir_size.size() > 0)
      {
        dir_size[0] = c_data.dir_size[0];
      }

      if (c_data.dir_lock.size() > 0)
      {
        dir_lock[0] = c_data.dir_lock[0];
      }

      if (c_data.inv_cnt.size() > 0)
      {
        inv_cnt[0] = c_data.inv_cnt[0];
      }

      if (st_update && (c_data.updataing.size() > 0))
      {
        st_update[0] = c_data.updataing[0];
      }
#if 0
      for (auto &d : c_data.state)
      {
        printf("Register — state: 0x%lx\n", d);
      }
// #if 0
      for (auto &d : c_data.updataing)
      {
        printf("Register — update: 0x%lx\n", d);
      }

      for (auto &d : c_data.sharer)
      {
        printf("Register — sharer: 0x%lx\n", d);
      }
#endif
    }

    void get_cacheline_reg_state(uint32_t cache_idx, uint16_t *state, uint16_t *update)
    {
      CachelineRegKey c_key;
      CachelineRegData c_data;

      c_key.cacheIdx = cache_idx;
      pthread_spin_lock(&cacheline_bfrt_lock);
      cacheline_state_reg_get(c_key, &c_data);
      pthread_spin_unlock(&cacheline_bfrt_lock);

      // We only get data from the first and second pipelines
      if (c_data.state.size() > 0)
      {
        state[0] = c_data.state[0];
      }
      if (c_data.updataing.size() > 0)
      {
        update[0] = c_data.updataing[0];
      }
    }

    void get_cacheline_reg_sharer(uint32_t cache_idx, uint16_t *sharer)
    {
      CachelineRegKey c_key;
      CachelineRegData c_data;

      c_key.cacheIdx = cache_idx;
      pthread_spin_lock(&cacheline_bfrt_lock);
      cacheline_sharer_reg_get(c_key, &c_data);
      pthread_spin_unlock(&cacheline_bfrt_lock);

      if (c_data.sharer.size() > 0)
      {
        sharer[0] = c_data.sharer[0];
      }
    }

    void get_cacheline_reg_lock(uint32_t cache_idx, uint16_t *dir_lock)
    {
      CachelineRegKey c_key;
      CachelineRegData c_data;

      c_key.cacheIdx = cache_idx;
      pthread_spin_lock(&cacheline_bfrt_lock);
      cacheline_dir_lock_reg_get(c_key, &c_data);
      pthread_spin_unlock(&cacheline_bfrt_lock);
      if (c_data.dir_lock.size() > 0)
      {
        dir_lock[0] = (uint16_t)c_data.dir_lock[0];
      }
    }

    void get_cacheline_reg_inv(uint32_t cache_idx, uint32_t *inv_cnt)
    {
      CachelineRegKey c_key;
      CachelineRegData c_data;

      c_key.cacheIdx = cache_idx;
      pthread_spin_lock(&cacheline_bfrt_lock);
      cacheline_inv_cnt_reg_get(c_key, &c_data);
      pthread_spin_unlock(&cacheline_bfrt_lock);

      if (c_data.inv_cnt.size() > 0)
      {
        inv_cnt[0] = c_data.inv_cnt[0];
      }
    }

    void set_cacheline_reg_inv(uint32_t cache_idx, uint32_t inv_cnt)
    {
      // Allocate initial data for servers
      CachelineRegKey c_key;
      CachelineRegData c_data;
      std::vector<uint64_t> v_dir_inv;

      c_key.cacheIdx = cache_idx;
      v_dir_inv.push_back(inv_cnt);
      c_data.inv_cnt = v_dir_inv;
      // memory_barrier();
      pthread_spin_lock(&cacheline_bfrt_lock);
      cacheline_inv_cnt_reg_add(c_key, c_data, false);
      pthread_spin_unlock(&cacheline_bfrt_lock);
    }

    void set_cacheline_reg_lock(uint32_t cache_idx, uint16_t dir_lock)
    {
      // Allocate initial data for servers
      CachelineRegKey c_key;
      CachelineRegData c_data;
      std::vector<uint64_t> v_dir_lock;

      c_key.cacheIdx = cache_idx;
      v_dir_lock.push_back(dir_lock);
      c_data.dir_lock = v_dir_lock;

      pthread_spin_lock(&cacheline_bfrt_lock);
      cacheline_dir_lock_reg_add(c_key, c_data, false);
      pthread_spin_unlock(&cacheline_bfrt_lock);
    }

    void set_cacheline_reg_state(uint32_t cache_idx, uint16_t state)
    {
      // Allocate initial data for servers
      CachelineRegKey c_key;
      CachelineRegData c_data;
      std::vector<uint64_t> v_state;

      c_key.cacheIdx = cache_idx;
      v_state.push_back(state);
      c_data.state = v_state;

      pthread_spin_lock(&cacheline_bfrt_lock);
      cacheline_state_reg_add(c_key, c_data, false);
      pthread_spin_unlock(&cacheline_bfrt_lock);
    }

    void reset_cacheline_reg_state_on_update(uint32_t cache_idx)
    {
      // Allocate initial data for servers
      CachelineRegKey c_key;
      CachelineRegData c_data;
      std::vector<uint64_t> v_state;

      c_key.cacheIdx = cache_idx;
      pthread_spin_lock(&cacheline_bfrt_lock);
      cacheline_state_on_update_reg_reset(c_key, c_data);
      pthread_spin_unlock(&cacheline_bfrt_lock);
    }

    void add_cacheline_reg_rule(uint32_t cache_idx, uint16_t state, uint16_t sharer,
                                uint16_t dir_size, uint16_t dir_lock, uint32_t inv_cnt,
                                const bool &add)
    {
      // Allocate initial data for servers
      CachelineRegKey c_key;
      CachelineRegData c_data;
      std::vector<uint64_t> v_state, v_sharer, v_dir_size, v_dir_lock, v_inv_cnt;

      c_key.cacheIdx = cache_idx;
      v_state.push_back(state);
      v_sharer.push_back(sharer);
      v_dir_size.push_back(dir_size);
      v_dir_lock.push_back(dir_lock);
      v_inv_cnt.push_back(inv_cnt);
      c_data.state = v_state;
      c_data.sharer = v_sharer;
      c_data.dir_size = v_dir_size;
      c_data.dir_lock = v_dir_lock;
      c_data.inv_cnt = v_inv_cnt;
      memory_barrier();
      pthread_spin_lock(&cacheline_bfrt_lock);
      cacheline_reg_add(c_key, c_data, add);
      pthread_spin_unlock(&cacheline_bfrt_lock);
    }

    void del_cacheline_reg_rule(uint32_t cache_idx)
    {
      CachelineRegKey c_key;

      c_key.cacheIdx = cache_idx;
      cacheline_reg_delete(c_key);
    }
  } // namespace tna_disagg_switch
} // namespace bfrt

// Exposed APIs
// Cacheline/directory
static unsigned int cacheline_count = 0;
static unsigned int cacheline_max_count = 0;
unsigned int bfrt_get_cacheline_count(void)
{
  return cacheline_count;
}
extern "C" void bfrt_add_cacheline(uint64_t vaddr, uint16_t vaddr_prefix, uint32_t c_idx);
void bfrt_add_cacheline(uint64_t vaddr, uint16_t vaddr_prefix, uint32_t c_idx)
{
  bfrt::tna_disagg_switch::add_cacheline_rule(vaddr, vaddr_prefix, c_idx);

  pr_cache_v("Directory[%u] added: pid(%u)+vaddr[0x%lx/%u] -> cacheline [%u]\n",
             (unsigned int)cacheline_count,
             (unsigned int)(vaddr >> 48), vaddr & 0xFFFFFFFFFFFF,
             vaddr_prefix, (unsigned int)c_idx);
  cacheline_count++;
  if (cacheline_count > cacheline_max_count)
  {
    cacheline_max_count = cacheline_count;
  }
}

extern "C" int bfrt_get_cacheline(uint64_t vaddr, uint16_t vaddr_prefix, uint32_t *c_idx);
int bfrt_get_cacheline(uint64_t vaddr, uint16_t vaddr_prefix, uint32_t *c_idx)
{
  uint32_t _c_idx;
  bf_status_t ret = bfrt::tna_disagg_switch::get_cacheline_rule(vaddr, vaddr_prefix, _c_idx);
  (*c_idx) = _c_idx;
  return (ret == BF_SUCCESS);
}

extern "C" void bfrt_del_cacheline(uint64_t vaddr, uint16_t vaddr_prefix);
void bfrt_del_cacheline(uint64_t vaddr, uint16_t vaddr_prefix)
{
  bfrt::tna_disagg_switch::del_cacheline_rule(vaddr, vaddr_prefix);
  cacheline_count--;
}

extern "C" void bfrt_set_cacheline_inv(uint32_t cache_idx, uint32_t inv_cnt);
void bfrt_set_cacheline_inv(uint32_t cache_idx, uint32_t inv_cnt)
{
  bfrt::tna_disagg_switch::set_cacheline_reg_inv(cache_idx, inv_cnt);
  // pr_cache("Directory Inv Cnt[%u] set: cnt [%u]\n",
  //          (unsigned int)cache_idx, (unsigned int)inv_cnt);
}

extern "C" void bfrt_set_cacheline_lock(uint32_t cache_idx, uint16_t dir_lock);
void bfrt_set_cacheline_lock(uint32_t cache_idx, uint16_t dir_lock)
{
  bfrt::tna_disagg_switch::set_cacheline_reg_lock(cache_idx, dir_lock);
  // pr_cache("Directory Lock[%u] set: lock [%u]\n",
  //          (unsigned int)cache_idx, (unsigned int)dir_lock);
}

extern "C" void bfrt_set_cacheline_state(uint32_t cache_idx, uint16_t state);
void bfrt_set_cacheline_state(uint32_t cache_idx, uint16_t state)
{
  bfrt::tna_disagg_switch::set_cacheline_reg_state(cache_idx, state);
}
extern "C" void bfrt_reset_cacheline_state_on_update(uint32_t cache_idx);
void bfrt_reset_cacheline_state_on_update(uint32_t cache_idx)
{
  bfrt::tna_disagg_switch::reset_cacheline_reg_state_on_update(cache_idx);
}

extern "C" void bfrt_add_cacheline_reg(uint32_t cache_idx, uint16_t state, uint16_t sharer,
                                       uint16_t dir_size, uint16_t dir_lock, uint32_t inv_cnt);
void bfrt_add_cacheline_reg(uint32_t cache_idx, uint16_t state, uint16_t sharer,
                            uint16_t dir_size, uint16_t dir_lock, uint32_t inv_cnt)
{
  bfrt::tna_disagg_switch::add_cacheline_reg_rule(cache_idx, state, sharer,
                                                  dir_size, dir_lock, inv_cnt, true);
  pr_cache_v("Directory Reg[%u] added: state [0x%x] sharer [0x%x] size [0x%x] lock/cnt [%u/%u]\n",
             (unsigned int)cache_idx, state, sharer, dir_size,
             (unsigned int)dir_lock, (unsigned int)inv_cnt);
}

extern "C" void bfrt_mod_cacheline_reg(uint32_t cache_idx, uint16_t state, uint16_t sharer,
                                       uint16_t dir_size, uint16_t dir_lock, uint32_t inv_cnt);
void bfrt_mod_cacheline_reg(uint32_t cache_idx, uint16_t state, uint16_t sharer,
                            uint16_t dir_size, uint16_t dir_lock, uint32_t inv_cnt)
{
  bfrt::tna_disagg_switch::add_cacheline_reg_rule(cache_idx, state, sharer,
                                                  dir_size, dir_lock, inv_cnt, false);
  pr_cache_v("Directory Reg[%u] modified: state [0x%x] sharer [0x%x] size [0x%x] lock/cnt [%u/%u]\n",
             (unsigned int)cache_idx, state, sharer, dir_size,
             (unsigned int)dir_lock, (unsigned int)inv_cnt);
}

extern "C" void bfrt_get_cacheline_reg(uint32_t cache_idx, uint16_t *state, uint16_t *st_update, uint16_t *sharer,
                                       uint16_t *dir_size, uint16_t *dir_lock, uint32_t *inv_cnt);
void bfrt_get_cacheline_reg(uint32_t cache_idx, uint16_t *state, uint16_t *st_update, uint16_t *sharer,
                            uint16_t *dir_size, uint16_t *dir_lock, uint32_t *inv_cnt)
{
  bfrt::tna_disagg_switch::get_cacheline_reg_rule(cache_idx, state, st_update, sharer,
                                                  dir_size, dir_lock, inv_cnt);
}

extern "C" void bfrt_get_cacheline_reg_state(uint32_t cache_idx, uint16_t *state, uint16_t *update);
void bfrt_get_cacheline_reg_state(uint32_t cache_idx, uint16_t *state, uint16_t *update)
{
  bfrt::tna_disagg_switch::get_cacheline_reg_state(cache_idx, state, update);
}

extern "C" void bfrt_get_cacheline_reg_state_sharer(uint32_t cache_idx, uint16_t *state,
                                                    uint16_t *update, uint16_t *sharer);
void bfrt_get_cacheline_reg_state_sharer(uint32_t cache_idx, uint16_t *state,
                                         uint16_t *update, uint16_t *sharer)
{
  bfrt::tna_disagg_switch::get_cacheline_reg_state(cache_idx, state, update);
  bfrt::tna_disagg_switch::get_cacheline_reg_sharer(cache_idx, sharer);
}

extern "C" void bfrt_get_cacheline_reg_lock(uint32_t cache_idx, uint16_t *dir_lock);
void bfrt_get_cacheline_reg_lock(uint32_t cache_idx, uint16_t *dir_lock)
{
  bfrt::tna_disagg_switch::get_cacheline_reg_lock(cache_idx, dir_lock);
}

extern "C" void bfrt_get_cacheline_reg_inv(uint32_t cache_idx, uint32_t *inv_cnt);
void bfrt_get_cacheline_reg_inv(uint32_t cache_idx, uint32_t *inv_cnt)
{
  bfrt::tna_disagg_switch::get_cacheline_reg_inv(cache_idx, inv_cnt);
}
