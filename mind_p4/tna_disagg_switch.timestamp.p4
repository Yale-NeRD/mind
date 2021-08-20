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

#include <core.p4>
#if __TARGET_TOFINO__ == 2
#include <t2na.p4>
#else
#include <tna.p4>
#endif

#include "common/headers.p4"
#include "common/util.p4"


#if __TARGET_TOFINO__ == 2
header tna_timestamps_h {
    bit<16> pad_1;
    bit<48> ingress_mac;
    bit<16> pad_2;
    bit<48> ingress_global;
    bit<32> enqueue;
    bit<32> dequeue_delta;
    bit<16> pad_5;
    bit<48> egress_global;
    bit<16> pad_6;
    bit<48> egress_tx;
}
#else
header tna_timestamps_h {
    bit<16> pad_1;
    bit<48> ingress_mac;
    bit<16> pad_2;
    bit<48> ingress_global;
    bit<14> pad_3;
    bit<18> enqueue;
    bit<14> pad_4;
    bit<18> dequeue_delta;
    bit<16> pad_5;
    bit<48> egress_global;
    bit<16> pad_6;
    bit<48> egress_tx;
}
#endif

struct metadata_t {
    tna_timestamps_h tna_timestamps_hdr;
    ptp_metadata_t tx_ptp_md_hdr;
}

// ---------------------------------------------------------------------------
// Ingress parser
// ---------------------------------------------------------------------------
parser SwitchIngressParser(
        packet_in pkt,
        out header_t hdr,
        out metadata_t ig_md,
        out ingress_intrinsic_metadata_t ig_intr_md) {

    TofinoIngressParser() tofino_parser;

    state start {
        tofino_parser.apply(pkt, ig_intr_md);
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            ETHERTYPE_IPV4 : parse_ipv4;
            default : reject;
        }
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            IP_PROTOCOLS_UDP : parse_udp;
            IP_PROTOCOLS_TCP : parse_tcp;
            default : accept;
        }
    }

    state parse_tcp {
        pkt.extract(hdr.tcp);
        transition accept;
    }

    state parse_udp {
        pkt.extract(hdr.udp);
        transition parse_tna_timestamp;
    }

    state parse_tna_timestamp {
        pkt.extract(ig_md.tna_timestamps_hdr);
        transition accept;
    }
}


