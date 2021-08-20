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
        transition parse_ipv4;
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        ig_md.do_roce = (bit<1>)0;
        ig_md.do_roce_req = (bit<1>)0;
        ig_md.do_roce_req_w = (bit<1>)0;
        ig_md.do_roce_ack = (bit<1>)0;
        ig_md.node_id = (bit<8>)0;
        ig_md.vaddr = (bit<48>)0;
        ig_md.pid = (bit<16>)0;
        ig_md.dest_matched = (bit<1>)0;
        ig_md.enable_mirror = (bit<1>)0;
        ig_md.cache_found = (bit<1>)0;
        ig_md.cache_has_state = (bit<1>)0;
        ig_md.reset_sharer = (bit<1>)0;
        ig_md.enable_resubmit = (bit<1>)0;
        ig_md.skip_in_routing = (bit<1>)0;
        ig_md.is_last_ack = (bit<1>)0;
        ig_md.cache_state = (bit<4>)0;
        // ig_md.cache_next_state = (bit<4>)0;
        ig_md.perm = (bit<4>)0;
        ig_md.cacheline_idx = (bit<32>)0;
        ig_md.sharer_list = (bit<16>)0;
        ig_md.qp_idx = (bit<16>)0;
        ig_md.mirror_ses = (MirrorId_t)0;
        ig_md.cache_dir_size = (bit<8>)0;
        ig_md.cache_dir_lock = (bit<8>)0;
        ig_md.qp_dummy_idx = (bit<16>)0;
        ig_md.passthrough_pkt = (bit<4>)0;
        // ig_md.psn_slot_off = (bit<16>)0;
        ig_md.caddr = (bit<64>)0;
        ig_md.orig_psn = (roce_psn_t)0;
        ig_md.pkt_type = (pkt_type_t)0;
        transition select(hdr.ipv4.protocol) {
            IP_PROTOCOLS_TCP : parse_tcp;
            IP_PROTOCOLS_UDP : parse_udp;
            default : accept;
        }
    }

    state parse_udp {
        pkt.extract(hdr.udp);
        hdr.recirc_data.setInvalid();
        transition select(hdr.udp.dst_port) {
            UDP_ROCE: parse_roce;
            UDP_RECIRC: parse_recirc;
            UDP_ROCE_INVAL: parse_roce;
            default: accept;
        }
    }

    state parse_recirc {
        pkt.extract(hdr.recirc_data);
        hdr.recirc_data.setValid();
        ig_md.cache_has_state = (bit<1>)1;
        transition parse_roce;
    }

    state parse_roce {
        pkt.extract(hdr.roce);
        ig_md.do_roce = (bit<1>)1;
        ig_md.orig_psn = hdr.roce.psn;
        ig_md.qp_idx = hdr.roce.qp[15:0];
        transition select(hdr.roce.opcode) {
            ROCE_READ_REQ: parse_roce_req;
            ROCE_READ_RES: parse_roce_res;
            ROCE_WRITE_REQ: parse_roce_req;
            ROCE_WRITE_RES: parse_roce_res;
            default: accept;
        }
    }

    state parse_roce_req {
        pkt.extract(hdr.roce_r);
        ig_md.do_roce_req = (bit<1>)1;
        ig_md.vaddr = hdr.roce_r.vaddr[47:0];
        ig_md.pid = hdr.roce_r.vaddr[63:48];
        ig_md.caddr = hdr.roce_r.vaddr;
        ig_md.perm = hdr.roce_r.rkey[31:28];
        transition select(hdr.roce.opcode) {
            ROCE_READ_REQ: parse_roce_check_inv;
            ROCE_WRITE_REQ: parse_roce_write_req;
            default: accept;
        }
    }

    state parse_roce_write_req {
        ig_md.do_roce_req_w = (bit<1>)1;
        transition select(hdr.roce_r.rkey[31:28]) {
            PERMISSION_EVCIT: parse_roce_check_inv;
            PERMISSION_FIN_ACK: parse_roce_check_inv;
            default: parse_roce_passthrough;
        }
    }

    state parse_roce_passthrough {
        ig_md.passthrough_pkt = (bit<4>)1;
        transition parse_roce_check_inv;
    }

    state parse_roce_check_inv {
        transition select(hdr.roce_r.rkey[31:28]) {
            INVALIDATION_ACK: parse_inv_ack;
            default: accept;
        }
    }

    // ACK to RDMA-based invalidation
    state parse_inv_ack {
        pkt.extract(hdr.inval_roce);
        transition accept;
    }

    state parse_roce_res {
        pkt.extract(hdr.roce_a);
        ig_md.do_roce_ack = (bit<1>)1;
        transition accept;
    }

    state parse_tcp {
        pkt.extract(hdr.tcp);
        transition accept;
    }
}

// ---------------------------------------------------------------------------
// Ingress Deparser
// ---------------------------------------------------------------------------
control SwitchIngressDeparser(
        packet_out pkt,
        inout header_t hdr,
        in metadata_t ig_md,
        in ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md) {

    Checksum() ipv4_checksum;
    Mirror() mirror;

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

        if (ig_md.enable_mirror == 1w1)
        {
            mirror.emit<mirror_h>(ig_md.mirror_ses, {ig_md.pkt_type});
        }
        pkt.emit(hdr);
    }
}

