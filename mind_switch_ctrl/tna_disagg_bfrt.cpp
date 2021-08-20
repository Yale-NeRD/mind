#include <bf_rt/bf_rt_info.hpp>
#include <bf_rt/bf_rt_init.hpp>
#include <bf_rt/bf_rt_common.h>
#include <bf_rt/bf_rt_table_key.hpp>
#include <bf_rt/bf_rt_table_data.hpp>
#include <bf_rt/bf_rt_table.hpp>

#ifdef __cplusplus
extern "C"
{
#endif
// multicast
#include <mc_mgr/mc_mgr_intf.h>
#ifdef __cplusplus
}
#endif

#include <getopt.h>
#include <unistd.h>
#include <atomic>
#include "bf_rt_utils.hpp"
#include "tna_disagg_bfrt.hpp"
#include "controller/config.h"
#include "controller/debug.h"
#include "controller/controller.h"

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

    namespace
    { // anonymous namespace
      // Key field ids, table data field ids, action ids, Table object required for
      // interacting with the table
      const bfrt::BfRtInfo *bfrtInfo = nullptr;
      const bfrt::BfRtTable *ipRouteTable = nullptr;
      const bfrt::BfRtTable *egressRouteTable = nullptr;
      const bfrt::BfRtTable *egressUpdateReqTable = nullptr;
      const bfrt::BfRtTable *egressInvRouteTable = nullptr;
      const bfrt::BfRtTable *egressAckTransTable = nullptr;

      const bfrt::BfRtTable *roceReqTable = nullptr;
      const bfrt::BfRtTable *roceAckTable = nullptr;
      const bfrt::BfRtTable *roceDummyAckTable = nullptr;
      const bfrt::BfRtTable *roceAckDestTable = nullptr;
      const bfrt::BfRtTable *setQpIdxTable = nullptr;
      const bfrt::BfRtTable *senderQpTable = nullptr;
      const bfrt::BfRtTable *addrTransTable = nullptr;
      const bfrt::BfRtTable *addrExceptTransTable = nullptr;

      std::shared_ptr<bfrt::BfRtSession> session;

      std::unique_ptr<bfrt::BfRtTableKey> bfrtTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtTableData;

      std::unique_ptr<bfrt::BfRtTableKey> bfrtEgressRouteTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtEgressRouteTableData;

      std::unique_ptr<bfrt::BfRtTableKey> bfrtEgressUpdateReqTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtEgressUpdateReqTableData;

      std::unique_ptr<bfrt::BfRtTableKey> bfrtEgressInvRouteTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtEgressInvRouteTableData;

      std::unique_ptr<bfrt::BfRtTableKey> bfrtEgressAckTransTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtEgressAckTransTableData;

      std::unique_ptr<bfrt::BfRtTableKey> bfrtRoceReqTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtRoceReqTableData;

      std::unique_ptr<bfrt::BfRtTableKey> bfrtRoceAckTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtRoceAckTableData;

      std::unique_ptr<bfrt::BfRtTableKey> bfrtRoceDummyAckTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtRoceDummyAckTableData;

      std::unique_ptr<bfrt::BfRtTableKey> bfrtRoceAckDestTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtRoceAckDestTableData;

      std::unique_ptr<bfrt::BfRtTableKey> bfrtSetQpIdxTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtSetQpIdxTableData;

      std::unique_ptr<bfrt::BfRtTableKey> bfrtSenderQpTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtSenderQpTableData;

      std::unique_ptr<bfrt::BfRtTableKey> bfrtAddrTransTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtAddrTransTableData;

      std::unique_ptr<bfrt::BfRtTableKey> bfrtAddrExceptTransTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtAddrExceptTransTableData;

      std::unique_ptr<bfrt::BfRtTableKey> bfrtAddrMirrorCfgTableKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtAddrMirrorCfgTableData;

      // Key field ids
      bf_rt_id_t ipRoute_ip_dst_field_id = 0;
      bf_rt_id_t egressRoute_port_field_id = 0;
      bf_rt_id_t egressUpdateReq_dst_ip_field_id = 0;
      bf_rt_id_t egressInvRoute_port_field_id = 0;
      bf_rt_id_t egressInvRoute_inv_id_id = 0;
      bf_rt_id_t egressAckTrans_ip_dst_field_id = 0;
      bf_rt_id_t egressAckTrans_dst_qp_field_id = 0;
      bf_rt_id_t roce_req_ip_src_field_id = 0;
      bf_rt_id_t roce_req_ip_dst_field_id = 0;
      bf_rt_id_t roce_req_qp_field_id = 0;
      bf_rt_id_t roce_ack_qp_field_id = 0;
      bf_rt_id_t roce_dummy_ack_ip_src_field_id = 0;
      bf_rt_id_t roce_dummy_ack_qp_field_id = 0;
      bf_rt_id_t roce_ack_dest_ip_src_field_id = 0;
      bf_rt_id_t roce_ack_dest_qp_field_id = 0;
      bf_rt_id_t set_qp_idx_qp_field_id = 0;
      bf_rt_id_t set_qp_idx_ip_src_field_id = 0;
      bf_rt_id_t sender_qp_cpu_qp_id_field_id = 0;
      bf_rt_id_t sender_qp_ip_src_field_id = 0;
      bf_rt_id_t addr_trans_vaddr_field_id = 0;
      bf_rt_id_t addr_except_trans_vaddr_field_id = 0;

      // Action Ids
      bf_rt_id_t ipRoute_route_action_id = 0;
      bf_rt_id_t egressRoute_action_id = 0;
      bf_rt_id_t egressUpdateReq_action_id = 0;
      bf_rt_id_t egressInvRoute_action_id = 0;
      bf_rt_id_t egressAckTrans_action_id = 0;
      bf_rt_id_t roce_req_action_id = 0;
      bf_rt_id_t roce_ack_action_id = 0;
      bf_rt_id_t roce_dummy_ack_action_id = 0;
      bf_rt_id_t roce_ack_dest_action_id = 0;
      bf_rt_id_t set_qp_idx_action_id = 0;
      bf_rt_id_t sender_qp_action_id = 0;
      bf_rt_id_t addr_trans_action_id = 0;
      bf_rt_id_t addr_except_trans_action_id = 0;

      // Data field Ids for route action
      bf_rt_id_t ipRoute_route_action_dst_mac_field_id = 0;
      bf_rt_id_t ipRoute_route_action_port_field_id = 0;
      bf_rt_id_t egressRoute_action_dst_mac_field_id = 0;
      bf_rt_id_t egressRoute_action_dst_ip_field_id = 0;
      bf_rt_id_t egressUpdateReq_action_src_ip_field_id = 0;
      bf_rt_id_t egressInvRoute_action_dst_mac_field_id = 0;
      bf_rt_id_t egressInvRoute_action_dst_ip_field_id = 0;
      bf_rt_id_t egressInvRoute_action_dst_qp_field_id = 0;
      bf_rt_id_t egressInvRoute_action_dst_rkey_field_id = 0;
      bf_rt_id_t egressInvRoute_action_inv_vaddr_field_id = 0;
      bf_rt_id_t egressInvRoute_action_reg_field_id = 0;
      bf_rt_id_t egressAckTrans_action_dst_qp_field_id = 0;
      bf_rt_id_t egressAckTrans_action_dst_rkey_field_id = 0;
      bf_rt_id_t egressAckTrans_action_vaddr_field_id = 0;
      bf_rt_id_t egressAckTrans_action_reg_field_id = 0;

      // Data field for RoCE action
      // Req
      bf_rt_id_t roce_req_action_dst_qp_field_id = 0;
      bf_rt_id_t roce_req_action_dst_rkey_field_id = 0;
      bf_rt_id_t roce_req_action_src_ip_field_id = 0;
      bf_rt_id_t roce_req_action_src_mac_field_id = 0;
      bf_rt_id_t roce_req_action_reg_idx_field_id = 0;
      // Ack
      bf_rt_id_t roce_ack_action_dst_qp_field_id = 0;
      bf_rt_id_t roce_ack_action_dst_ip_field_id = 0;
      bf_rt_id_t roce_ack_action_src_ip_field_id = 0;
      bf_rt_id_t roce_ack_action_reg_idx_field_id = 0;
      // Dummy Ack
      bf_rt_id_t roce_dummy_ack_action_dst_qp_field_id = 0;
      bf_rt_id_t roce_dummy_ack_action_vaddr_field_id = 0;
      // Destination for Acks (set dest for dummy acks)
      bf_rt_id_t roce_ack_dest_action_dst_qp_field_id = 0;
      bf_rt_id_t roce_ack_dest_action_dummy_qp_field_id = 0;
      bf_rt_id_t set_qp_idx_action_qp_idx_field_id = 0;
      // Sender QP id
      bf_rt_id_t sender_qp_action_qp_idx_field_id = 0;

      // Data field for address translation
      // Partition
      bf_rt_id_t addr_trans_action_dst_ip_field_id = 0;
      bf_rt_id_t addr_trans_action_va_to_dma_field_id = 0;
      // Exceptions
      bf_rt_id_t addr_except_trans_action_dst_ip_field_id = 0;
      bf_rt_id_t addr_except_trans_action_va_to_dma_field_id = 0;
      bf_rt_id_t addr_except_trans_action_permission_field_id = 0;

      // static table for match soley owned directory
      const bfrt::BfRtTable *cacheOwnerCheckTable = nullptr;
      std::unique_ptr<bfrt::BfRtTableKey> bfrtCacheOwnerCheckKey;
      std::unique_ptr<bfrt::BfRtTableData> bfrtCacheOwnerCheckData;
      bf_rt_id_t cache_owner_check_mask_field_id = 0;
      bf_rt_id_t cache_owner_check_action_id = 0;

#define ALL_PIPES 0xffff
      bf_rt_target_t dev_tgt;
    } // anonymous namespace

    // statistic of rules
    static std::atomic<unsigned long> num_addr_except_trans_rules;
    static std::atomic<unsigned long> num_except_trans_pages;
    static std::atomic<unsigned long> num_except_trans_huge_pages;
    static std::atomic<unsigned long> num_addr_trans_rules;

    std::atomic<unsigned long> *get_num_except_trans_rules(void)
    {
      return &num_addr_except_trans_rules;
    }

    std::atomic<unsigned long> *get_num_except_trans_pages(void)
    {
      return &num_except_trans_pages;
    }

    std::atomic<unsigned long> *get_num_except_trans_huge_pages(void)
    {
      return &num_except_trans_huge_pages;
    }

    std::atomic<unsigned long> *get_num_addr_trans_rules(void)
    {
      return &num_addr_trans_rules;
    }

    bf_rt_target_t get_dev_target(void)
    {
      return dev_tgt;
    }

    std::shared_ptr<bfrt::BfRtSession> get_bfrt_session(void)
    {
      return session;
    }

    // This function does the initial setUp of getting bfrtInfo object associated
    // with the P4 program from which all other required objects are obtained
    void setUp()
    {
      dev_tgt.dev_id = 0;
      dev_tgt.pipe_id = ALL_PIPES;
      // Get devMgr singleton instance
      auto &devMgr = bfrt::BfRtDevMgr::getInstance();
      std::vector<std::reference_wrapper<const std::string>> p4_names;
      auto p4_status = devMgr.bfRtInfoP4NamesGet(dev_tgt.dev_id, p4_names);
      int p4_size = static_cast<int>(p4_names.size());
      printf("== total P4 programs: %d ==\n", p4_size);
      for (int i = 0; i < p4_size; i++)
      {
        printf("[%d]: %s\n", i, p4_names.back().get().c_str());
        p4_names.pop_back();
      }

      // Get bfrtInfo object from dev_id and p4 program name
      auto bf_status =
          devMgr.bfRtInfoGet(dev_tgt.dev_id, "tna_disagg_switch", &bfrtInfo);
      // Check for status
      printf("Dev: %d, bf_status: %d, p4_status: %d\n", (int)dev_tgt.dev_id, bf_status, p4_status);
      assert(bf_status == BF_SUCCESS);

      // Create a session object
      session = bfrt::BfRtSession::sessionCreate();

      // initial mirroring and multicasting setup
      setUp_cache_unmatched_mirror();
    }

    // This function does the initial set up of getting key field-ids, action-ids
    // and data field ids associated with the ipRoute table. This is done once
    // during init time.
    void get_table_helper(std::string table_name, const bfrt::BfRtTable **bfrtTable)
    {
      auto bf_status = bfrtInfo->bfrtTableFromNameGet(table_name, bfrtTable);
      assert(bf_status == BF_SUCCESS);
    }

    void get_act_id_helper(const bfrt::BfRtTable *bfrtTable, std::string act_name, bf_rt_id_t *act_id)
    {
      auto bf_status = bfrtTable->actionIdGet(act_name, act_id);
      assert(bf_status == BF_SUCCESS);
    }

    void get_key_id_helper(const bfrt::BfRtTable *bfrtTable, std::string key_name, bf_rt_id_t *key_id)
    {
      auto bf_status = bfrtTable->keyFieldIdGet(key_name, key_id);
      assert(bf_status == BF_SUCCESS);
    }

    void get_data_id_helper(const bfrt::BfRtTable *bfrtTable, std::string data_name,
                            bf_rt_id_t action_id, bf_rt_id_t *data_id)
    {
      auto bf_status = bfrtTable->dataFieldIdGet(data_name, action_id, data_id);
      assert(bf_status == BF_SUCCESS);
    }

    void get_data_id_helper(const bfrt::BfRtTable *bfrtTable, std::string data_name,
                            bf_rt_id_t *data_id)
    {
      auto bf_status = bfrtTable->dataFieldIdGet(data_name, data_id);
      assert(bf_status == BF_SUCCESS);
    }

    void get_key_data_obj_helper(const bfrt::BfRtTable *bfrtTable,
                                 std::unique_ptr<bfrt::BfRtTableKey> *table_key,
                                 std::unique_ptr<bfrt::BfRtTableData> *table_data)
    {
      auto bf_status = bfrtTable->keyAllocate(table_key);
      assert(bf_status == BF_SUCCESS);

      bf_status = bfrtTable->dataAllocate(table_data);
      assert(bf_status == BF_SUCCESS);
    }

    void tableSetUp()
    {
      // 0) Initialize statistics
      num_addr_except_trans_rules = 0;
      num_addr_trans_rules = 0;

      /***********************************************************************/
      // 1) Get table object from name
      get_table_helper("SwitchIngress.ipRoute", &ipRouteTable);
      get_table_helper("SwitchEgress.eroute.ipRoute", &egressRouteTable);
      get_table_helper("SwitchEgress.eroute.reqSrcRoute", &egressUpdateReqTable);
      get_table_helper("SwitchEgress.translateInvToRoce", &egressInvRouteTable);
      get_table_helper("SwitchEgress.translateAckToRoce", &egressAckTransTable);
      get_table_helper("SwitchIngress.roceReq", &roceReqTable);
      get_table_helper("SwitchIngress.roceAck", &roceAckTable);
      get_table_helper("SwitchIngress.roceDummyAck", &roceDummyAckTable);
      get_table_helper("SwitchIngress.roceAckDest", &roceAckDestTable);
      get_table_helper("SwitchIngress.roceSetQpIdx", &setQpIdxTable);
      get_table_helper("SwitchIngress.getSenderQp", &senderQpTable);
      get_table_helper("SwitchIngress.isLastAckForModData", &cacheOwnerCheckTable);
      get_table_helper("SwitchIngress.addrTrans", &addrTransTable);
      get_table_helper("SwitchIngress.addrExceptTrans", &addrExceptTransTable);

      /***********************************************************************/
      // 2) Get action Ids for route and nat actions
      // 2.a) routing table
      get_act_id_helper(ipRouteTable, "SwitchIngress.route", &ipRoute_route_action_id);
      get_act_id_helper(egressRouteTable, "SwitchEgress.eroute.route", &egressRoute_action_id);
      get_act_id_helper(egressUpdateReqTable, "SwitchEgress.eroute.updateReqSrc", &egressUpdateReq_action_id);
      get_act_id_helper(egressInvRouteTable, "SwitchEgress.invalidationIpRoute", &egressInvRoute_action_id);
      get_act_id_helper(egressAckTransTable, "SwitchEgress.make_ack_as_roce", &egressAckTrans_action_id);
      // 2.b) roce table
      get_act_id_helper(roceReqTable, "SwitchIngress.roce_req_meta", &roce_req_action_id);
      get_act_id_helper(roceAckTable, "SwitchIngress.roce_ack_meta", &roce_ack_action_id);
      get_act_id_helper(roceDummyAckTable, "SwitchIngress.roce_dummy_ack", &roce_dummy_ack_action_id);
      get_act_id_helper(roceAckDestTable, "SwitchIngress.roce_ack_dest_meta", &roce_ack_dest_action_id);
      get_act_id_helper(setQpIdxTable, "SwitchIngress.set_qp_idx", &set_qp_idx_action_id);
      get_act_id_helper(senderQpTable, "SwitchIngress.get_sender_qp", &sender_qp_action_id);
      get_act_id_helper(cacheOwnerCheckTable, "SwitchIngress.mark_last_ack", &cache_owner_check_action_id);

      // 2.c) address translation & access control
      get_act_id_helper(addrTransTable, "SwitchIngress.trans_addr", &addr_trans_action_id);
      get_act_id_helper(addrExceptTransTable, "SwitchIngress.trans_except_addr", &addr_except_trans_action_id);
      /***********************************************************************/
      // 3) Get field-ids for key field
      // IP route assignment (in/egress)
      get_key_id_helper(ipRouteTable, "hdr.ipv4.dst_addr", &ipRoute_ip_dst_field_id);
      get_key_id_helper(egressRouteTable, "eg_intr_md.egress_port", &egressRoute_port_field_id);
      // get_key_id_helper(egressUpdateReqTable, "eg_intr_md.egress_port", &egressUpdateReq_port_field_id);
      get_key_id_helper(egressUpdateReqTable, "hdr.ipv4.dst_addr", &egressUpdateReq_dst_ip_field_id);
      get_key_id_helper(egressInvRouteTable, "eg_intr_md.egress_port", &egressInvRoute_port_field_id);
      get_key_id_helper(egressInvRouteTable, "eg_md.node_id", &egressInvRoute_inv_id_id);
      // egress ack translation
      get_key_id_helper(egressAckTransTable, "hdr.ipv4.dst_addr", &egressAckTrans_ip_dst_field_id);
      get_key_id_helper(egressAckTransTable, "hdr.roce.qp", &egressAckTrans_dst_qp_field_id);
      // RoCE multiplexing
      get_key_id_helper(roceReqTable, "hdr.ipv4.src_addr", &roce_req_ip_src_field_id);
      get_key_id_helper(roceReqTable, "hdr.ipv4.dst_addr", &roce_req_ip_dst_field_id);
      get_key_id_helper(roceReqTable, "hdr.roce.qp", &roce_req_qp_field_id);
      get_key_id_helper(roceAckTable, "hdr.roce.qp", &roce_ack_qp_field_id);
      get_key_id_helper(roceDummyAckTable, "hdr.ipv4.src_addr", &roce_dummy_ack_ip_src_field_id);
      get_key_id_helper(roceDummyAckTable, "hdr.roce.qp", &roce_dummy_ack_qp_field_id);
      get_key_id_helper(roceAckDestTable, "hdr.ipv4.src_addr", &roce_ack_dest_ip_src_field_id);
      get_key_id_helper(roceAckDestTable, "hdr.roce.qp", &roce_ack_dest_qp_field_id);
      get_key_id_helper(setQpIdxTable, "hdr.ipv4.src_addr", &set_qp_idx_ip_src_field_id);
      get_key_id_helper(setQpIdxTable, "hdr.roce.qp", &set_qp_idx_qp_field_id);
      // Roce req to ack conversion
      get_key_id_helper(senderQpTable, "hdr.ipv4.src_addr", &sender_qp_ip_src_field_id);
      get_key_id_helper(senderQpTable, "hdr.roce.qp", &sender_qp_cpu_qp_id_field_id);
      // Check there is only one owner
      get_key_id_helper(cacheOwnerCheckTable, "hdr.recirc_data.next_sharer", &cache_owner_check_mask_field_id);
      get_key_id_helper(addrTransTable, "ig_md.vaddr", &addr_trans_vaddr_field_id);
      get_key_id_helper(addrExceptTransTable, "hdr.roce_r.vaddr", &addr_except_trans_vaddr_field_id);
      /***********************************************************************/
      // 4) Get data field for actions
      // DATA FIELD ID GET FOR "route" ACTION
      get_data_id_helper(ipRouteTable, "dst_port", ipRoute_route_action_id, &ipRoute_route_action_port_field_id);
      get_data_id_helper(ipRouteTable, "dst_mac", ipRoute_route_action_id, &ipRoute_route_action_dst_mac_field_id);
      get_data_id_helper(egressRouteTable, "dest_mac", egressRoute_action_id, &egressRoute_action_dst_mac_field_id);
      get_data_id_helper(egressRouteTable, "dest_ip", egressRoute_action_id, &egressRoute_action_dst_ip_field_id);
      get_data_id_helper(egressUpdateReqTable, "src_ip", egressUpdateReq_action_id, &egressUpdateReq_action_src_ip_field_id);
      //
      get_data_id_helper(egressInvRouteTable, "dest_mac", egressInvRoute_action_id, &egressInvRoute_action_dst_mac_field_id);
      get_data_id_helper(egressInvRouteTable, "dest_ip", egressInvRoute_action_id, &egressInvRoute_action_dst_ip_field_id);
      get_data_id_helper(egressInvRouteTable, "dest_qp", egressInvRoute_action_id, &egressInvRoute_action_dst_qp_field_id);
      get_data_id_helper(egressInvRouteTable, "dest_rkey", egressInvRoute_action_id, &egressInvRoute_action_dst_rkey_field_id);
      get_data_id_helper(egressInvRouteTable, "inv_vaddr", egressInvRoute_action_id, &egressInvRoute_action_inv_vaddr_field_id);
      get_data_id_helper(egressInvRouteTable, "reg_idx", egressInvRoute_action_id, &egressInvRoute_action_reg_field_id);
      //
      get_data_id_helper(egressAckTransTable, "dest_qp", egressAckTrans_action_id, &egressAckTrans_action_dst_qp_field_id);
      get_data_id_helper(egressAckTransTable, "dest_rkey", egressAckTrans_action_id, &egressAckTrans_action_dst_rkey_field_id);
      get_data_id_helper(egressAckTransTable, "ack_vaddr", egressAckTrans_action_id, &egressAckTrans_action_vaddr_field_id);
      get_data_id_helper(egressAckTransTable, "reg_idx", egressAckTrans_action_id, &egressAckTrans_action_reg_field_id);
      // DATA FIELD ID GET FOR "roce_req_meta" ACTION
      get_data_id_helper(roceReqTable, "dest_qp", roce_req_action_id, &roce_req_action_dst_qp_field_id);
      get_data_id_helper(roceReqTable, "dest_rkey", roce_req_action_id, &roce_req_action_dst_rkey_field_id);
      get_data_id_helper(roceReqTable, "src_ip", roce_req_action_id, &roce_req_action_src_ip_field_id);
      get_data_id_helper(roceReqTable, "src_mac", roce_req_action_id, &roce_req_action_src_mac_field_id);
      get_data_id_helper(roceReqTable, "reg_idx", roce_req_action_id, &roce_req_action_reg_idx_field_id);
      // DATA FIELD ID GET FOR "roce_ack_meta" ACTION
      get_data_id_helper(roceAckTable, "dest_qp", roce_ack_action_id, &roce_ack_action_dst_qp_field_id);
      get_data_id_helper(roceAckTable, "dest_ip", roce_ack_action_id, &roce_ack_action_dst_ip_field_id);
      get_data_id_helper(roceAckTable, "src_ip", roce_ack_action_id, &roce_ack_action_src_ip_field_id);
      get_data_id_helper(roceAckTable, "reg_idx", roce_ack_action_id, &roce_ack_action_reg_idx_field_id);
      //
      get_data_id_helper(roceDummyAckTable, "dest_qp", roce_dummy_ack_action_id, &roce_dummy_ack_action_dst_qp_field_id);
      get_data_id_helper(roceDummyAckTable, "dummy_va", roce_dummy_ack_action_id, &roce_dummy_ack_action_vaddr_field_id);
      //
      get_data_id_helper(roceAckDestTable, "dest_qp", roce_ack_dest_action_id, &roce_ack_dest_action_dst_qp_field_id);
      get_data_id_helper(roceAckDestTable, "dummy_qp_id", roce_ack_dest_action_id, &roce_ack_dest_action_dummy_qp_field_id);
      //
      get_data_id_helper(setQpIdxTable, "qp_idx", set_qp_idx_action_id, &set_qp_idx_action_qp_idx_field_id);
      //
      get_data_id_helper(senderQpTable, "qp_idx", sender_qp_action_id, &sender_qp_action_qp_idx_field_id);
      // DATA FIELD ID GET FOR "trans_addr" ACTION (address translation)
      get_data_id_helper(addrTransTable, "dest_ip", addr_trans_action_id, &addr_trans_action_dst_ip_field_id);
      get_data_id_helper(addrTransTable, "va_base_to_dma", addr_trans_action_id, &addr_trans_action_va_to_dma_field_id);
      // DATA FIELD ID GET FOR "trans_except_addr" ACTION (exceptional address translation)
      get_data_id_helper(addrExceptTransTable, "dest_ip",
                         addr_except_trans_action_id, &addr_except_trans_action_dst_ip_field_id);
      get_data_id_helper(addrExceptTransTable, "va_base_to_dma",
                         addr_except_trans_action_id, &addr_except_trans_action_va_to_dma_field_id);
      get_data_id_helper(addrExceptTransTable, "permission",
                         addr_except_trans_action_id, &addr_except_trans_action_permission_field_id);
      /***********************************************************************/
      // 5) Allocate key and data once, and use reset across different uses
      get_key_data_obj_helper(ipRouteTable, &bfrtTableKey, &bfrtTableData);
      get_key_data_obj_helper(egressRouteTable, &bfrtEgressRouteTableKey, &bfrtEgressRouteTableData);
      get_key_data_obj_helper(egressUpdateReqTable, &bfrtEgressUpdateReqTableKey, &bfrtEgressUpdateReqTableData);
      get_key_data_obj_helper(egressInvRouteTable, &bfrtEgressInvRouteTableKey, &bfrtEgressInvRouteTableData);
      get_key_data_obj_helper(egressAckTransTable, &bfrtEgressAckTransTableKey, &bfrtEgressAckTransTableData);
      // RoCE req / ack / req to ack conversion
      get_key_data_obj_helper(roceReqTable, &bfrtRoceReqTableKey, &bfrtRoceReqTableData);
      get_key_data_obj_helper(roceAckTable, &bfrtRoceAckTableKey, &bfrtRoceAckTableData);
      get_key_data_obj_helper(roceDummyAckTable, &bfrtRoceDummyAckTableKey, &bfrtRoceDummyAckTableData);
      get_key_data_obj_helper(roceAckDestTable, &bfrtRoceAckDestTableKey, &bfrtRoceAckDestTableData);
      get_key_data_obj_helper(setQpIdxTable, &bfrtSetQpIdxTableKey, &bfrtSetQpIdxTableData);
      get_key_data_obj_helper(senderQpTable, &bfrtSenderQpTableKey, &bfrtSenderQpTableData);
      //
      get_key_data_obj_helper(cacheOwnerCheckTable, &bfrtCacheOwnerCheckKey, &bfrtCacheOwnerCheckData);
      // Address translation
      get_key_data_obj_helper(addrTransTable, &bfrtAddrTransTableKey, &bfrtAddrTransTableData);
      get_key_data_obj_helper(addrExceptTransTable, &bfrtAddrExceptTransTableKey, &bfrtAddrExceptTransTableData);

      tableSetUp_cacheline(bfrtInfo);
      tableSetUp_cache_state(bfrtInfo);

      // File for log
      set_datetime_filename();
      open_datetime_file();
    }

    /*******************************************************************************
     * Helper functions for table management
     ******************************************************************************/

    void call_table_entry_add_modify(
        std::unique_ptr<bfrt::BfRtTableKey> &_bfrtTableKey,
        std::unique_ptr<bfrt::BfRtTableData> &_bfrtTableData,
        const bfrt::BfRtTable *tar_table,
        const bool &add)
    {
      // Call table entry add API, if the request is for an add, else call modify
      bf_status_t status = BF_SUCCESS;
      session->sessionCompleteOperations();
      if (add)
      {
        status = tar_table->tableEntryAdd(
            *session, dev_tgt, *_bfrtTableKey, *_bfrtTableData);
      }
      else
      {
        status = tar_table->tableEntryMod(
            *session, dev_tgt, *_bfrtTableKey, *_bfrtTableData);
      }
      assert(status == BF_SUCCESS);
      session->sessionCompleteOperations();
    }

    bf_status_t call_table_entry_get_return_error(
        std::unique_ptr<bfrt::BfRtTableKey> &_bfrtTableKey,
        std::unique_ptr<bfrt::BfRtTableData> &_bfrtTableData,
        const bfrt::BfRtTable *tar_table)
    {
      bf_status_t status = BF_SUCCESS;
      // Entry get from hardware with the flag set to read from hardware
      auto flag = bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_HW;
      status = tar_table->tableEntryGet(
          *session, dev_tgt, *_bfrtTableKey, flag, _bfrtTableData.get());
      session->sessionCompleteOperations();
      return status;
    }

    void call_table_entry_get(
        std::unique_ptr<bfrt::BfRtTableKey> &_bfrtTableKey,
        std::unique_ptr<bfrt::BfRtTableData> &_bfrtTableData,
        const bfrt::BfRtTable *tar_table)
    {
      bf_status_t status = BF_SUCCESS;
      status = call_table_entry_get_return_error(_bfrtTableKey, _bfrtTableData, tar_table);
      assert(status == BF_SUCCESS);
    }

    /*******************************************************************************
     * Utility functions associated with "ipRoute" table in the P4 program.
     ******************************************************************************/

    // This function sets the passed in ip_dst and vrf value into the key object
    // passed using the setValue methods on the key object
    void ipRoute_key_setup(const IpRouteKey &ipRoute_key,
                           bfrt::BfRtTableKey *table_key)
    {
      // Set value into the key object. Key type is "EXACT"
      auto bf_status = table_key->setValue(
          ipRoute_ip_dst_field_id, static_cast<uint64_t>(ipRoute_key.ipDstAddr));
      assert(bf_status == BF_SUCCESS);

      return;
    }

    // This function sets the passed in "route" action data  into the
    // data object associated with the ipRoute table
    void ipRoute_data_setup_for_route(const IpRoute_routeData &ipRoute_data,
                                      bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(ipRoute_route_action_port_field_id,
                               static_cast<uint64_t>(ipRoute_data.dst_port));
      assert(bf_status == BF_SUCCESS);

      bf_status =
          table_data->setValue(ipRoute_route_action_dst_mac_field_id,
                               static_cast<uint64_t>(ipRoute_data.dst_mac));
      assert(bf_status == BF_SUCCESS);

      return;
    }

    // This function adds or modifies an entry in the ipRoute table with "route"
    // action. The workflow is similar for either table entry add or modify
    void ipRoute_entry_add_modify_with_route(const IpRouteKey &ipRoute_key,
                                             const IpRoute_routeData &ipRoute_data,
                                             const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      ipRouteTable->keyReset(bfrtTableKey.get());
      ipRouteTable->dataReset(ipRoute_route_action_id, bfrtTableData.get());

      // Fill in the Key and Data object
      ipRoute_key_setup(ipRoute_key, bfrtTableKey.get());
      ipRoute_data_setup_for_route(ipRoute_data, bfrtTableData.get());

      // Call table entry add API, if the request is for an add, else call modify
      bf_status_t status = BF_SUCCESS;
      if (add)
      {
        status = ipRouteTable->tableEntryAdd(
            *session, dev_tgt, *bfrtTableKey, *bfrtTableData);
      }
      else
      {
        status = ipRouteTable->tableEntryMod(
            *session, dev_tgt, *bfrtTableKey, *bfrtTableData);
      }
      assert(status == BF_SUCCESS);
      session->sessionCompleteOperations();
    }

    // This function process the entry obtained by a get call for a "route" action
    // and populates the IpRoute_routeData structure
    void ipRoute_process_route_entry_get(const bfrt::BfRtTableData &data,
                                         IpRoute_routeData *route_data)
    {
      bf_status_t status = BF_SUCCESS;
      uint64_t port;
      status = data.getValue(ipRoute_route_action_port_field_id, &port);
      route_data->dst_port = static_cast<uint16_t>(port);
      assert(status == BF_SUCCESS);

      status = data.getValue(ipRoute_route_action_dst_mac_field_id, &port);
      route_data->dst_mac = port;
      assert(status == BF_SUCCESS);

      return;
    }

    // This function processes the entry obtained by a get call. Based on the action
    // id the data object is intepreted.
    void ipRoute_process_entry_get(const bfrt::BfRtTableData &data,
                                   IpRouteData *ipRoute_data)
    {
      // First get actionId, then based on that, fill in appropriate fields
      bf_status_t bf_status;
      bf_rt_id_t action_id;

      bf_status = data.actionIdGet(&action_id);
      assert(bf_status == BF_SUCCESS);

      if (action_id == ipRoute_route_action_id)
      {
        ipRoute_process_route_entry_get(data, &ipRoute_data->data.route_data);
      }
      return;
    }

    // This function reads an entry specified by the ipRoute_key, and fills in the
    // passedin IpRoute object
    void ipRoute_entry_get(const IpRouteKey &ipRoute_key, IpRouteData *data)
    {
      // Reset key and data before use
      ipRouteTable->keyReset(bfrtTableKey.get());
      // Data reset is done without action-id, since the action-id is filled in by
      // the get function
      ipRouteTable->dataReset(bfrtTableData.get());

      ipRoute_key_setup(ipRoute_key, bfrtTableKey.get());

      bf_status_t status = BF_SUCCESS;
      // Entry get from hardware with the flag set to read from hardware
      auto flag = bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_HW;
      status = ipRouteTable->tableEntryGet(
          *session, dev_tgt, *bfrtTableKey, flag, bfrtTableData.get());
      assert(status == BF_SUCCESS);
      session->sessionCompleteOperations();

      ipRoute_process_entry_get(*bfrtTableData, data);

      return;
    }

    // This function deletes an entry specified by the ipRoute_key
    void ipRoute_entry_delete(const IpRouteKey &ipRoute_key)
    {
      // Reset key before use
      ipRouteTable->keyReset(bfrtTableKey.get());

      ipRoute_key_setup(ipRoute_key, bfrtTableKey.get());

      auto status = ipRouteTable->tableEntryDel(*session, dev_tgt, *bfrtTableKey);
      assert(status == BF_SUCCESS);
      session->sessionCompleteOperations();
      return;
    }

    /*******************************************************************************
     * Utility functions associated with "EgressRoute" table in the P4 program.
     ******************************************************************************/
    void egress_route_key_setup(const EgressRouteKey &egress_key,
                            bfrt::BfRtTableKey *table_key)
    {
      auto bf_status = table_key->setValue(egressRoute_port_field_id,
                                      static_cast<uint16_t>(egress_key.dst_port));
      assert(bf_status == BF_SUCCESS);
      return;
    }

    void
    egress_route_data_setup(const EgressRouteData &egress_data,
                            bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(egressRoute_action_dst_mac_field_id,
                               static_cast<uint64_t>(egress_data.dstMac));
      assert(bf_status == BF_SUCCESS);

      bf_status = table_data->setValue(egressRoute_action_dst_ip_field_id,
                                       static_cast<uint64_t>(egress_data.dstAddr));
      assert(bf_status == BF_SUCCESS);
    }

    void egress_route_entry_add_modify(const EgressRouteKey &egress_key,
                                       const EgressRouteData &egress_data,
                                       const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      egressRouteTable->keyReset(bfrtEgressRouteTableKey.get());
      egressRouteTable->dataReset(egressRoute_action_id, bfrtEgressRouteTableData.get());

      // Fill in the Key and Data object
      egress_route_key_setup(egress_key, bfrtEgressRouteTableKey.get());
      egress_route_data_setup(egress_data, bfrtEgressRouteTableData.get());

      call_table_entry_add_modify(
          bfrtEgressRouteTableKey, bfrtEgressRouteTableData, egressRouteTable, add);
    }

    void egress_route_entry_get(const EgressRouteKey &egress_key, EgressRouteData *data)
    {
      // Reset key and data before use
      egressRouteTable->keyReset(bfrtEgressRouteTableKey.get());
      egressRouteTable->dataReset(bfrtEgressRouteTableData.get());

      egress_route_key_setup(egress_key, bfrtEgressRouteTableKey.get());
      call_table_entry_get(bfrtEgressRouteTableKey, bfrtEgressRouteTableData, egressRouteTable);

      bf_status_t status = BF_SUCCESS;
      uint64_t edata;
      status = bfrtEgressRouteTableData->getValue(egressRoute_action_dst_ip_field_id, &edata);
      data->dstAddr = static_cast<uint32_t>(edata);
      assert(status == BF_SUCCESS);

      status = bfrtEgressRouteTableData->getValue(egressRoute_action_dst_mac_field_id, &edata);
      data->dstMac = static_cast<uint64_t>(edata);
      assert(status == BF_SUCCESS);
    }

    void egress_update_req_key_setup(const EgressUpdateReqKey &egress_key,
                                bfrt::BfRtTableKey *table_key)
    {
      auto bf_status = table_key->setValue(egressUpdateReq_dst_ip_field_id,
                                           static_cast<uint64_t>(egress_key.dstAddr));
      assert(bf_status == BF_SUCCESS);
      return;
    }

    void
    egress_update_req_data_setup(const EgressUpdateReqData &egress_data,
                            bfrt::BfRtTableData *table_data)
    {
      auto bf_status = table_data->setValue(egressUpdateReq_action_src_ip_field_id,
                                       static_cast<uint64_t>(egress_data.srcAddr));
      assert(bf_status == BF_SUCCESS);
    }

    void egress_update_req_entry_add_modify(const EgressUpdateReqKey &egress_key,
                                      const EgressUpdateReqData &egress_data,
                                      const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      egressUpdateReqTable->keyReset(bfrtEgressUpdateReqTableKey.get());
      egressUpdateReqTable->dataReset(egressUpdateReq_action_id, bfrtEgressUpdateReqTableData.get());

      // Fill in the Key and Data object
      egress_update_req_key_setup(egress_key, bfrtEgressUpdateReqTableKey.get());
      egress_update_req_data_setup(egress_data, bfrtEgressUpdateReqTableData.get());

      call_table_entry_add_modify(
          bfrtEgressUpdateReqTableKey, bfrtEgressUpdateReqTableData, egressUpdateReqTable, add);
    }

    void egress_update_req_entry_get(const EgressUpdateReqKey &egress_key, EgressUpdateReqData *data)
    {
      // Reset key and data before use
      egressUpdateReqTable->keyReset(bfrtEgressUpdateReqTableKey.get());
      egressUpdateReqTable->dataReset(bfrtEgressUpdateReqTableData.get());

      egress_update_req_key_setup(egress_key, bfrtEgressUpdateReqTableKey.get());
      call_table_entry_get(bfrtEgressUpdateReqTableKey, bfrtEgressUpdateReqTableData, egressUpdateReqTable);

      bf_status_t status = BF_SUCCESS;
      uint64_t edata;
      status = bfrtEgressUpdateReqTableData->getValue(egressUpdateReq_action_src_ip_field_id, &edata);
      data->srcAddr = static_cast<uint32_t>(edata);
      assert(status == BF_SUCCESS);
    }

    /*******************************************************************************
     * Utility functions associated with "invalidationRoute"
     ******************************************************************************/
    void egress_inv_route_key_setup(const EgressInvRouteKey &egress_key,
                                    bfrt::BfRtTableKey *table_key)
    {
      auto bf_status = table_key->setValue(egressInvRoute_port_field_id,
                                           static_cast<uint64_t>(egress_key.dst_port));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_key->setValue(egressInvRoute_inv_id_id,
                                      static_cast<uint64_t>(egress_key.inv_idx));
      assert(bf_status == BF_SUCCESS);
      return;
    }

    void
    egress_inv_route_data_setup(const EgressInvRouteData &egress_data,
                                bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(egressInvRoute_action_dst_mac_field_id,
                               static_cast<uint64_t>(egress_data.dstMac));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_data->setValue(egressInvRoute_action_dst_ip_field_id,
                                       static_cast<uint64_t>(egress_data.dstAddr));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_data->setValue(egressInvRoute_action_dst_qp_field_id,
                                       static_cast<uint64_t>(egress_data.destQp));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_data->setValue(egressInvRoute_action_dst_rkey_field_id,
                                       static_cast<uint64_t>(egress_data.rkey));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_data->setValue(egressInvRoute_action_inv_vaddr_field_id,
                                       static_cast<uint64_t>(egress_data.vaddr));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_data->setValue(egressInvRoute_action_reg_field_id,
                                       static_cast<uint64_t>(egress_data.regIdx));
      assert(bf_status == BF_SUCCESS);
    }

    void egress_inv_route_entry_add_modify(const EgressInvRouteKey &egress_key,
                                       const EgressInvRouteData &egress_data,
                                       const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      egressInvRouteTable->keyReset(bfrtEgressInvRouteTableKey.get());
      egressInvRouteTable->dataReset(egressInvRoute_action_id, bfrtEgressInvRouteTableData.get());

      // Fill in the Key and Data object
      egress_inv_route_key_setup(egress_key, bfrtEgressInvRouteTableKey.get());
      egress_inv_route_data_setup(egress_data, bfrtEgressInvRouteTableData.get());

      call_table_entry_add_modify(
          bfrtEgressInvRouteTableKey, bfrtEgressInvRouteTableData, egressInvRouteTable, add);
    }

    void egress_inv_route_entry_get(const EgressInvRouteKey &egress_key, EgressInvRouteData *data)
    {
      // Reset key and data before use
      egressInvRouteTable->keyReset(bfrtEgressInvRouteTableKey.get());
      egressInvRouteTable->dataReset(bfrtEgressInvRouteTableData.get());

      egress_inv_route_key_setup(egress_key, bfrtEgressInvRouteTableKey.get());
      call_table_entry_get(bfrtEgressInvRouteTableKey, bfrtEgressInvRouteTableData, egressInvRouteTable);

      bf_status_t status = BF_SUCCESS;
      uint64_t edata;
      status = bfrtEgressInvRouteTableData->getValue(egressInvRoute_action_dst_ip_field_id, &edata);
      data->dstAddr = static_cast<uint32_t>(edata);
      assert(status == BF_SUCCESS);

      status = bfrtEgressInvRouteTableData->getValue(egressInvRoute_action_dst_mac_field_id, &edata);
      data->dstMac = static_cast<uint64_t>(edata);
      assert(status == BF_SUCCESS);
    }

    /*******************************************************************************
     * Utility functions associated with "translateAckToRoce" table
     ******************************************************************************/
    void ackTrans_key_setup(const EgressAckTransKey &ackTrans_key,
                            bfrt::BfRtTableKey *table_key)
    {
      // Set value into the key object. Key type is "EXACT"
      auto bf_status = table_key->setValue(
          egressAckTrans_ip_dst_field_id, static_cast<uint64_t>(ackTrans_key.ipDstAddr));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_key->setValue(
          egressAckTrans_dst_qp_field_id, static_cast<uint64_t>(ackTrans_key.destQp));
      assert(bf_status == BF_SUCCESS);
      return;
    }

    void ackTrans_data_setup(const EgressAckTransData &ackTrans_data,
                             bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(egressAckTrans_action_dst_qp_field_id,
                               static_cast<uint64_t>(ackTrans_data.destQp));
      assert(bf_status == BF_SUCCESS);
      bf_status =
          table_data->setValue(egressAckTrans_action_dst_rkey_field_id,
                               static_cast<uint64_t>(ackTrans_data.rkey));
      assert(bf_status == BF_SUCCESS);
      bf_status =
          table_data->setValue(egressAckTrans_action_vaddr_field_id,
                               static_cast<uint64_t>(ackTrans_data.vaddr));
      assert(bf_status == BF_SUCCESS);
      bf_status =
          table_data->setValue(egressAckTrans_action_reg_field_id,
                               static_cast<uint64_t>(ackTrans_data.regIdx));
      assert(bf_status == BF_SUCCESS);

      return;
    }

    void ackTrans_entry_add_modify(const EgressAckTransKey &ackTrans_key,
                                   const EgressAckTransData &ackTrans_data,
                                   const bool &add)
    {
      // Reset key and data before use
      egressAckTransTable->keyReset(bfrtEgressAckTransTableKey.get());
      egressAckTransTable->dataReset(egressAckTrans_action_id, bfrtEgressAckTransTableData.get());

      // Fill in the Key and Data object
      ackTrans_key_setup(ackTrans_key, bfrtEgressAckTransTableKey.get());
      ackTrans_data_setup(ackTrans_data, bfrtEgressAckTransTableData.get());

      call_table_entry_add_modify(
          bfrtEgressAckTransTableKey, bfrtEgressAckTransTableData, egressAckTransTable, add);
    }

    /*******************************************************************************
     * Utility functions associated with "roceReq" and "roceAck" tables
     ******************************************************************************/
    // Functions for RoCE
    void roce_req_key_setup(const RoceReqKey &roce_req_key,
                            bfrt::BfRtTableKey *table_key)
    {
      // Set value into the key object. Key type is "EXACT"
      auto bf_status = table_key->setValue(roce_req_ip_src_field_id,
                                           static_cast<uint64_t>(roce_req_key.ipSrcAddr));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_key->setValue(roce_req_ip_dst_field_id,
                                      static_cast<uint64_t>(roce_req_key.ipDstAddr));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_key->setValue(roce_req_qp_field_id,
                                      static_cast<uint64_t>(roce_req_key.roceDestQp));
      assert(bf_status == BF_SUCCESS);
    }

    void roce_ack_key_setup(const RoceAckKey &roce_ack_key,
                            bfrt::BfRtTableKey *table_key)
    {
      // Set value into the key object. Key type is "EXACT"
      auto bf_status = table_key->setValue(roce_ack_qp_field_id,
                                           static_cast<uint64_t>(roce_ack_key.roceDestQp));
      assert(bf_status == BF_SUCCESS);
    }

    void roce_dummy_ack_key_setup(const RoceDummyAckKey &roce_dummy_ack_key,
                                  bfrt::BfRtTableKey *table_key)
    {
      auto bf_status = table_key->setValue(roce_dummy_ack_ip_src_field_id,
                                           static_cast<uint64_t>(roce_dummy_ack_key.ipSrcAddr));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_key->setValue(roce_dummy_ack_qp_field_id,
                                      static_cast<uint64_t>(roce_dummy_ack_key.roceDestQp));
      assert(bf_status == BF_SUCCESS);
    }

    void roce_ack_dest_key_setup(const RoceAckDestKey &roce_ack_dest_key,
                                 bfrt::BfRtTableKey *table_key)
    {
      auto bf_status = table_key->setValue(roce_ack_dest_ip_src_field_id,
                                           static_cast<uint64_t>(roce_ack_dest_key.ipSrcAddr));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_key->setValue(roce_ack_dest_qp_field_id,
                                      static_cast<uint64_t>(roce_ack_dest_key.roceDestQp));
      assert(bf_status == BF_SUCCESS);
    }

    void set_qp_idx_key_setup(const SetQpIdxKey &set_qp_idx_key,
                              bfrt::BfRtTableKey *table_key)
    {
      auto bf_status = table_key->setValue(set_qp_idx_ip_src_field_id,
                                           static_cast<uint64_t>(set_qp_idx_key.ipSrcAddr));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_key->setValue(set_qp_idx_qp_field_id,
                                      static_cast<uint64_t>(set_qp_idx_key.cpuQpId));
      assert(bf_status == BF_SUCCESS);
    }

    void sender_qp_key_setup(const SenderQpKey &sender_qp_key,
                            bfrt::BfRtTableKey *table_key)
    {
      auto bf_status = table_key->setValue(sender_qp_ip_src_field_id,
                                           static_cast<uint64_t>(sender_qp_key.ipSrcAddr));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_key->setValue(sender_qp_cpu_qp_id_field_id,
                                      static_cast<uint64_t>(sender_qp_key.cpuQpId));
      assert(bf_status == BF_SUCCESS);
    }

    void
    roce_req_data_setup(const RoceReqData &roce_req_data,
                        bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(roce_req_action_dst_qp_field_id,
                               static_cast<uint64_t>(roce_req_data.destQp));
      assert(bf_status == BF_SUCCESS);

      bf_status = table_data->setValue(roce_req_action_dst_rkey_field_id,
                                       static_cast<uint64_t>(roce_req_data.rkey));
      assert(bf_status == BF_SUCCESS);

      bf_status = table_data->setValue(roce_req_action_src_ip_field_id,
                                       static_cast<uint64_t>(roce_req_data.srcAddr));
      assert(bf_status == BF_SUCCESS);

      bf_status = table_data->setValue(roce_req_action_src_mac_field_id,
                                       static_cast<uint64_t>(roce_req_data.srcMac));
      assert(bf_status == BF_SUCCESS);

      bf_status = table_data->setValue(roce_req_action_reg_idx_field_id,
                                       static_cast<uint64_t>(roce_req_data.regIdx));
      assert(bf_status == BF_SUCCESS);
    }

    void
    roce_ack_data_setup(const RoceAckData &roce_ack_data,
                        bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(roce_ack_action_dst_qp_field_id,
                               static_cast<uint64_t>(roce_ack_data.destQp));
      assert(bf_status == BF_SUCCESS);

      bf_status = table_data->setValue(roce_ack_action_dst_ip_field_id,
                                       static_cast<uint64_t>(roce_ack_data.dstAddr));
      assert(bf_status == BF_SUCCESS);

      bf_status = table_data->setValue(roce_ack_action_src_ip_field_id,
                                       static_cast<uint64_t>(roce_ack_data.srcAddr));
      assert(bf_status == BF_SUCCESS);

      bf_status = table_data->setValue(roce_ack_action_reg_idx_field_id,
                                       static_cast<uint64_t>(roce_ack_data.regIdx));
      assert(bf_status == BF_SUCCESS);

    }

    void
    roce_dummy_ack_data_setup(const RoceDummyAckData &roce_dummy_ack_data,
                              bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(roce_dummy_ack_action_dst_qp_field_id,
                               static_cast<uint64_t>(roce_dummy_ack_data.destQp));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_data->setValue(roce_dummy_ack_action_vaddr_field_id,
                               static_cast<uint64_t>(roce_dummy_ack_data.vaddr));
      assert(bf_status == BF_SUCCESS);
    }

    void
    roce_ack_dest_data_setup(const RoceAckDestData &roce_ack_dest_data,
                             bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(roce_ack_dest_action_dst_qp_field_id,
                               static_cast<uint64_t>(roce_ack_dest_data.destQp));
      assert(bf_status == BF_SUCCESS);
      bf_status = table_data->setValue(roce_ack_dest_action_dummy_qp_field_id,
                               static_cast<uint64_t>(roce_ack_dest_data.dummyQPId));
      assert(bf_status == BF_SUCCESS);
    }

    void
    set_qp_idx_data_setup(const SetQpIdxData &set_qp_idx_data,
                          bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(set_qp_idx_action_qp_idx_field_id,
                               static_cast<uint64_t>(set_qp_idx_data.globalQpId));
      assert(bf_status == BF_SUCCESS);
    }

    void
    sender_qp_data_setup(const SenderQpData &sender_qp_data,
                        bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(sender_qp_action_qp_idx_field_id,
                               static_cast<uint64_t>(sender_qp_data.memQpId));
      assert(bf_status == BF_SUCCESS);
    }

    void roce_req_entry_add_modify(const RoceReqKey &roceReq_key,
                                   const RoceReqData &roceReq_data,
                                   const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      roceReqTable->keyReset(bfrtRoceReqTableKey.get());
      roceReqTable->dataReset(roce_req_action_id, bfrtRoceReqTableData.get());

      // Fill in the Key and Data object
      roce_req_key_setup(roceReq_key, bfrtRoceReqTableKey.get());
      roce_req_data_setup(roceReq_data, bfrtRoceReqTableData.get());

      call_table_entry_add_modify(
          bfrtRoceReqTableKey, bfrtRoceReqTableData, roceReqTable, add);
    }

    void roce_ack_entry_add_modify(const RoceAckKey &roceAck_key,
                                   const RoceAckData &roceAck_data,
                                   const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      roceAckTable->keyReset(bfrtRoceAckTableKey.get());
      roceAckTable->dataReset(roce_ack_action_id, bfrtRoceAckTableData.get());

      // Fill in the Key and Data object
      roce_ack_key_setup(roceAck_key, bfrtRoceAckTableKey.get());
      roce_ack_data_setup(roceAck_data, bfrtRoceAckTableData.get());

      call_table_entry_add_modify(
          bfrtRoceAckTableKey, bfrtRoceAckTableData, roceAckTable, add);
    }

    void roce_dummy_ack_entry_add_modify(const RoceDummyAckKey &roceDummyAck_key,
                                         const RoceDummyAckData &roceDummyAck_data,
                                         const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      roceDummyAckTable->keyReset(bfrtRoceDummyAckTableKey.get());
      roceDummyAckTable->dataReset(roce_dummy_ack_action_id, bfrtRoceDummyAckTableData.get());

      // Fill in the Key and Data object
      roce_dummy_ack_key_setup(roceDummyAck_key, bfrtRoceDummyAckTableKey.get());
      roce_dummy_ack_data_setup(roceDummyAck_data, bfrtRoceDummyAckTableData.get());

      call_table_entry_add_modify(
          bfrtRoceDummyAckTableKey, bfrtRoceDummyAckTableData, roceDummyAckTable, add);
    }

    void roce_ack_dest_entry_add_modify(const RoceAckDestKey &roceAckDest_key,
                                         const RoceAckDestData &roceAckDest_data,
                                         const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      roceAckDestTable->keyReset(bfrtRoceAckDestTableKey.get());
      roceAckDestTable->dataReset(roce_ack_dest_action_id, bfrtRoceAckDestTableData.get());

      // Fill in the Key and Data object
      roce_ack_dest_key_setup(roceAckDest_key, bfrtRoceAckDestTableKey.get());
      roce_ack_dest_data_setup(roceAckDest_data, bfrtRoceAckDestTableData.get());

      call_table_entry_add_modify(
          bfrtRoceAckDestTableKey, bfrtRoceAckDestTableData, roceAckDestTable, add);
    }

    void set_qp_idx_entry_add_modify(const SetQpIdxKey &set_qp_idx_key,
                                     const SetQpIdxData &set_qp_idx_data,
                                     const bool &add)
    {
      setQpIdxTable->keyReset(bfrtSetQpIdxTableKey.get());
      setQpIdxTable->dataReset(set_qp_idx_action_id, bfrtSetQpIdxTableData.get());

      // Fill in the Key and Data object
      set_qp_idx_key_setup(set_qp_idx_key, bfrtSetQpIdxTableKey.get());
      set_qp_idx_data_setup(set_qp_idx_data, bfrtSetQpIdxTableData.get());

      call_table_entry_add_modify(
          bfrtSetQpIdxTableKey, bfrtSetQpIdxTableData, setQpIdxTable, add);
    }

    void sender_qp_entry_add_modify(const SenderQpKey &sender_qp_key,
                                   const SenderQpData &sender_qp_data,
                                   const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      senderQpTable->keyReset(bfrtSenderQpTableKey.get());
      senderQpTable->dataReset(sender_qp_action_id, bfrtSenderQpTableData.get());

      // Fill in the Key and Data object
      sender_qp_key_setup(sender_qp_key, bfrtSenderQpTableKey.get());
      sender_qp_data_setup(sender_qp_data, bfrtSenderQpTableData.get());

      call_table_entry_add_modify(
          bfrtSenderQpTableKey, bfrtSenderQpTableData, senderQpTable, add);
    }

    void roce_req_entry_get(const RoceReqKey &roce_req_key, RoceReqData *data)
    {
      // Reset key and data before use
      roceReqTable->keyReset(bfrtRoceReqTableKey.get());
      roceReqTable->dataReset(bfrtRoceReqTableData.get());

      roce_req_key_setup(roce_req_key, bfrtRoceReqTableKey.get());
      call_table_entry_get(bfrtRoceReqTableKey, bfrtRoceReqTableData, roceReqTable);

      bf_status_t status = BF_SUCCESS;
      uint64_t edata;
      status = bfrtRoceReqTableData->getValue(roce_req_action_dst_qp_field_id, &edata);
      data->destQp = static_cast<uint32_t>(edata);
      assert(status == BF_SUCCESS);

      status = bfrtRoceReqTableData->getValue(roce_req_action_dst_rkey_field_id, &edata);
      data->rkey = static_cast<uint32_t>(edata);
      assert(status == BF_SUCCESS);

      status = bfrtRoceReqTableData->getValue(roce_req_action_src_ip_field_id, &edata);
      data->srcAddr = static_cast<uint32_t>(edata);
      assert(status == BF_SUCCESS);

      status = bfrtRoceReqTableData->getValue(roce_req_action_src_mac_field_id, &edata);
      data->srcMac = static_cast<uint64_t>(edata);
      assert(status == BF_SUCCESS);

      status = bfrtRoceReqTableData->getValue(roce_req_action_reg_idx_field_id, &edata);
      data->regIdx = static_cast<uint16_t>(edata);
      assert(status == BF_SUCCESS);
    }

    void roce_ack_entry_get(const RoceAckKey &roce_ack_key, RoceAckData *data)
    {
      // Reset key and data before use
      roceAckTable->keyReset(bfrtRoceAckTableKey.get());
      roceAckTable->dataReset(bfrtRoceAckTableData.get());

      roce_ack_key_setup(roce_ack_key, bfrtRoceAckTableKey.get());
      call_table_entry_get(bfrtRoceAckTableKey, bfrtRoceAckTableData, roceAckTable);

      bf_status_t status = BF_SUCCESS;
      uint64_t edata;
      status = bfrtRoceAckTableData->getValue(roce_ack_action_dst_qp_field_id, &edata);
      data->destQp = static_cast<uint32_t>(edata);
      assert(status == BF_SUCCESS);

      status = bfrtRoceAckTableData->getValue(roce_ack_action_dst_ip_field_id, &edata);
      data->dstAddr = static_cast<uint32_t>(edata);
      assert(status == BF_SUCCESS);

      status = bfrtRoceAckTableData->getValue(roce_ack_action_src_ip_field_id, &edata);
      data->srcAddr = static_cast<uint32_t>(edata);
      assert(status == BF_SUCCESS);

      status = bfrtRoceAckTableData->getValue(roce_ack_action_reg_idx_field_id, &edata);
      data->regIdx = static_cast<uint16_t>(edata);
      assert(status == BF_SUCCESS);
    }
    // TODO: entry delete functions

    /*******************************************************************************
     * Utility functions associated with "isLastAckForModData" table
     ******************************************************************************/
    void cache_owner_key_setup(const CacheOwnerCheckKey &cache_owner_key,
                            bfrt::BfRtTableKey *table_key)
    {
      auto bf_status = table_key->setValue(cache_owner_check_mask_field_id,
                                           static_cast<uint64_t>(cache_owner_key.sharer_mask));
      assert(bf_status == BF_SUCCESS);
    }

    void
    cache_owner_data_setup(bfrt::BfRtTableData *table_data)
    {
      (void)table_data;
    }

    void cache_owner_entry_add_modify(const CacheOwnerCheckKey &cache_owner_key,
                                      const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      cacheOwnerCheckTable->keyReset(bfrtCacheOwnerCheckKey.get());
      cacheOwnerCheckTable->dataReset(cache_owner_check_action_id, bfrtCacheOwnerCheckData.get());

      // Fill in the Key and Data object
      cache_owner_key_setup(cache_owner_key, bfrtCacheOwnerCheckKey.get());
      // cache_owner_data_setup(bfrtCacheOwnerCheckData.get());

      call_table_entry_add_modify(
          bfrtCacheOwnerCheckKey, bfrtCacheOwnerCheckData, cacheOwnerCheckTable, add);
    }


    /*******************************************************************************
     * Utility functions associated with "addrTrans" and "accCtrl" tables
     ******************************************************************************/
    // key setup
    void addr_trans_key_setup(const AddrTransKey &addr_trans_key,
                              bfrt::BfRtTableKey *table_key)
    {
      auto bf_status = table_key->setValueLpm(addr_trans_vaddr_field_id,
                                              static_cast<uint64_t>(addr_trans_key.vaddr),
                                              static_cast<uint16_t>(addr_trans_key.vaddr_prefix));
      assert(bf_status == BF_SUCCESS);
    }

    void addr_except_trans_key_setup(const AddrExceptTransKey &addr_except_trans_key,
                                     bfrt::BfRtTableKey *table_key)
    {
      // Set value into the key object. Key type is "EXACT"
      auto bf_status = table_key->setValueLpm(addr_except_trans_vaddr_field_id,
                                              static_cast<uint64_t>(addr_except_trans_key.vaddr),
                                              static_cast<uint16_t>(addr_except_trans_key.vaddr_prefix));
      assert(bf_status == BF_SUCCESS);
    }

    // Data setup
    void
    addr_trans_data_setup(const AddrTransData &addr_trans_data,
                          bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(addr_trans_action_dst_ip_field_id,
                               static_cast<uint64_t>(addr_trans_data.dstAddr));
      assert(bf_status == BF_SUCCESS);

      bf_status = table_data->setValue(addr_trans_action_va_to_dma_field_id,
                                       static_cast<uint64_t>(addr_trans_data.vaddrToDmaAddr));
      assert(bf_status == BF_SUCCESS);
    }

    void
    addr_except_trans_data_setup(const AddrExceptTransData &addr_except_trans_data,
                                 bfrt::BfRtTableData *table_data)
    {
      auto bf_status =
          table_data->setValue(addr_except_trans_action_dst_ip_field_id,
                               static_cast<uint64_t>(addr_except_trans_data.dstAddr));
      assert(bf_status == BF_SUCCESS);

      bf_status = table_data->setValue(addr_except_trans_action_va_to_dma_field_id,
                                       static_cast<uint64_t>(addr_except_trans_data.vaddrToDmaAddr));
      assert(bf_status == BF_SUCCESS);

      bf_status = table_data->setValue(addr_except_trans_action_permission_field_id,
                                       static_cast<uint64_t>(addr_except_trans_data.permission));
      assert(bf_status == BF_SUCCESS);
    }

    // add / modify entry
    void addr_trans_entry_add_modify(const AddrTransKey &addr_trans_key,
                                     const AddrTransData &table_data,
                                     const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      addrTransTable->keyReset(bfrtAddrTransTableKey.get());
      addrTransTable->dataReset(addr_trans_action_id, bfrtAddrTransTableData.get());

      // Fill in the Key and Data object
      addr_trans_key_setup(addr_trans_key, bfrtAddrTransTableKey.get());
      addr_trans_data_setup(table_data, bfrtAddrTransTableData.get());

      call_table_entry_add_modify(
          bfrtAddrTransTableKey, bfrtAddrTransTableData, addrTransTable, add);
    }

    void addr_except_trans_entry_add_modify(const AddrExceptTransKey &addr_except_trans_key,
                                            const AddrExceptTransData &table_data,
                                            const bool &add)
    {
      // Adding a match entry with below mac Addr to be forwarded to the below port
      // Reset key and data before use
      addrExceptTransTable->keyReset(bfrtAddrExceptTransTableKey.get());
      addrExceptTransTable->dataReset(addr_except_trans_action_id, bfrtAddrExceptTransTableData.get());

      // Fill in the Key and Data object
      addr_except_trans_key_setup(addr_except_trans_key, bfrtAddrExceptTransTableKey.get());
      addr_except_trans_data_setup(table_data, bfrtAddrExceptTransTableData.get());

      call_table_entry_add_modify(
          bfrtAddrExceptTransTableKey, bfrtAddrExceptTransTableData, addrExceptTransTable, add);
    }

    // get entry
    void addr_trans_entry_get(const AddrTransKey &addr_trans_key, AddrTransData *data)
    {
      // Reset key and data before use
      addrTransTable->keyReset(bfrtAddrTransTableKey.get());
      addrTransTable->dataReset(bfrtAddrTransTableData.get());

      addr_trans_key_setup(addr_trans_key, bfrtAddrTransTableKey.get());
      call_table_entry_get(bfrtAddrTransTableKey, bfrtAddrTransTableData, addrTransTable);

      bf_status_t status = BF_SUCCESS;
      uint64_t edata;
      status = bfrtAddrTransTableData->getValue(addr_trans_action_dst_ip_field_id, &edata);
      data->dstAddr = static_cast<uint32_t>(edata);
      assert(status == BF_SUCCESS);

      status = bfrtAddrTransTableData->getValue(addr_trans_action_va_to_dma_field_id, &edata);
      data->vaddrToDmaAddr = static_cast<uint64_t>(edata);
      assert(status == BF_SUCCESS);
    }

    void addr_except_trans_entry_get(const AddrExceptTransKey &addr_except_trans_key, AddrExceptTransData *data)
    {
      // Reset key and data before use
      addrExceptTransTable->keyReset(bfrtAddrExceptTransTableKey.get());
      addrExceptTransTable->dataReset(bfrtAddrExceptTransTableData.get());

      addr_except_trans_key_setup(addr_except_trans_key, bfrtAddrExceptTransTableKey.get());
      call_table_entry_get(bfrtAddrExceptTransTableKey, bfrtAddrExceptTransTableData, addrExceptTransTable);

      bf_status_t status = BF_SUCCESS;
      uint64_t edata;
      status = bfrtAddrExceptTransTableData->getValue(addr_except_trans_action_dst_ip_field_id, &edata);
      data->dstAddr = static_cast<uint32_t>(edata);
      assert(status == BF_SUCCESS);

      status = bfrtAddrExceptTransTableData->getValue(addr_except_trans_action_va_to_dma_field_id, &edata);
      data->vaddrToDmaAddr = static_cast<uint64_t>(edata);
      assert(status == BF_SUCCESS);

      status = bfrtAddrExceptTransTableData->getValue(addr_except_trans_action_permission_field_id, &edata);
      data->permission = static_cast<uint8_t>(edata);
      assert(status == BF_SUCCESS);
    }

    // delete entry
    void addr_trans_entry_delete(const AddrTransKey &addr_trans_key)
    {
      // Reset key before use
      addrTransTable->keyReset(bfrtAddrTransTableKey.get());

      addr_trans_key_setup(addr_trans_key, bfrtAddrTransTableKey.get());

      auto status = addrTransTable->tableEntryDel(*session, dev_tgt, *bfrtAddrTransTableKey);
      assert(status == BF_SUCCESS);
      session->sessionCompleteOperations();
      return;
    }

    void addr_except_trans_entry_delete(const AddrExceptTransKey &addr_except_trans_key)
    {
      // Reset key before use
      addrExceptTransTable->keyReset(bfrtAddrExceptTransTableKey.get());

      addr_except_trans_key_setup(addr_except_trans_key, bfrtAddrExceptTransTableKey.get());

      auto status = addrExceptTransTable->tableEntryDel(*session, dev_tgt, *bfrtAddrExceptTransTableKey);
      assert(status == BF_SUCCESS);
      session->sessionCompleteOperations();
      return;
    }

    /*******************************************************************************
     * Main page set-up and initialization
     ******************************************************************************/

    static void add_ipRoute_rule(const std::string ip_addr, uint16_t port, const std::string dmac)
    {
      // Allocate initial data for servers
      IpRouteKey route_key;
      IpRouteData route_data;

      route_key.ipDstAddr = parse_ipv4(ip_addr);
      route_data.action_id = ipRoute_route_action_id;
      route_data.data.route_data.dst_port = port;
      route_data.data.route_data.dst_mac = parse_mac(dmac);
      ipRoute_entry_add_modify_with_route(route_key, route_data.data.route_data, true);
    }

    static void add_egressRoute_rule(uint16_t port, const std::string ip_addr, const std::string dmac)
    {
      // Allocate initial data for servers
      EgressRouteKey route_key;
      EgressRouteData route_data;

      route_key.dst_port = port;
      route_data.dstAddr = parse_ipv4(ip_addr);
      route_data.dstMac = parse_mac(dmac);
      egress_route_entry_add_modify(route_key, route_data, true);
    }

    static void add_egressUpdateReq_rule(const std::string dst_addr, const std::string ip_addr)
    {
      // Allocate initial data for servers
      EgressUpdateReqKey update_req_key;
      EgressUpdateReqData update_req_data;

      update_req_key.dstAddr = parse_ipv4(dst_addr);
      update_req_data.srcAddr = parse_ipv4(ip_addr);
      egress_update_req_entry_add_modify(update_req_key, update_req_data, true);
    }

    void add_egressInvRoute_rule(int nid, int inv_idx, uint32_t qp, uint32_t rkey, uint64_t vaddr, uint16_t reg_idx)
    {
      // Allocate initial data for servers
      EgressInvRouteKey route_key;
      EgressInvRouteData route_data;

      route_key.dst_port = get_comp_port(nid);
      route_key.inv_idx = inv_idx;
      route_data.dstAddr = parse_ipv4(get_comp_ip(nid));
      route_data.dstMac = parse_mac(get_comp_mac(nid));
      route_data.destQp = qp;
      route_data.rkey = rkey;
      route_data.vaddr = vaddr;
      route_data.regIdx = reg_idx;
      egress_inv_route_entry_add_modify(route_key, route_data, true);
    }

    void add_egressAckTrans_rule(const std::string dst_ip_addr, uint32_t qp, uint32_t new_qp,
                                 uint32_t rkey, uint64_t vaddr, uint16_t reg_idx)
    {
      // Allocate initial data for servers
      EgressAckTransKey ackTrans_key;
      EgressAckTransData ackTrans_data;

      ackTrans_key.destQp = qp;
      ackTrans_key.ipDstAddr = parse_ipv4(dst_ip_addr);
      ackTrans_data.destQp = new_qp;
      ackTrans_data.rkey = rkey;
      ackTrans_data.vaddr = vaddr;
      ackTrans_data.regIdx = reg_idx;
      ackTrans_entry_add_modify(ackTrans_key, ackTrans_data, true);
    }

    static const char controller_ip_addr[] = "10.10.10.1";
    static const char controller_mac_addr[] = "00:02:00:00:03:00";
    static const char dummy_mac_addr[] = "00:00:00:03:02:01";

    void add_roceReq_rule(const std::string src_ip_addr, const std::string dst_ip_addr,
                          uint32_t qp, uint32_t new_qp, uint32_t rkey, uint16_t reg_idx)
    {
      // Allocate initial data for servers
      RoceReqKey roce_req_key;
      RoceReqData roce_req_data;

      roce_req_key.ipSrcAddr = parse_ipv4(src_ip_addr);
      roce_req_key.ipDstAddr = parse_ipv4(dst_ip_addr);
      roce_req_key.roceDestQp = qp;
      roce_req_data.action_id = roce_req_action_id;
      roce_req_data.destQp = new_qp;
      roce_req_data.rkey = rkey;
      roce_req_data.srcAddr = parse_ipv4(controller_ip_addr);
      roce_req_data.srcMac = parse_mac(dummy_mac_addr);
      roce_req_data.regIdx = reg_idx;
      roce_req_entry_add_modify(roce_req_key, roce_req_data, true);
    }

    void add_roceAck_rule(uint32_t qp,
                          uint32_t new_qp, const std::string new_ip_addr,
                          uint16_t reg_idx)
    {
      // Allocate initial data for servers
      RoceAckKey roce_ack_key;
      RoceAckData roce_ack_data;

      roce_ack_key.roceDestQp = qp;
      roce_ack_data.action_id = roce_ack_action_id;
      roce_ack_data.destQp = new_qp;
      roce_ack_data.dstAddr = parse_ipv4(new_ip_addr);
      roce_ack_data.srcAddr = parse_ipv4(controller_ip_addr);
      roce_ack_data.regIdx = reg_idx;
      roce_ack_entry_add_modify(roce_ack_key, roce_ack_data, true);
    }

    void add_roceDummyAck_rule(uint32_t qp, const std::string src_ip_addr,
                               uint32_t new_qp, uint64_t vaddr)
    {
      RoceDummyAckKey roce_dummy_ack_key;
      RoceDummyAckData roce_dummy_ack_data;

      roce_dummy_ack_key.roceDestQp = qp;
      roce_dummy_ack_key.ipSrcAddr = parse_ipv4(src_ip_addr);
      roce_dummy_ack_data.destQp = new_qp;
      roce_dummy_ack_data.vaddr = vaddr;
      roce_dummy_ack_entry_add_modify(roce_dummy_ack_key, roce_dummy_ack_data, true);
    }

    void add_roceAckDest_rule(uint32_t qp, const std::string src_ip_addr,
                              uint32_t dest_qp_id, uint32_t dummy_qp_id)
    {
      RoceAckDestKey roce_ack_dest_key;
      RoceAckDestData roce_ack_dest_data;

      roce_ack_dest_key.roceDestQp = qp;
      roce_ack_dest_key.ipSrcAddr = parse_ipv4(src_ip_addr);
      roce_ack_dest_data.destQp = dest_qp_id;
      roce_ack_dest_data.dummyQPId = dummy_qp_id;
      roce_ack_dest_entry_add_modify(roce_ack_dest_key, roce_ack_dest_data, true);
    }

    void add_setQpIdx_rule(uint32_t qp, const std::string src_ip_addr,
                           uint16_t qp_id)
    {
      // Allocate initial data for servers
      SetQpIdxKey set_qp_idx_key;
      SetQpIdxData set_qp_idx_data;

      set_qp_idx_key.cpuQpId = qp;
      set_qp_idx_key.ipSrcAddr = parse_ipv4(src_ip_addr);
      set_qp_idx_data.globalQpId = qp_id;
      set_qp_idx_entry_add_modify(set_qp_idx_key, set_qp_idx_data, true);
    }

    void add_senderQp_rule(uint32_t cpu_qp_id, const std::string src_ip_addr,
                           uint16_t mem_qp_id)
    {
      // Allocate initial data for servers
      SenderQpKey sender_qp_key;
      SenderQpData sender_qp_data;

      sender_qp_key.cpuQpId = cpu_qp_id;
      sender_qp_key.ipSrcAddr = parse_ipv4(src_ip_addr);
      sender_qp_data.memQpId = mem_qp_id;
      sender_qp_entry_add_modify(sender_qp_key, sender_qp_data, true);
    }

    void add_cache_owner_rule(uint16_t sharer_mask)
    {
      // Allocate initial data for servers
      CacheOwnerCheckKey cache_owner_key;

      cache_owner_key.sharer_mask = sharer_mask;
      cache_owner_entry_add_modify(cache_owner_key, true);
    }

    void add_addrTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix,
                            const std::string dst_ip_addr, uint64_t va_to_dma)
    {
      // Allocate initial data for servers
      AddrTransKey addr_trans_key;
      AddrTransData addr_trans_data;

      addr_trans_key.vaddr = 0xFFFFFFFFFFFF & vaddr;  // Remove the first 16 bits (PID)
      addr_trans_key.vaddr_prefix = vaddr_prefix;     // Start from 48th bit (not 64 th) down to 1st
      addr_trans_data.action_id = addr_trans_action_id;
      addr_trans_data.dstAddr = parse_ipv4(dst_ip_addr);
      addr_trans_data.vaddrToDmaAddr = va_to_dma;
      addr_trans_entry_add_modify(addr_trans_key, addr_trans_data, true);
    }

    void del_addrTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix)
    {
      AddrTransKey addr_trans_key;

      // addr_trans_key.rkey = rkey;
      addr_trans_key.vaddr = vaddr;
      addr_trans_key.vaddr_prefix = vaddr_prefix; // must be >= 16, to fully match PID
      addr_trans_entry_delete(addr_trans_key);
    }

    void add_modify_addrExceptTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix,
                                         const std::string dst_ip_addr, uint64_t va_to_dma,
                                         uint8_t permission, int is_add)
    {
      // Allocate initial data for servers
      AddrExceptTransKey addr_except_trans_key;
      AddrExceptTransData addr_except_trans_data;

      addr_except_trans_key.vaddr = vaddr;
      addr_except_trans_key.vaddr_prefix = vaddr_prefix;  // must be >= 16, to fully match PID
      addr_except_trans_data.action_id = addr_trans_action_id;
      addr_except_trans_data.dstAddr = parse_ipv4(dst_ip_addr);
      addr_except_trans_data.vaddrToDmaAddr = va_to_dma;  // 64-bit: 16-bit of PID + 48-bit of VA
      addr_except_trans_data.permission = permission & 0xF; // last 4 bit
      addr_except_trans_entry_add_modify(addr_except_trans_key, addr_except_trans_data, is_add);
    }

    void del_addrExceptTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix)
    {
      AddrExceptTransKey addr_except_trans_key;

      addr_except_trans_key.vaddr = vaddr;
      addr_except_trans_key.vaddr_prefix = vaddr_prefix; // must be >= 16, to fully match PID
      addr_except_trans_entry_delete(addr_except_trans_key);
    }
    
    void table_insert_init_data()
    {
      // memory VMs
      for (int i = 0; i < BFRT_NUM_MEMORY_NODE; i++)
        add_ipRoute_rule(get_mem_ip(i), get_mem_port(i), get_mem_mac(i));  // direct map to NIC

      // computing VMs
      for (int i = 0; i < BFRT_NUM_COMPUTE_NODE; i++)
        add_ipRoute_rule(get_comp_ip(i), get_comp_port(i), get_comp_mac(i));

      // == Switch general CPU internal ports == //
      add_ipRoute_rule(controller_ip_addr, dev_cpu_port, controller_mac_addr);

      // == egress route rules - controller + memory nodes== //
      add_egressRoute_rule(dev_cpu_port, controller_ip_addr, controller_mac_addr);
      for (int i = 0; i < BFRT_NUM_MEMORY_NODE; i++)
        add_egressRoute_rule(get_mem_port(i), get_mem_ip(i), get_mem_mac(i));

      // == egress route rules - memory nodes (to make the packet sent from controller) == //
      for (int i = 0; i < BFRT_NUM_MEMORY_NODE; i++)
        add_egressUpdateReq_rule(get_mem_ip(i), controller_ip_addr);

      // Cache onwer detection for 16 nodes
      for (int i = 0; i< MAX_NUMBER_COMPUTE_NODE; i++)
      {
        add_cache_owner_rule(((uint16_t)0x1) << i);
      }

      // Static state transition rules
      table_insert_static_state_transitions();
    }

    // Function to iterate over all the entries in the table
    void table_iterate()
    {
      // Table iteration involves the following
      //    1. Use the getFirst API to get the first entry
      //    2. Use the tableUsageGet API to get the number of entries currently in
      //    the table.
      //    3. Use the number of entries returned in step 2 and pass it as a
      //    parameter to getNext_n (as n) to get all the remaining entries
      std::unique_ptr<BfRtTableKey> first_key;
      std::unique_ptr<BfRtTableData> first_data;

      auto bf_status = ipRouteTable->keyAllocate(&first_key);
      assert(bf_status == BF_SUCCESS);

      bf_status = ipRouteTable->dataAllocate(&first_data);
      assert(bf_status == BF_SUCCESS);

      auto flag = bfrt::BfRtTable::BfRtTableGetFlag::GET_FROM_HW;

      bf_status = ipRouteTable->tableEntryGetFirst(
          *session, dev_tgt, flag, first_key.get(), first_data.get());

      if (bf_status == BF_OBJECT_NOT_FOUND)
      {
        printf("No entry found!\n");
        return;
      }
      assert(bf_status == BF_SUCCESS);
      session->sessionCompleteOperations();

      // Process the first entry
      IpRouteData route_data;
      ipRoute_process_entry_get(*first_data, &route_data);

      // Get the usage of table
      uint32_t entry_count = 0;
      bf_status =
          ipRouteTable->tableUsageGet(*session, dev_tgt, flag, &entry_count);
      assert(bf_status == BF_SUCCESS);

      if (entry_count == 1)
      {
        return;
      }

      BfRtTable::keyDataPairs key_data_pairs;
      std::vector<std::unique_ptr<BfRtTableKey>> keys(entry_count - 1);
      std::vector<std::unique_ptr<BfRtTableData>> data(entry_count - 1);

      for (unsigned i = 0; i < entry_count - 1; ++i)
      {
        bf_status = ipRouteTable->keyAllocate(&keys[i]);
        assert(bf_status == BF_SUCCESS);

        bf_status = ipRouteTable->dataAllocate(&data[i]);
        assert(bf_status == BF_SUCCESS);

        key_data_pairs.push_back(std::make_pair(keys[i].get(), data[i].get()));
      }

      // Get next N
      uint32_t num_returned = 0;
      bf_status =
          ipRouteTable->tableEntryGetNext_n(*session,
                                            dev_tgt,
                                            *first_key.get(),
                                            entry_count - 1,
                                            flag,
                                            &key_data_pairs,
                                            &num_returned);
      assert(bf_status == BF_SUCCESS);
      assert(num_returned == entry_count - 1);
      session->sessionCompleteOperations();

      // Process the rest of the entries
      for (unsigned i = 0; i < entry_count - 1; ++i)
      {
        ipRoute_process_entry_get(*data[i], &route_data);
        // Do any required processing with the obtained data and key
      }
      return;
    }

    void memory_barrier(void)
    {
      _mem_barrier_();
    }
  } // namespace tna_disagg_switch
} // namespace bfrt
