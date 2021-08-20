
// ORIGINAL COPYRIGHT FROM BAREFOOT FOR EXAMPLE TEMPLATE //
/*******************************************************************************
 * BAREFOOT NETWORKS CONFIDENTIAL & PROPRIETARY
 *
 * Copyright (c) 2019-present Barefoot Networks, Inc.
 *
 * All Rights Reserved.
 *
 * NOTICE: All information contained herein is, and remains the property of
 * Barefoot Networks, Inc. and its suppliers, if any. The intellectual and
 * technical concepts contained herein are proprietary to Barefoot Networks, Inc.
 * and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 * process, and are protected by trade secret or copyright law.  Dissemination of
 * this information or reproduction of this material is strictly forbidden unless
 * prior written permission is obtained from Barefoot Networks, Inc.
 *
 * No warranty, explicit or implicit is provided, unless granted under a written
 * agreement with Barefoot Networks, Inc.
 *
 ******************************************************************************/

#ifndef _HEADERS_
#define _HEADERS_

#define MAX_CACHE_DIR_ENTRY 30624

typedef bit<48> mac_addr_t;
typedef bit<32> ipv4_addr_t;
typedef bit<128> ipv6_addr_t;
typedef bit<12> vlan_id_t;

const ipv4_addr_t CTRL_ADDR = 0x0a0a0a01;
const bit<9> DISAGG_RECIRC = 68;     // Default circulartion port
const bit<9> DISAGG_TOCTRL = 192;    // Default cpu port
const bit<8> inval_queue_num = 15;

typedef bit<16> ether_type_t;
const ether_type_t ETHERTYPE_IPV4 = 16w0x0800;
const ether_type_t ETHERTYPE_ARP = 16w0x0806;
const ether_type_t ETHERTYPE_IPV6 = 16w0x86dd;
const ether_type_t ETHERTYPE_VLAN = 16w0x8100;

typedef bit<8> ip_protocol_t;
const ip_protocol_t IP_PROTOCOLS_ICMP = 1;
const ip_protocol_t IP_PROTOCOLS_TCP = 6;
const ip_protocol_t IP_PROTOCOLS_UDP = 17;

typedef bit<16> udp_port_t;
// const udp_port_t UDP_ROCE = 65001;  //DUMMY;
const udp_port_t UDP_ROCE = 4791;
const udp_port_t DEBUGGING = 40002;
const udp_port_t UDP_RECIRC = 40003;
const udp_port_t UDP_ACK = 40004;
const udp_port_t UDP_ROCE_INVAL = 40005;

// RoCE related length
const bit<16> ip_roce_ack_page_size = 4144;
const bit<16> udp_roce_ack_page_size = 4124;
const bit<16> ip_roce_ack_size = 48;
const bit<16> udp_roce_ack_size = 28;

// RoCE Opcodes
typedef bit<8> roce_opcode_t;
const roce_opcode_t ROCE_READ_REQ = 12;
const roce_opcode_t ROCE_READ_RES = 16;
const roce_opcode_t ROCE_WRITE_REQ = 10;
const roce_opcode_t ROCE_WRITE_RES = 17;
const roce_opcode_t ROCE_UC_WRITE_REQ = 42;

typedef bit<24> roce_qp_t;
typedef bit<24> roce_psn_t;
typedef bit<32> roce_rkey_t;
typedef bit<24> roce_mseq_t;
typedef bit<64> roce_addr_t;

const bit<16> TEST_APP_PID = 0x1000;

header ethernet_h {
    mac_addr_t dst_addr;
    mac_addr_t src_addr;
    bit<16> ether_type;
}

header vlan_tag_h {
    bit<3> pcp;
    bit<1> cfi;
    vlan_id_t vid;
    bit<16> ether_type;
}

header mpls_h {
    bit<20> label;
    bit<3> exp;
    bit<1> bos;
    bit<8> ttl;
}

header ipv4_h {
    bit<4> version;
    bit<4> ihl;
    bit<8> diffserv;
    bit<16> total_len;
    bit<16> identification;
    bit<3> flags;
    bit<13> frag_offset;
    bit<8> ttl;
    bit<8> protocol;
    bit<16> hdr_checksum;
    ipv4_addr_t src_addr;
    ipv4_addr_t dst_addr;
}

header ipv6_h {
    bit<4> version;
    bit<8> traffic_class;
    bit<20> flow_label;
    bit<16> payload_len;
    bit<8> next_hdr;
    bit<8> hop_limit;
    ipv6_addr_t src_addr;
    ipv6_addr_t dst_addr;
}

