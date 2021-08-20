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
#include <math.h>
#include <time.h>
#include "tna_disagg_bfrt.hpp"
#include "controller/debug.h"

#ifdef __cplusplus
extern "C"
{
#endif
#include <bf_switchd/bf_switchd.h>
#ifdef __cplusplus
}
#endif

// Exposed function for rule management
// RoCE request
static unsigned long max_at_rule_count = 0;
static unsigned long max_ac_rule_count = 0;
static unsigned long max_ac_page_count = 0;
static unsigned long max_ac_huge_page_count = 0;
extern "C" void bfrt_add_roce_req(char *src_ip_addr, char *dst_ip_addr, uint32_t qp,
                                  uint32_t new_qp, uint32_t rkey, uint16_t reg_idx);

void bfrt_add_roce_req(char *src_ip_addr, char *dst_ip_addr, uint32_t qp,
                       uint32_t new_qp, uint32_t rkey, uint16_t reg_idx)
{
    // IP address for key
    std::string src_ip_addr_str = src_ip_addr;
    std::string dst_ip_addr_str = dst_ip_addr;
    bfrt::tna_disagg_switch::add_roceReq_rule(
        src_ip_addr_str, dst_ip_addr_str, qp, new_qp, rkey, reg_idx);

    pr_rule("Rule[%u] added [REQ]: src_ip[%s], ip[%s], qp[%u] -> n_qp[%u] rkey[0x%x] \n",
            (unsigned int)reg_idx, src_ip_addr, dst_ip_addr, qp, new_qp, rkey);
}

// RoCE ack
extern "C" void bfrt_add_roce_ack(uint32_t qp, //char *src_ip_addr,
                                  uint32_t new_qp, char *new_ip_addr, uint16_t reg_idx);
void bfrt_add_roce_ack(uint32_t qp, //char *src_ip_addr,
                       uint32_t new_qp, char *new_ip_addr, uint16_t reg_idx)
{
    std::string new_ip_addr_str = new_ip_addr;
    bfrt::tna_disagg_switch::add_roceAck_rule(qp, new_qp, new_ip_addr_str, reg_idx);

    pr_rule("Rule[%u] added [ACK]: qp[%u] -> qp[%u] ip[%s]\n",
            (unsigned int)reg_idx, qp, new_qp, new_ip_addr);
}

// Roce Dummy ack for NACK
extern "C" void bfrt_add_roce_dummy_ack(uint32_t qp, char* ip_addr,
                                        uint32_t new_qp, uint64_t vaddr);
void bfrt_add_roce_dummy_ack(uint32_t qp, char *ip_addr,
                             uint32_t new_qp, uint64_t vaddr)
{
    std::string ip_addr_str = ip_addr;
    bfrt::tna_disagg_switch::add_roceDummyAck_rule(qp, ip_addr_str, new_qp, vaddr);
    pr_rule("Rule[%u] added [Dummy ACK]: qp[%u] ip[%s] -> qp[%u] vaddr[0x%lx]\n",
            qp, qp, ip_addr, new_qp, (unsigned long)vaddr);
}

// Roce Ack Forwarding (for NACK)
extern "C" void bfrt_add_roce_ack_dest(uint32_t dummy_qp, char *ip_addr,
                                       uint32_t dest_qp_id, uint32_t dummy_qp_id);
void bfrt_add_roce_ack_dest(uint32_t dummy_qp, char *ip_addr,
                            uint32_t dest_qp_id, uint32_t dummy_qp_id)
{
    std::string ip_addr_str = ip_addr;
    bfrt::tna_disagg_switch::add_roceAckDest_rule(dummy_qp, ip_addr_str, dest_qp_id, dummy_qp_id);
    pr_rule("Rule[%u] added [ACK Dest]: dummy_qp[%u] ip[%s] -> dest_qp_id[%u] dummy_qp_id[%u]\n",
            dummy_qp, dummy_qp, ip_addr, dest_qp_id, dummy_qp_id);
}