// ---------------------------------------------------------------------------
// Ingress
// ---------------------------------------------------------------------------
control SwitchIngress(
        inout header_t hdr,
        inout metadata_t ig_md,
        in ingress_intrinsic_metadata_t ig_intr_md,
        in ingress_intrinsic_metadata_from_parser_t ig_intr_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md,
        inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md) {

    action set_ts_ingress() {
        ig_md.tna_timestamps_hdr.ingress_mac = ig_intr_md.ingress_mac_tstamp;
        ig_md.tna_timestamps_hdr.ingress_global = ig_intr_prsr_md.global_tstamp;
    }

    action set_ts_ingress_dummy1() {
        ig_md.tna_timestamps_hdr.ingress_mac = ig_intr_md.ingress_mac_tstamp;
        ig_md.tna_timestamps_hdr.ingress_global = ig_intr_prsr_md.global_tstamp;
        ig_intr_dprsr_md.drop_ctl = 1;
    }

    action set_ts_ingress_dummy2() {
        ig_md.tna_timestamps_hdr.ingress_mac = ig_intr_md.ingress_mac_tstamp;
        ig_md.tna_timestamps_hdr.ingress_global = ig_intr_prsr_md.global_tstamp;
        ig_intr_dprsr_md.drop_ctl = 0;
    }

    action route(mac_addr_t dstMac, PortId_t dst_port) {
        ig_intr_tm_md.ucast_egress_port = dst_port;
        hdr.ethernet.dst_addr = dstMac;
        // hdr.ethernet.src_addr = srcMac;
        // cntr.count();
        // color = (bit<2>) meter.execute();
        ig_intr_dprsr_md.drop_ctl = 0;
    }

    action nop() {}

    table ip_route {
        key = {
            hdr.ipv4.dst_addr : exact;
        }
        actions = {
            route;
            nop;
        }
        size = 32768;
    }

    // ===== DUMMYS ===== //

    // table skip_tcp {
    //     key = {
    //         hdr.ipv4.protocol: exact;
    //         ig_intr_md.ingress_port  : lpm;
    //     }
    //     actions = {
    //         nop;
    //         set_ts_ingress;
    //         set_ts_ingress_dummy1;
    //     }
    //     const default_action = nop();
    //     size = 2048;
    // }

    // table skip_tcp2 {
    //     key = {
    //         hdr.ipv4.protocol: exact;
    //         ig_intr_md.ingress_port  : lpm;
    //         hdr.ipv4.src_addr: exact;
    //     }
    //     actions = {
    //         nop;
    //         set_ts_ingress;
    //         set_ts_ingress_dummy1;
    //     }
    //     const default_action = nop();
    //     size = 8192;
    // }

    // table skip_tcp3 {
    //     key = {
    //         hdr.ipv4.protocol: exact;
    //         ig_intr_md.ingress_port  : lpm;
    //         hdr.ipv4.src_addr: exact;
    //     }
    //     actions = {
    //         nop;
    //         set_ts_ingress;
    //         set_ts_ingress_dummy1;
    //     }
    //     const default_action = nop();
    //     size = 8192;
    // }

    // table skip_tcp4 {
    //     key = {
    //         hdr.ipv4.protocol: exact;
    //         ig_intr_md.ingress_port  : lpm;
    //         hdr.ipv4.src_addr: exact;
    //     }
    //     actions = {
    //         nop;
    //         set_ts_ingress;
    //         set_ts_ingress_dummy1;
    //     }
    //     const default_action = nop();
    //     size = 8192;
    // }

    // table skip_tcp5 {
    //     key = {
    //         hdr.ipv4.protocol: exact;
    //         ig_intr_md.ingress_port  : lpm;
    //         hdr.ipv4.src_addr: exact;
    //     }
    //     actions = {
    //         nop;
    //         set_ts_ingress;
    //         set_ts_ingress_dummy1;
    //     }
    //     const default_action = nop();
    //     size = 8192;
    // }

    // table skip_udp {
    //     key = {
    //         hdr.ipv4.protocol: exact;
    //         ig_intr_md.ingress_port  : lpm;
    //         hdr.ipv4.src_addr: exact;
    //     }
    //     actions = {
    //         nop;
    //         set_ts_ingress;
    //         set_ts_ingress_dummy2;
    //     }
    //     const default_action = nop();
    //     size = 16384;
    // }

    // ===== End of DUMMYS ===== //

    apply {
        ip_route.apply();
        if (hdr.ipv4.protocol == IP_PROTOCOLS_TCP) {
            // update_tcp_checksum();
            // skip_tcp.apply();
            // if (ig_intr_md.ingress_port == 9w30)
            // {
            //     skip_tcp2.apply();
            //     if (ig_intr_dprsr_md.drop_ctl == 1) {
            //         skip_tcp3.apply();
            //     }

            //     if (ig_intr_dprsr_md.drop_ctl == 1) {
            //         skip_tcp4.apply();
            //     }
            // }

            // if (ig_intr_dprsr_md.drop_ctl == 0)
            // {
            //     skip_tcp5.apply();
            // }

            ig_intr_tm_md.bypass_egress = 1w1;
        }else{
            if (hdr.ipv4.protocol == IP_PROTOCOLS_UDP) {
                // skip_udp.apply();
                set_ts_ingress();
            }
            // else{
            //     ig_intr_tm_md.bypass_egress = 1w0;
            // }
        }
    }
}