header tcp_h {
    bit<16> src_port;
    bit<16> dst_port;
    bit<32> seq_no;
    bit<32> ack_no;
    bit<4> data_offset;
    bit<4> res;
    bit<8> flags;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgent_ptr;
}

header udp_h {
    udp_port_t src_port;
    udp_port_t dst_port;
    bit<16> hdr_length;
    bit<16> checksum;
}

header icmp_h {
    bit<8> type_;
    bit<8> code;
    bit<16> hdr_checksum;
}

// RoCE
header roce_h {
    bit<8> opcode;
    bit<8> event_and_hdr;
    bit<16> pkey;
    bit<8> reserved;        // 8 bits of reserved zeros 
    roce_qp_t qp;           // 24 bits of queue pair
    bit<1> ack_req;
    bit<7> reserved2;
    roce_psn_t psn;         // packet sequence number
}
const bit<8> RESERVED_INVALIDATION = 0x1;

// RoCE (if opcode=10 or 12, RDMA extended transport header)
header roce_reth_h {
    roce_addr_t vaddr;
    roce_rkey_t rkey;
    bit<32> length;
}

header dummy_ack_over_roce_h {
    bit<16> preamble;    // will be 0xff
    roce_addr_t vaddr;
    roce_rkey_t rkey;
    bit<16> _pad;
}
const bit<32> LEN_ACK_OVER_ROCE = 16;

header dummy_inval_over_roce_h {
    bit<16> preamble;
    bit<16> pkey;
    bit<8> reserved;
    roce_qp_t qp;
    bit<8> reserved2;
    roce_psn_t psn;
    roce_addr_t vaddr;
    roce_rkey_t rkey;
    ipv4_addr_t src_addr;   // sender or requester
    bit<32> reserved3;      // padding
}
const bit<32> LEN_INV_OVER_ROCE = 32;

// RoCE (if opcode=16 or 17, ACK extended transport header)
header roce_aeth_h {
    bit<8> syndrome;
    roce_mseq_t mseq_no;    // message sequence number (start from 0 at the first tx in the queue)
}

// Address Resolution Protocol -- RFC 6747
header arp_h {
    bit<16> hw_type;
    bit<16> proto_type;
    bit<8> hw_addr_len;
    bit<8> proto_addr_len;
    bit<16> opcode;
    // ...
}

// Segment Routing Extension (SRH) -- IETFv7
header ipv6_srh_h {
    bit<8> next_hdr;
    bit<8> hdr_ext_len;
    bit<8> routing_type;
    bit<8> seg_left;
    bit<8> last_entry;
    bit<8> flags;
    bit<16> tag;
}

// VXLAN -- RFC 7348
header vxlan_h {
    bit<8> flags;
    bit<24> reserved;
    bit<24> vni;
    bit<8> reserved2;
}

// Generic Routing Encapsulation (GRE) -- RFC 1701
header gre_h {
    bit<1> C;
    bit<1> R;
    bit<1> K;
    bit<1> S;
    bit<1> s;
    bit<3> recurse;
    bit<5> flags;
    bit<3> version;
    bit<16> proto;
}

// Struct for mirroring packet
const MirrorId_t mirror_sess_to_ctrl = 1;
const MirrorId_t mirror_sess_recirc = 3;
#if __TARGET_TOFINO__ == 1
typedef bit<3> mirror_type_t;
#else
typedef bit<4> mirror_type_t;
#endif
const mirror_type_t MIRROR_TYPE_I2E = 1;
const mirror_type_t MIRROR_TYPE_E2E = 2;
typedef bit<8>  pkt_type_t;
const pkt_type_t PKT_TYPE_NORMAL = 1;
const pkt_type_t PKT_TYPE_MIRROR = 2;

header mirror_h {
  pkt_type_t  pkt_type;
}

header mirror_bridged_metadata_h {
    pkt_type_t pkt_type;
}

struct cached_t {   // Cache directory register
    bit<8> state;
    bit<8> on_update;
}
const bit<4> STATE_SUCCESS = 0xf;
const bit<4> STATE_EMPTY = 0xe;
const bit<4> STATE_FAIL = 0x0;
const bit<4> STATE_SHA = 6;
const bit<4> STATE_SHA_DATA = 2;
const bit<4> STATE_MOD = 7;
const bit<4> STATE_MOD_DATA = 9;
const bit<4> STATE_IDLE = 8;
// Locked states : **NOT USED**
const bit<4> STATE_SHA_LOCK = 0xb;
const bit<4> STATE_MOD_LOCK = 0xc;

header recirc_h {
    bit<4> cur_state;
    bit<4> next_state;
    bit<16> next_sharer;
    bit<16> invalidation_mask;
    bit<2> code;
    bit<2> reset_sharer;
    bit<1> verify_and_skip;
    bit<3> _pad1;     // pad
    bit<8> send_inval;
    bit<8> dir_size;
}