// RoCE request to ack convert
extern "C" void bfrt_add_set_qp_idx(uint32_t qp_id, char *src_ip_addr,
                                    uint16_t global_qp_idx);
void bfrt_add_set_qp_idx(uint32_t qp_id, char *src_ip_addr,
                         uint16_t global_qp_idx)
{
    std::string new_ip_addr_str = src_ip_addr;
    bfrt::tna_disagg_switch::add_setQpIdx_rule(qp_id, new_ip_addr_str, global_qp_idx);
    pr_rule("Rule added [RtoA2]: qp_id[%u] ip[%s] -> qp_idx[%u]\n",
            qp_id, src_ip_addr, global_qp_idx);
}

// RoCE request to ack convert
extern "C" void bfrt_add_sender_qp(uint32_t cpu_qp_id, char *src_ip_addr,
                                   uint16_t mem_qp_id);
void bfrt_add_sender_qp(uint32_t cpu_qp_id, char *src_ip_addr,
                        uint16_t mem_qp_id)
{
    std::string new_ip_addr_str = src_ip_addr;
    bfrt::tna_disagg_switch::add_senderQp_rule(cpu_qp_id, new_ip_addr_str, mem_qp_id);
    pr_rule("Rule added [RtoA]: qp_id[%u] ip[%s] -> qp_id[%u]\n",
            cpu_qp_id, src_ip_addr, mem_qp_id);
}

// Address translation
extern "C" void bfrt_add_addr_trans(uint64_t vaddr, uint16_t vaddr_prefix,
                                    char *dst_ip_addr, uint64_t va_to_dma);

void bfrt_add_addr_trans(uint64_t vaddr, uint16_t vaddr_prefix,
                         char *dst_ip_addr, uint64_t va_to_dma)
{
    std::string ip_addr_str = dst_ip_addr;
    bfrt::tna_disagg_switch::add_addrTrans_rule(vaddr, vaddr_prefix, ip_addr_str, va_to_dma);
    unsigned long num_rule = bfrt::tna_disagg_switch::get_num_addr_trans_rules()->fetch_add(1, std::memory_order_release);

    pr_rule("Rule added [REQ, AtRules: %lu]: pid(%u)+vaddr[0x%lx/%u] -> ip[%s] offset[0x%lx]\n",
            num_rule, (unsigned int)(vaddr >> 48), vaddr & 0xFFFFFFFFFFFF,
            vaddr_prefix, dst_ip_addr, va_to_dma);
    if (num_rule + 1 > max_at_rule_count)
        max_at_rule_count = num_rule + 1;
}

extern "C" void bfrt_del_addrTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix);
void bfrt_del_addrTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix)
{
    unsigned long num_rule = bfrt::tna_disagg_switch::get_num_addr_trans_rules()->fetch_add(-1, std::memory_order_release);

    pr_rule("Rule deleted [REQ, AtTrules %lu]: pid(%u)+vaddr[0x%lx/%u]\n",
            num_rule - 1, (unsigned int)(vaddr >> 48), vaddr & 0xFFFFFFFFFFFF, vaddr_prefix);
    bfrt::tna_disagg_switch::del_addrTrans_rule(vaddr, vaddr_prefix);
    (void)num_rule;
}

// Access control
extern "C" void bfrt_add_addr_except_trans(uint64_t vaddr, uint16_t vaddr_prefix,
                                           char *dst_ip_addr, uint64_t va_to_dma, uint8_t permission,
                                           int account);