// ---------------------------------------------------------------------------
// Egress parser
// ---------------------------------------------------------------------------
parser SwitchEgressParser(
        packet_in pkt,
        out header_t hdr,
        out metadata_t eg_md,
        out egress_intrinsic_metadata_t eg_intr_md) {

    TofinoEgressParser() tofino_parser;

    state start {
        tofino_parser.apply(pkt, eg_intr_md);
        transition set_initial_context;
    }

    state set_initial_context {
        eg_md.do_roce = (bit<1>)0;
        eg_md.do_roce_req = (bit<1>)0;
        eg_md.do_roce_req_w = (bit<1>)0;
        eg_md.do_roce_ack = (bit<1>)0;
        eg_md.node_id = (bit<8>)0;
        eg_md.vaddr = (bit<48>)0;
        eg_md.pid = (bit<16>)0;
        eg_md.dest_matched = (bit<1>)0;
        eg_md.enable_mirror = (bit<1>)0;
        eg_md.cache_found = (bit<1>)0;
        eg_md.cache_has_state = (bit<1>)0;
        eg_md.reset_sharer = (bit<1>)0;
        eg_md.enable_resubmit = (bit<1>)0;
        eg_md.skip_in_routing = (bit<1>)0;
        eg_md.is_last_ack = (bit<1>)0;
        eg_md.cache_state = (bit<4>)0;
        eg_md.perm = (bit<4>)0;
        eg_md.cacheline_idx = (bit<32>)0;
        eg_md.sharer_list = (bit<16>)0;
        eg_md.qp_idx = (bit<16>)0;
        eg_md.mirror_ses = (MirrorId_t)0;
        eg_md.cache_dir_size = (bit<8>)0;
        eg_md.cache_dir_lock = (bit<8>)0;
        eg_md.qp_dummy_idx = (bit<16>)0;
        eg_md.passthrough_pkt = (bit<4>)0;
        eg_md.caddr = (bit<64>)0;
        eg_md.orig_psn = (roce_psn_t)0;
        eg_md.pkt_type = (pkt_type_t)0;
        transition parse_metadata;
    }

    state parse_metadata {
        mirror_h mirror_md = pkt.lookahead<mirror_h>();
        transition select(mirror_md.pkt_type) {
            PKT_TYPE_MIRROR : parse_mirror_md;
            PKT_TYPE_NORMAL : parse_bridged_md;
            default : accept;
        }
    }

    state parse_bridged_md {
        pkt.extract(hdr.bridged_md);
        eg_md.pkt_type = PKT_TYPE_NORMAL;
        transition parse_ethernet;
    }

    state parse_mirror_md {
        mirror_h mirror_md;
        pkt.extract(mirror_md);
        eg_md.pkt_type = mirror_md.pkt_type;
        mirror_md.setInvalid();
        transition parse_ethernet;
    }

    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition parse_ipv4;
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition select(hdr.ipv4.protocol) {
            IP_PROTOCOLS_UDP : parse_udp;
            default : accept;
        }
    }

    state parse_udp {
        pkt.extract(hdr.udp);
        transition select(hdr.udp.dst_port) {
            UDP_ROCE: parse_roce;
            UDP_RECIRC: parse_recirc;   // recirculation with custom header
            UDP_ACK: parse_roce;
            default: accept;
        }
    }

    state parse_recirc {
        recirc_h recirc_data;
        pkt.extract(recirc_data);
        recirc_data.setInvalid();
        transition parse_roce;
    }

    state parse_roce {
        pkt.extract(hdr.roce);
        eg_md.do_roce = (bit<1>)1;
        transition select(hdr.roce.opcode) {
            ROCE_READ_REQ: parse_roce_req;
            ROCE_WRITE_REQ: parse_roce_req;
            default: accept;
        }
    }

    state parse_roce_req {
        pkt.extract(hdr.roce_r);
        eg_md.do_roce_req = (bit<1>)1;
        transition select(hdr.roce_r.rkey[31:28]) {
            INVALIDATION_ACK: parse_inv_ack;
            default: accept;
        }
    }

    state parse_inv_ack {
        pkt.extract(hdr.inval_roce);
        transition accept;
    }
}

// ---------------------------------------------------------------------------
// Ingress Deparser
// ---------------------------------------------------------------------------
control SwitchEgressDeparser(
        packet_out pkt,
        inout header_t hdr,
        in metadata_t eg_md,
        in egress_intrinsic_metadata_for_deparser_t eg_dprsr_md) {

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
        
        // Emit the parsed headers
        pkt.emit(hdr.ethernet); 
        pkt.emit(hdr.ipv4);
        pkt.emit(hdr.udp);
        pkt.emit(hdr.roce);
        pkt.emit(hdr.roce_r);
        pkt.emit(hdr.ack_roce);
        pkt.emit(hdr.inval_roce);
    }
}