const bit<2> RECIRC_FIRST_PASS = 0x0;
const bit<2> RECIRC_CREATE_NACK = 0x1;
const bit<2> RECIRC_RESET = 0x3;
const bit<2> CACHE_ADD_SHARER = 0x0;
const bit<2> CACHE_RESET_SHARER = 0x1;
const bit<2> CACHE_DEL_SHARER = 0x2;
const bit<2> CACHE_SAME_SHARER = 0x3;
const bit<4> CACHE_NO_INVAL = 0x0;
const bit<8> CACHE_NO_INVAL_FULL = 0x0;
const bit<4> CACHE_SINGLE_INVAL = 0x1;
const bit<4> CACHE_MULTI_INVAL = 0x2;
const bit<4> CACHE_FORWARD_DATA_CPU = 0x3; // Forward request to the sharer as a response
const bit<4> CACHE_FORWARD_DATA_MEM = 0x4; // Forward request to the sharer as a response
const bit<4> CACHE_RETURN_WRITE_ACK = 0x5;
const bit<4> CACHE_RETURN_READ_NACK = 0x6;
//
const bit<2> CACHE_NEED_ACK = 0x0;
const bit<2> CACHE_NEED_NACK = 0x1; // First digit of 0x4
const bit<2> CACHE_NO_ACK = 0x2;    // First two digits of 0x8
const bit<2> CACHE_SKIP_ACK = 0x3;  // First two digits of 0xC
const bit<2> CACHE_DROP_MEM = 0x1;
const bit<4> PERMISSION_PASSTHROUGH = 0x0;
const bit<4> PERMISSION_READ = 0x1;
const bit<4> PERMISSION_WRITE = 0x2;
const bit<4> PERMISSION_WRITE_REQ = 0x3;
const bit<4> PERMISSION_RESET_REQ = 0x4;
const bit<4> PERMISSION_FIN_ACK = 0x3;
const bit<4> PERMISSION_EVCIT = 0x4;
const bit<4> CREATE_NACK = 0x6;
const bit<4> INVALIDATION_ACK = 0x7; // Rerouting invalidation ACK

const bit<16> CACHE_DIR_LOCK_MASK = 0xff00;
const bit<16> CACHE_DIR_SIZE_MASK = 0x00ff;

const bit<32> RDMA_RECV_BUF_SIZE = 131071;   // MAX_SIZE - 1 (=allowed index)

// Invalidation related
const bit<32> INVALIDATION_FLAG = 0x100;
const bit<4> INVALIDATION_SHARED = 0x3;
const bit<4> INVALIDATION_MODIFIED = 0x5;
const bit<32> INVALIDATION_DATA_REQ = 0x800;

struct metadata_t {
    // Parsed states
    bit<1> do_roce;
    bit<1> do_roce_req;
    bit<1> do_roce_req_w;
    bit<1> do_roce_ack;
    // Current state values
    bit<8> node_id;
    bit<48> vaddr;
    bit<16> pid;
    bit<1> dest_matched;
    bit<1> enable_mirror;
    bit<1> cache_found;
    bit<1> cache_has_state; // is submitted
    // Control flags
    bit<1> reset_sharer;
    bit<1> enable_resubmit;
    bit<1> skip_in_routing;
    bit<1> is_last_ack;
    bit<4> cache_state;
    bit<4> perm;            // 0x1 for read (shared), 0x10 for write (modified)
    bit<32> cacheline_idx;
    bit<16> sharer_list;
    bit<16> qp_idx;         //index to retrieve QP of CPU, indexed by same cacheline_idx
    MirrorId_t mirror_ses;  // Ingress mirror session ID
    bit<8> cache_dir_size;
    bit<8> cache_dir_lock;
    bit<16> qp_dummy_idx;
    bit<4> passthrough_pkt; // For fin ACK and evict
    bit<64> caddr;
    roce_psn_t orig_psn;
    pkt_type_t pkt_type;
}

struct header_t {
    mirror_bridged_metadata_h bridged_md;
    ethernet_h ethernet;
    ipv4_h ipv4;
    ipv6_h ipv6;
    tcp_h tcp;
    udp_h udp;
    recirc_h recirc_data;
    roce_h roce;
    roce_reth_h roce_r;
    roce_aeth_h roce_a;
    dummy_ack_over_roce_h ack_roce;
    dummy_inval_over_roce_h inval_roce;
}

struct empty_header_t {}

struct empty_metadata_t {}

#endif /* _HEADERS_ */