void bfrt_add_addr_except_trans(uint64_t vaddr, uint16_t vaddr_prefix,
                                char *dst_ip_addr, uint64_t va_to_dma, uint8_t permission,
                                int account)
{
    std::string ip_addr_str = dst_ip_addr;
    bfrt::tna_disagg_switch::add_modify_addrExceptTrans_rule(
        vaddr, vaddr_prefix, ip_addr_str, va_to_dma, permission, 1);
    if (account)
    {
        unsigned long num_rule = bfrt::tna_disagg_switch::
                                     get_num_except_trans_rules()
                                         ->fetch_add(1, std::memory_order_release);
        unsigned long num_cur_page = pow(2, 64 - vaddr_prefix - 12);
        unsigned long num_page = bfrt::tna_disagg_switch::
                                     get_num_except_trans_pages()
                                         ->fetch_add(num_cur_page, std::memory_order_release);
        unsigned long num_cur_hpage = (unsigned long)(ceil((double)num_cur_page / 512.));
        unsigned long num_hpage = bfrt::tna_disagg_switch::
                                      get_num_except_trans_huge_pages()
                                          ->fetch_add(num_cur_hpage, std::memory_order_release);
        // rules
        if (num_rule + 1 > max_ac_rule_count)
            max_ac_rule_count = num_rule + 1;
        // 4KB page
        if (num_page + num_cur_page > max_ac_page_count)
            max_ac_page_count = num_page + num_cur_page;
        // 2MB page
        if (num_hpage + num_cur_hpage > max_ac_huge_page_count)
            max_ac_huge_page_count = num_hpage + num_cur_hpage;
    }

    pr_rule("Rule added [REQ, ExRules %lu]: pid(%u)+vaddr[0x%lx/%u] -> ip[%s] offset[0x%lx] permission[%u]\n",
            bfrt::tna_disagg_switch::get_num_except_trans_rules()->load(std::memory_order_release),
            (unsigned int)(vaddr >> 48), vaddr & 0xFFFFFFFFFFFF,
            vaddr_prefix, dst_ip_addr, va_to_dma, permission);
}

extern "C" void bfrt_modify_addr_except_trans(uint64_t vaddr, uint16_t vaddr_prefix,
                                              char *dst_ip_addr, uint64_t va_to_dma, uint8_t permission);
void bfrt_modify_addr_except_trans(uint64_t vaddr, uint16_t vaddr_prefix,
                                   char *dst_ip_addr, uint64_t va_to_dma, uint8_t permission)
{
    std::string ip_addr_str = dst_ip_addr;
    bfrt::tna_disagg_switch::add_modify_addrExceptTrans_rule(
        vaddr, vaddr_prefix, ip_addr_str, va_to_dma, permission, 0);

    pr_rule("Rule modified [REQ, Trans %lu]: pid(%u)+vaddr[0x%lx/%u] -> ip[%s] offset[0x%lx] permission[%u]\n",
            bfrt::tna_disagg_switch::get_num_except_trans_rules()->load(std::memory_order_release),
            (unsigned int)(vaddr >> 48), vaddr & 0xFFFFFFFFFFFF,
            vaddr_prefix, dst_ip_addr, va_to_dma, permission);
}

static unsigned int delete_print_counter = 0;
extern "C" void bfrt_del_addrExceptTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix, int account);
void bfrt_del_addrExceptTrans_rule(uint64_t vaddr, uint16_t vaddr_prefix, int account)
{
    unsigned long num_rule;
    (void)num_rule;
    if (account)
    {
        num_rule = bfrt::tna_disagg_switch::
                                    get_num_except_trans_rules()
                                        ->fetch_add(-1, std::memory_order_release);
        unsigned long num_cur_page = pow(2, 64 - vaddr_prefix - 12);
        unsigned long num_page = bfrt::tna_disagg_switch::
                                    get_num_except_trans_pages()
                                        ->fetch_add(-num_cur_page, std::memory_order_release);
        unsigned long num_cur_hpage = (unsigned long)(ceil((double)num_cur_page / 512.));
        unsigned long num_hpage = bfrt::tna_disagg_switch::
                                    get_num_except_trans_huge_pages()
                                        ->fetch_add(-num_cur_hpage, std::memory_order_release);
        (void)num_page;
        (void)num_hpage;
    }
    else
    {
        num_rule = bfrt::tna_disagg_switch::get_num_except_trans_rules()->load(std::memory_order_release);
    }
    delete_print_counter++;
    pr_rule("Rule deleted [REQ, Trans R%lu]: pid(%u)+vaddr[0x%lx/%u] (*not full list)\n",
            num_rule, (unsigned int)(vaddr >> 48), vaddr & 0xFFFFFFFFFFFF, vaddr_prefix);
    bfrt::tna_disagg_switch::del_addrExceptTrans_rule(vaddr, vaddr_prefix);
}

