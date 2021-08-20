// ---------------------------------------------------------------------------
// Switch Egress MAU
// ---------------------------------------------------------------------------
control EgressRoute(inout header_t hdr, inout metadata_t eg_md, in egress_intrinsic_metadata_t eg_intr_md) {

    action route(mac_addr_t dest_mac, ipv4_addr_t dest_ip) {
        hdr.ipv4.dst_addr = dest_ip;
        hdr.ethernet.dst_addr = dest_mac;
    }

    table ipRoute {
        key = {
            eg_intr_md.egress_port: exact;
        }
        actions = {
            route;
        }
        size = 17;  // 16 memory + 1 controller
    }

    // This is only for request toward memory
    action updateReqSrc(ipv4_addr_t src_ip) {
        hdr.ipv4.src_addr = src_ip;
    }

    action updateReqUnMatch()
    {
        eg_md.enable_mirror = 1w1; // Debugging port
    }

    table reqSrcRoute {
        key = {
            hdr.ipv4.dst_addr: exact;
        }
        actions = {
            updateReqSrc;
            updateReqUnMatch;
        }
        const default_action = updateReqUnMatch();
        size = 16;  // 16 memory
    }

    apply {
        ipRoute.apply();
        reqSrcRoute.apply();
    }
}

control SwitchEgress(
        inout header_t hdr,
        inout metadata_t eg_md,
        in    egress_intrinsic_metadata_t                 eg_intr_md,
        in    egress_intrinsic_metadata_from_parser_t     eg_prsr_md,
        inout egress_intrinsic_metadata_for_deparser_t    eg_dprsr_md,
        inout egress_intrinsic_metadata_for_output_port_t eg_oport_md) {
    
    EgressRoute() eroute;

    action nop() {}

    action prepare_invalidation_data() {
        hdr.inval_roce.setValid();
        // hdr.inval_roce.preamble = 0xffff;
        hdr.inval_roce.pkey = hdr.roce.pkey;
        hdr.inval_roce.qp = hdr.roce.qp;
        hdr.inval_roce.psn = hdr.roce.psn;
        hdr.inval_roce.vaddr = hdr.roce_r.vaddr;
        hdr.inval_roce.rkey = hdr.roce_r.rkey;
        hdr.inval_roce.src_addr = hdr.ipv4.src_addr;
        // Data added by this routine
        hdr.udp.hdr_length = hdr.udp.hdr_length + (bit<16>)LEN_INV_OVER_ROCE;
        hdr.ipv4.total_len = hdr.ipv4.total_len + (bit<16>)LEN_INV_OVER_ROCE;
    }

    // Register for invalidation queue selection
    Register<bit<8>, bit<32>>(256, 8w0) inval_roce_qp_sel;
    RegisterAction<bit<8>, bit<32>, bit<8>>(inval_roce_qp_sel) inval_roce_qp_sel_action = {
        void apply(inout bit<8> value, out bit<8> out_val){
                out_val = value;
                if (value < inval_queue_num)
                    value = value + 1;  // 0 ~ N-1 -[+1]-> 1 ~ N
                else
                    value = 0;  // N -> 0
        }
    };

    action set_inval_qp_select()
    {
        eg_md.node_id = inval_roce_qp_sel_action.execute((bit<32>)eg_intr_md.egress_port);
    }

    // Register for invalidation QP
    Register<bit<32>, bit<16>>(256, 32w0) inval_roce_msg_seq;  // <value type, idx type> (size, init val)
    RegisterAction<bit<32>, bit<16>, bit<32>>(inval_roce_msg_seq) inval_roce_mseq_action = {
        void apply(inout bit<32> value, out bit<32> out_val){
                out_val = value;
                value = value + 1;
        }
    };

    action invalidationIpRoute(mac_addr_t dest_mac, ipv4_addr_t dest_ip, 
                               roce_qp_t dest_qp, roce_rkey_t dest_rkey,
                               roce_addr_t inv_vaddr, bit<16> reg_idx) {
        // IP and MAC
        hdr.ethernet.dst_addr = dest_mac;
        hdr.ipv4.dst_addr = dest_ip;
        hdr.ipv4.src_addr = CTRL_ADDR;
        // RoCE header
        hdr.roce.opcode = ROCE_WRITE_REQ;
        hdr.roce.pkey = 0xffff;
        hdr.roce.reserved = 8w0;
        hdr.roce.qp = dest_qp;
        hdr.roce.ack_req = 1w0;
        // RoCE write request header
        hdr.roce_r.rkey = dest_rkey;        
        hdr.roce_r.vaddr = inv_vaddr;
        hdr.roce_r.length = LEN_INV_OVER_ROCE;
        // Register index
        eg_md.cacheline_idx = (bit<32>)reg_idx;
    }

    table translateInvToRoce {
        key = {
            eg_intr_md.egress_port: exact;
            eg_md.node_id: exact;          // Selected invalidation queue
        }
        actions = {
            invalidationIpRoute;
        }
        size = 256;  // enough for 8 * 16 CPU + 1 controller
    }

    // Get current destination id as a sharer list
    action get_dst_node_as_sharer(bit<16> cur_node)
    {
        eg_md.sharer_list = cur_node;
    }

    table getDestNodeMask{
        key = {
            hdr.ipv4.dst_addr: exact;
        }

        actions = {
            get_dst_node_as_sharer;
            nop;
        }

        const default_action = nop();
        size = 16;   // number of state
    }

    // Register for head of circular queue
    Register<bit<32>, bit<16>>(512, 32w0) inval_circ_queue_head;  // <value type, idx type> (size, init val)
    RegisterAction<bit<32>, bit<16>, bit<32>>(inval_circ_queue_head) inval_cq_action = {
        void apply(inout bit<32> value, out bit<32> out_val){
                out_val = value;
                if (value < RDMA_RECV_BUF_SIZE)
                    value = value + 1;
                else
                    value = 0;
        }
    };

    action set_circ_queue_inval_roce()
    {
        eg_md.caddr = (bit<64>)inval_cq_action.execute((bit<16>)eg_md.cacheline_idx);
    }

    action set_inval_psn_roce()
    {
        hdr.roce.psn = inval_roce_mseq_action.execute((bit<16>)eg_md.cacheline_idx)[23:0];
        // hdr.roce.psn = 0;   // in case of RDMA UC
    }

    // UDP ACK to ROCE write req conversion
    // - Fill the data field required for ACK
    action set_up_ack_data()
    {
        hdr.ack_roce.setValid();
        hdr.ack_roce.preamble = 0xffff;
        hdr.ack_roce.vaddr = hdr.roce_r.vaddr;
        hdr.ack_roce.rkey = hdr.roce_r.rkey;
        hdr.udp.hdr_length = hdr.udp.hdr_length + (bit<16>)LEN_ACK_OVER_ROCE;
        hdr.ipv4.total_len = hdr.ipv4.total_len + (bit<16>)LEN_ACK_OVER_ROCE;
    }

    // - Convert it as RoCE/RDAM write
    Register<bit<32>, bit<16>>(512, 32w0) ack_roce_msg_seq;  // <value type, idx type> (size, init val)
    RegisterAction<bit<32>, bit<16>, bit<32>>(ack_roce_msg_seq) ack_roce_mseq_action = {
        void apply(inout bit<32> value, out bit<32> out_val){
                out_val = value;
                value = value + 1;
        }
    };

    action make_ack_as_roce(roce_qp_t dest_qp, roce_rkey_t dest_rkey,
                         roce_addr_t ack_vaddr, bit<16> reg_idx)
    {
        // This is for RDMA UC
        // hdr.roce.opcode = ROCE_UC_WRITE_REQ;
        //
        hdr.roce.opcode = ROCE_WRITE_REQ;
        hdr.roce.qp = dest_qp;
        hdr.roce.psn = ack_roce_mseq_action.execute(reg_idx)[23:0];
        hdr.roce.pkey = 0xffff;
        hdr.roce.reserved = 8w0;
        hdr.roce.ack_req = 1w0;
        //
        hdr.roce_r.rkey = dest_rkey;        
        hdr.roce_r.vaddr = ack_vaddr;
        hdr.roce_r.length = LEN_ACK_OVER_ROCE;
    }

    table translateAckToRoce{
        key = {
            hdr.ipv4.dst_addr: exact;
            hdr.roce.qp: exact;
        }

        actions = {
            make_ack_as_roce;
            nop;
        }
        const default_action = nop();
        size = 768; // 384 = 2 * 16 x 24 cores / node
    }

    //== Main pipieline routine ==//
    apply {
        hdr.bridged_md.setInvalid();
        if (eg_md.do_roce_req == 1w1)
        {
            // All the packet actually have custom header must bypass egress
            if (hdr.udp.dst_port == UDP_RECIRC)
            {
                hdr.udp.dst_port = UDP_ROCE;
            }

            // Do not touch recirculated packets
            if (eg_intr_md.egress_port != DISAGG_RECIRC)
            {
                if (hdr.udp.dst_port == UDP_ACK)
                {   // 1. ACK/NACK
                    hdr.udp.dst_port = UDP_ROCE;
                    set_up_ack_data();
                    translateAckToRoce.apply();
                }
                else if (hdr.roce.reserved == RESERVED_INVALIDATION)
                {   // 2. INVALIDATION
                    // Make header for invalidation (copy from the roce header)
                    if (hdr.inval_roce.isValid())
                    {
                        ;   // Already parsed
                    }else{
                        prepare_invalidation_data();
                    }
                    hdr.inval_roce.preamble = 0xffff;

                    // Current values
                    // - QP[15:0]: invalidation target list as bitmap
                    // - pkey: requester bitmap
                    // Assign dest IP for the invalidation traffics
                    // and set up RoCE header (IP/MAC addresses, QP, rkey, base vaddr, etc.)
                    set_inval_qp_select();
                    translateInvToRoce.apply();
                    
                    // Get bitmask of the destination - IP -> bitmask in eg_md.sharer_list
                    getDestNodeMask.apply();
                    
                    // If it is sending back to the requester, update first 1 of 3 bytes of QP
                    if (hdr.inval_roce.pkey == eg_md.sharer_list)
                    {
                        // Invalidation to the requester, set special flag
                        hdr.inval_roce.qp[16:16] = 1w1;
                        hdr.inval_roce.src_addr = CTRL_ADDR;
                    }
                    
                    // Check qp (=invalidation target) and drop unnecessary packets
                    eg_md.sharer_list = hdr.inval_roce.qp[15:0] & eg_md.sharer_list;
                    if (eg_md.sharer_list == 0)
                    {
                        eg_dprsr_md.drop_ctl = 1;
                    }
                    else
                    {
                        set_inval_psn_roce();           // Set psn
                        set_circ_queue_inval_roce();    // Get header location
                        eg_md.caddr = eg_md.caddr * (bit<64>)LEN_INV_OVER_ROCE; // Calculate offset
                        hdr.roce_r.vaddr = hdr.roce_r.vaddr + eg_md.caddr;      // Add the offset to the base
                    }
                }else{
                    eroute.apply(hdr, eg_md, eg_intr_md);  // update destination info
                }
                if (eg_md.enable_mirror == 1w1)
                {
                    hdr.udp.dst_port = DEBUGGING;
                }
            }else{
            }
        }
    }
}