// ---------------------------------------------------------------------------
// Ingress Deparser
// ---------------------------------------------------------------------------
control SwitchIngressDeparser(
        packet_out pkt,
        inout header_t hdr,
        in metadata_t ig_md,
        in ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {

    Checksum() ipv4_checksum;

    apply {
        hdr.ipv4.hdr_checksum = ipv4_checksum.update({
            hdr.ipv4.version,
            hdr.ipv4.ihl,
            hdr.ipv4.diffserv,
            hdr.ipv4.total_len,
            hdr.ipv4.identification,
            hdr.ipv4.flags,
            hdr.ipv4.frag_offset,
            hdr.ipv4.ttl,
            hdr.ipv4.protocol,
            hdr.ipv4.src_addr,
            hdr.ipv4.dst_addr});
        
        pkt.emit(hdr);
        pkt.emit(ig_md.tna_timestamps_hdr);
    }
}

// ---------------------------------------------------------------------------
// Egress Parser
// ---------------------------------------------------------------------------
parser SwitchEgressParser(
        packet_in pkt,
        out header_t hdr,
        out metadata_t eg_md,
        out egress_intrinsic_metadata_t eg_intr_md) {

    TofinoEgressParser() tofino_parser;

    state start {
        tofino_parser.apply(pkt, eg_intr_md);
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select(hdr.ethernet.ether_type) {
            ETHERTYPE_IPV4 : parse_ipv4;
            default : reject;
        }
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            IP_PROTOCOLS_UDP : parse_udp;
            IP_PROTOCOLS_TCP : accept;
            default : accept;
        }
    }

    state parse_udp {
        pkt.extract(hdr.udp);
        transition parse_tna_timestamp;
    }

    state parse_tna_timestamp {
        pkt.extract(eg_md.tna_timestamps_hdr);
        transition accept;
    }
}


// ---------------------------------------------------------------------------
// Egress 
// ---------------------------------------------------------------------------
control SwitchEgress(
        inout header_t hdr,
        inout metadata_t eg_md,
        in egress_intrinsic_metadata_t eg_intr_md,
        in egress_intrinsic_metadata_from_parser_t eg_intr_from_prsr,
        inout egress_intrinsic_metadata_for_deparser_t eg_intr_md_for_dprsr,
        inout egress_intrinsic_metadata_for_output_port_t eg_intr_md_for_oport) {

    apply {
        if (hdr.ipv4.protocol == IP_PROTOCOLS_UDP) {
            eg_md.tna_timestamps_hdr.enqueue = eg_intr_md.enq_tstamp;
            eg_md.tna_timestamps_hdr.dequeue_delta = eg_intr_md.deq_timedelta;
            eg_md.tna_timestamps_hdr.egress_global = eg_intr_from_prsr.global_tstamp;

            // tx timestamping is only available on hardware

            // request tx ptp correction timestamp insertion
            // eg_intr_md_for_oport.update_delay_on_tx = true;

            // Instructions for the ptp correction timestamp writer
            // eg_md.tx_ptp_md_hdr.setValid();
            // eg_md.tx_ptp_md_hdr.cf_byte_offset = 8w76;
            // eg_md.tx_ptp_md_hdr.udp_cksum_byte_offset = 8w34;
            // eg_md.tx_ptp_md_hdr.updated_cf = 0;
            hdr.udp.checksum = 16w0;
        }
    }
}

// ---------------------------------------------------------------------------
// Egress Deparser
// ---------------------------------------------------------------------------
control SwitchEgressDeparser(packet_out pkt,
                              inout header_t hdr,
                              in metadata_t eg_md,
                              in egress_intrinsic_metadata_for_deparser_t 
                                eg_intr_dprsr_md
                              ) {

    apply {
        // tx timestamping is only available on hardware
        // pkt.emit(eg_md.tx_ptp_md_hdr);
        pkt.emit(hdr);  // if no changes in IP header, then no update on checksum
        pkt.emit(eg_md.tna_timestamps_hdr);
    }
}


Pipeline(SwitchIngressParser(),
         SwitchIngress(),
         SwitchIngressDeparser(),
         SwitchEgressParser(),
         SwitchEgress(),
         SwitchEgressDeparser()) pipe;

Switch(pipe) main;