extern "C" void bfrt_add_ack_trans(char *dst_ip_addr, uint32_t qp, uint32_t new_qp,
                                      uint32_t rkey, uint64_t vaddr, uint16_t reg_idx);
void bfrt_add_ack_trans(char *dst_ip_addr, uint32_t qp, uint32_t new_qp,
                           uint32_t rkey, uint64_t vaddr, uint16_t reg_idx)
{
    std::string ip_addr_str = dst_ip_addr;
    bfrt::tna_disagg_switch::add_egressAckTrans_rule(ip_addr_str, qp, new_qp, rkey, vaddr, reg_idx);
    pr_rule("Rule added [ACk2RoCE %d]: QP[%d] Dest IP[%s] -> QP[%d] RegIdx[%d] Rkey[0x%x] BufADDR[0x%lx]\n",
            reg_idx, qp, dst_ip_addr, new_qp, reg_idx, rkey, vaddr);
}

static char file_name[64] = "";
FILE *datetime_fp = NULL;
void set_datetime_filename(void)
{
    // TEMPORARILY DISABLED
    /*
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    sprintf(file_name, "log/%4d%02d%02d_%02d%02d%02d.log",
            tm.tm_year + 1900, tm.tm_mon + 1,
            tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    */
    sprintf(file_name, "log/latest.log");
}

static char *get_datetime_filename(void)
{
    return file_name;
}

FILE *open_datetime_file(void)
{
    if (!datetime_fp)
    {
        datetime_fp = fopen(get_datetime_filename(), "w+");
    }
    return datetime_fp;
}

FILE *get_datetime_filep(void)
{
    return datetime_fp;
}

extern "C" FILE *c_get_datetime_filep(void);
FILE *c_get_datetime_filep(void)
{
    return get_datetime_filep();
}

void close_datetime_file(void)
{
    if (datetime_fp)
    {
        fclose(datetime_fp);
        datetime_fp = NULL;
    }
}

unsigned int bfrt_get_cacheline_count(void);
void print_bfrt_addr_trans_rule_counters(void)
{
    FILE *fp = get_datetime_filep();
    if (!fp)
        return;

    fprintf(fp, "Address translation rules  : max: %lu\n", max_at_rule_count);
    fprintf(fp, "Access control rules       : max: %lu\n", max_ac_rule_count);
    fprintf(fp, "Access control pages (2MB) : max: %lu\n", max_ac_huge_page_count);
    fprintf(fp, "Access control pages (4KB) : max: %lu\n", max_ac_page_count);
    fprintf(fp, "Cache directory            : max: %u\n", bfrt_get_cacheline_count());
    close_datetime_file();  // this function is the last one to use the file
}

extern "C" void bfrt_add_egressInvRoute_rule(int nid, int inv_idx, uint32_t qp, uint32_t rkey, 
                                             uint64_t vaddr, uint16_t reg_idx);
void bfrt_add_egressInvRoute_rule(int nid, int inv_idx, uint32_t qp, uint32_t rkey, 
                                        uint64_t vaddr, uint16_t reg_idx)
{
    bfrt::tna_disagg_switch::add_egressInvRoute_rule(nid, inv_idx, qp, rkey, vaddr, reg_idx);
}
