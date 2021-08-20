
// Function to set up multicast for invalidation messages
control SendInvalidation(inout header_t hdr, 
                         inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {
    apply {
        ig_tm_md.rid = 0xffff;
        ig_tm_md.level2_exclusion_id = 0;
        ig_tm_md.level1_mcast_hash = 1;
        ig_tm_md.level2_mcast_hash = 1;
        ig_tm_md.mcast_grp_a = 0;

        hdr.roce_r.rkey[23:16] = hdr.roce.qp[7:0];
        hdr.roce.reserved = RESERVED_INVALIDATION;      // Invalidation mark for egress
    }
}

// Main ingress pipeline
control SwitchIngress(
        inout header_t hdr,
        inout metadata_t ig_md,
        in ingress_intrinsic_metadata_t ig_intr_md,
        in ingress_intrinsic_metadata_from_parser_t ig_prsr_md,
        inout ingress_intrinsic_metadata_for_deparser_t ig_dprsr_md,
        inout ingress_intrinsic_metadata_for_tm_t ig_tm_md) {
    // General purpose nope/empty action
    action nop() {}

    // 0) Cache coherence
    action cache_found(bit<32> cache_idx){
        ig_md.cache_found = 1w1;
        ig_md.cacheline_idx = cache_idx;
    }

    action cache_not_found()
    {
        ig_md.cache_found = 1w0;
    }

    table findCacheSlot{
        key = {
            hdr.roce_r.vaddr: lpm;
        }

        actions = {
            cache_found;
            cache_not_found;
        }

        const default_action = cache_not_found();
        size = MAX_CACHE_DIR_ENTRY;
    }

    // Cache state and on-going update flag
    Register<cached_t, bit<32>>(MAX_CACHE_DIR_ENTRY) cache_dir_state_reg;
    RegisterAction<cached_t, bit<32>, bit<4>>(cache_dir_state_reg) cache_state_get_action = {
        void apply(inout cached_t value, out bit<4> state){
            if (ig_md.passthrough_pkt == 4w1)
            {
                state = value.state[3:0];
            }
            else 
            if (value.on_update == 0)
            {
                value.on_update = 1;
                state = value.state[3:0];
            }else{
                state = value.state[3:0] & STATE_FAIL;  // becomes 0x0
            }
        }
    };

    action get_current_cache_status()
    {
        hdr.recirc_data.cur_state = cache_state_get_action.execute(ig_md.cacheline_idx);
    }

    RegisterAction<cached_t, bit<32>, bit<4>>(cache_dir_state_reg) cache_state_set_action = {
        void apply(inout cached_t value, out bit<4> state){
            if (ig_md.passthrough_pkt == 4w0)
            {
                value.on_update = 0;
                value.state = (bit<8>)hdr.recirc_data.next_state;
            }
            state = value.state[3:0];
        }
    };

    action set_cache_status()
    {
        hdr.recirc_data.next_state = cache_state_set_action.execute(ig_md.cacheline_idx);
    }

    // Sharer list registers
    Register<bit<16>, bit<32>>(MAX_CACHE_DIR_ENTRY) cache_dir_sharer_reg;
    RegisterAction<bit<16>, bit<32>, bit<16>>(cache_dir_sharer_reg) cache_sharer_get_action = {
        void apply(inout bit<16> value, out bit<16> sharer){
            sharer = value;
        }
    };

    action get_current_cache_sharer()
    {
        ig_md.sharer_list = cache_sharer_get_action.execute(ig_md.cacheline_idx);
    }

    RegisterAction<bit<16>, bit<32>, bit<16>>(cache_dir_sharer_reg) cache_sharer_set_action = {
        void apply(inout bit<16> value, out bit<16> sharer){
            if (ig_md.passthrough_pkt == 4w0)
            {
                value = hdr.recirc_data.next_sharer;
            }
            sharer = value;
        }
    };

    action set_cache_sharer()
    {
        cache_sharer_set_action.execute(ig_md.cacheline_idx);
    }

    // Sender's psn
    Register<bit<32>, bit<16>>(1024) prev_psn_reg;
    RegisterAction<bit<32>, bit<16>, bit<32>>(prev_psn_reg) cpu_blade_psn_get_action = {
        void apply(inout bit<32> value, out bit<32> psn_val){
            psn_val = value;
        }
    };

    RegisterAction<bit<32>, bit<16>, bit<32>>(prev_psn_reg) cpu_blade_psn_set_action = {
        void apply(inout bit<32> value, out bit<32> psn_val){
            value = (bit<32>)ig_md.orig_psn;
            psn_val = value;
        }
    };

    // Invalidation counter
    Register<bit<32>, bit<32>>(MAX_CACHE_DIR_ENTRY) cache_dir_inv_cnt;
    RegisterAction<bit<32>, bit<32>, bit<32>>(cache_dir_inv_cnt) cache_dir_inv_cnt_action = {
        void apply(inout bit<32> value, out bit<32> inv){
            value = value + 1;
            inv = value;
        }
    };

    action inc_inval_counter()
    {
        // Set up QP here
        cache_dir_inv_cnt_action.execute(ig_md.cacheline_idx);
    }

    // Directory size and lock flag: locked (first one byte > 1), size = second byte
    Register<bit<8>, bit<32>>(MAX_CACHE_DIR_ENTRY) cache_dir_lock_reg;
    RegisterAction<bit<8>, bit<32>, bit<8>>(cache_dir_lock_reg) cache_dir_get_lock_action = {
        void apply(inout bit<8> value, out bit<8> dir_lock){
            dir_lock = value;
        }
    };
    action get_cache_dir_lock()
    {
        ig_md.cache_dir_lock = cache_dir_get_lock_action.execute(ig_md.cacheline_idx);
    }

    Register<bit<8>, bit<32>>(MAX_CACHE_DIR_ENTRY) cache_dir_size;
    RegisterAction<bit<8>, bit<32>, bit<8>>(cache_dir_size) cache_dir_get_size_action = {
        void apply(inout bit<8> value, out bit<8> dir_size){
            dir_size = value;
        }
    };
    action get_cache_dir_size()
    {
        hdr.recirc_data.dir_size = cache_dir_get_size_action.execute(ig_md.cacheline_idx);
    }

    // Sharer and state related
    action get_next_state_and_sharers(bit<4> next_state, bit<3> reset_sharer, bit<8> send_inval)
    {
        hdr.recirc_data.next_state = next_state;
        hdr.recirc_data.reset_sharer = reset_sharer[1:0];
        hdr.recirc_data.verify_and_skip = reset_sharer[2:2];
        hdr.recirc_data.send_inval = send_inval;
    }

    action no_state_op()
    {
        hdr.recirc_data.next_state = 0xe;   // empty
        hdr.recirc_data.reset_sharer = 0x3; // same
        hdr.recirc_data.send_inval = 0x10;  // NACK
    }

    table cacheDirectoryPre2{
        key = {
            hdr.recirc_data.cur_state: exact;   // Current state: I, S, S^D, M, M^D, etc.
            ig_md.perm: exact;                  // Request type: R, W, invalidation ACK, FinAck, Data, etc.
            ig_md.do_roce_req_w: exact;
        }

        actions = {
            get_next_state_and_sharers;
            no_state_op;
        }

        const default_action = no_state_op();
        size = 40;
    }

    // Get current node/blade id as a sharer list (=bitmask)
    action get_current_node_as_sharer(bit<16> cur_node)
    {
        hdr.recirc_data.next_sharer = cur_node;
    }

    table getRequesterNode{
        key = {
            hdr.ipv4.src_addr: exact;
        }

        actions = {
            get_current_node_as_sharer;
            nop;
        }

        const default_action = nop();
        size = 16;   // Number of nodes = kind of bitmask with single '1'
    }

    action cache_recirc_with_custom_header()
    {
        ig_tm_md.ucast_egress_port = DISAGG_RECIRC;
        hdr.udp.dst_port = UDP_RECIRC;
        ig_tm_md.bypass_egress = 1w1;
        ig_md.skip_in_routing = 1w1;
        ig_md.enable_mirror = 1w0;
    }

    action cache_recirc_without_header()
    {
        hdr.recirc_data.code = RECIRC_RESET;
        ig_tm_md.ucast_egress_port = DISAGG_RECIRC;
        ig_tm_md.bypass_egress = 1w1;
        ig_md.skip_in_routing = 1w1;
    }

    action cache_recirc_as_roce()
    {
        ig_tm_md.ucast_egress_port = DISAGG_RECIRC;
        hdr.udp.dst_port = UDP_ROCE;
        ig_tm_md.bypass_egress = 1w1;
        ig_md.skip_in_routing = 1w1;
    }

    action cache_recirc()
    {
        // RDMA(=RoCE in MIND) request to the memory
        ig_md.mirror_ses = mirror_sess_recirc;
        ig_md.pkt_type = PKT_TYPE_MIRROR;
        ig_dprsr_md.mirror_type = MIRROR_TYPE_I2E;
        ig_md.enable_mirror = 1w1;
    }

    action cache_send_ctrl()
    {
        // To the controller
        hdr.ipv4.dst_addr = CTRL_ADDR; 
        ig_tm_md.ucast_egress_port = DISAGG_TOCTRL;
        hdr.recirc_data.setInvalid();
        // As we invalidate recirc_data, UDP port must not be UDP_RECIRC
        hdr.udp.dst_port = DEBUGGING;
        hdr.roce_r.rkey[3:0] = hdr.recirc_data.next_state;
    }

    action cache_send_ack()
    {
        // ACK back to the cpu
        hdr.ipv4.dst_addr = hdr.ipv4.src_addr;
        hdr.ipv4.src_addr = CTRL_ADDR;  // look like it sent from ctrl
        hdr.recirc_data.setInvalid();
        hdr.udp.dst_port = UDP_ACK;
        hdr.roce_r.rkey[3:0] = hdr.recirc_data.next_state;
        hdr.roce_r.rkey[7:4] = 0;
    }

    action cache_send_nack()
    {
        // ACK back to the cpu
        hdr.ipv4.dst_addr = hdr.ipv4.src_addr;
        hdr.ipv4.src_addr = CTRL_ADDR;  // Look like it sent from ctrl
        hdr.udp.dst_port = UDP_ACK;
        hdr.roce_r.rkey[4:4] = 1;       // NACK (invalidation without any other flags)
    }

    action cache_send_nack_first_loop()
    {
        hdr.ipv4.dst_addr = hdr.ipv4.src_addr;
        hdr.ipv4.src_addr = CTRL_ADDR;  // Look like it sent from ctrl
        hdr.recirc_data.setInvalid();
        hdr.udp.dst_port = UDP_ACK;
        hdr.roce_r.rkey[4:4] = 1;    
        //
        ig_md.mirror_ses = mirror_sess_recirc;
        ig_dprsr_md.mirror_type = MIRROR_TYPE_I2E;
        ig_md.pkt_type = PKT_TYPE_MIRROR;
        ig_md.enable_mirror = 1w1;
    }

    action cache_no_ack()
    {
        // We will drop the original packet (non-exist egress port)
        ig_tm_md.ucast_egress_port = 9w0x1ff;
        ig_md.skip_in_routing = 1w1;
        hdr.recirc_data.setInvalid();
        hdr.udp.dst_port = UDP_ROCE;    // For multicast packets to be handled inside egress
        hdr.roce_r.rkey[3:0] = hdr.recirc_data.next_state;
    }

    action cache_skip_ack()
    {
        hdr.recirc_data.setInvalid();
    }

    action cache_drop_and_wait()
    {
        // Drop the original packet
        ig_tm_md.ucast_egress_port = 9w0x1ff;
        ig_md.skip_in_routing = 1w1;
        hdr.recirc_data.setInvalid();
    }
    // 1) Virtual memory mapping
    action nop_rst_vaddr()
    {
        hdr.roce_r.vaddr = (bit<64>)ig_md.vaddr;
        ig_md.perm = 4w0;
    }

    // 1.1) Exceptional per process mappings
    action trans_except_addr(ipv4_addr_t dest_ip, roce_addr_t va_base_to_dma, bit<4> permission) {
        hdr.ipv4.dst_addr = dest_ip;
        hdr.roce_r.vaddr = hdr.roce_r.vaddr + va_base_to_dma;
        ig_md.perm = ig_md.perm & permission;
        ig_md.dest_matched = 1w1;
    }

    table addrExceptTrans{
        key = {
            hdr.roce_r.vaddr: lpm;
        }

        actions = {
            trans_except_addr;
            nop_rst_vaddr;
        }

        const default_action = nop_rst_vaddr();
        size = 8192;
    }

    // 1.2) default partition-based mapping
    action trans_addr(ipv4_addr_t dest_ip, roce_addr_t va_base_to_dma) {
        hdr.ipv4.dst_addr = dest_ip;
        hdr.roce_r.vaddr = hdr.roce_r.vaddr + va_base_to_dma;
    }

    table addrTrans{
        key = {
            ig_md.vaddr: lpm;
        }

        actions = {
            trans_addr;
            nop;
        }

        const default_action = nop();
        size = 128;
    }

    // 2) RoCE req and ack
    // 2.1) req
    // Registers for request / ack message sequence number tracking
    // roce_psn_t is 24 bit, but we use 32
    Register<bit<32>, bit<16>>(512, 32w0) req_msg_seq;
    RegisterAction<bit<32>, bit<16>, bit<32>>(req_msg_seq) mem_blade_psn_get_action = {
        void apply(inout bit<32> value, out bit<32> read_mseq){
            read_mseq = value;
        }
    };

    RegisterAction<bit<32>, bit<16>, bit<32>>(req_msg_seq) mem_blade_psn_set_action = {
        void apply(inout bit<32> value){
            if (ig_intr_md.ingress_port != DISAGG_RECIRC)
            value = (bit<32>)hdr.roce_a.mseq_no;    // mseq_no indicates the expected next mseq
        }
    };

    // Action updates metadata for RoCE header
    action roce_req_meta(roce_qp_t dest_qp, roce_rkey_t dest_rkey,
                         ipv4_addr_t src_ip, mac_addr_t src_mac,
                         bit<16> reg_idx) {
        hdr.roce.qp = dest_qp;
        hdr.roce_r.rkey = dest_rkey;
        hdr.ipv4.src_addr = CTRL_ADDR;
        ig_tm_md.bypass_egress = 1w1;
        hdr.ethernet.src_addr = src_mac;    // May not be checked by end host/NIC RDMA logic
        // Next psn
        hdr.roce.psn = mem_blade_psn_get_action.execute(reg_idx)[23:0]; // Can be used for sending request to the cpu
        hdr.roce.pkey = 0xffff;
        hdr.roce.reserved = 8w0;
    }

    table roceReq{
        key = {
            hdr.ipv4.dst_addr : exact;
            hdr.ipv4.src_addr : exact;
            hdr.roce.qp : exact;
        }

        actions = {
            roce_req_meta;
            nop;
        }

        const default_action = nop();
        size = 1024;
    }

    action roce_dummy_ack(roce_qp_t dest_qp, roce_addr_t dummy_va) {
        hdr.roce.qp = dest_qp;  // pgfault qp -> ack qp (+ #cores)
        hdr.roce_r.vaddr = dummy_va;
    }

    table roceDummyAck{
        key = {
            hdr.ipv4.src_addr : exact;
            hdr.roce.qp : exact;
        }
        actions = {
            roce_dummy_ack;
            nop;
        }
        const default_action = nop();
        size = 512;
    }

    action roce_ack_meta(roce_qp_t dest_qp, ipv4_addr_t dest_ip,
                         ipv4_addr_t src_ip, bit<16> reg_idx) {
        hdr.roce.qp = dest_qp;
        hdr.ipv4.dst_addr = dest_ip;
        hdr.ipv4.src_addr = src_ip;
        hdr.roce.pkey = 0xffff;
        ig_tm_md.bypass_egress = 1w1;
    }

    table roceAck{
        key = {
            hdr.roce.qp : exact;
        }

        actions = {
            roce_ack_meta;
            nop;
        }

        const default_action = nop();
        size = 1024;
    }

    action roce_ack_dest_meta(roce_qp_t dest_qp, bit<16> dummy_qp_id) {
        hdr.roce.qp = dest_qp;
        ig_md.qp_idx = (bit<16>)dest_qp;    // Global qp id for page fault QPs
        ig_md.qp_dummy_idx = dummy_qp_id;   // Global qp id for dummy QPs
    }

    action roce_ack_dest_nop() {
        ig_md.qp_dummy_idx = ig_md.qp_idx;
    }

    table roceAckDest{
        key = {
            hdr.ipv4.src_addr : exact;
            hdr.roce.qp : exact;
        }

        actions = {
            roce_ack_dest_meta;
            roce_ack_dest_nop;
        }
        const default_action = roce_ack_dest_nop();
        size = 1024;
    }

    action set_qp_idx(bit<16> qp_idx) {
        cpu_blade_psn_set_action.execute(qp_idx);
    }

    table roceSetQpIdx{
        key = {
            hdr.ipv4.src_addr : exact;
            hdr.roce.qp : exact;
        }

        actions = {
            set_qp_idx;
            nop;
        }
        const default_action = nop();
        size = 1024;
    }


    action get_sender_qp(bit<16> qp_idx) {  // qp_idx <= # of CPU blades * # of QPs/blade
        cpu_blade_psn_set_action.execute(qp_idx);
    }

    action get_sender_dummy_qp() {
        // unused one
    }

    table getSenderQp{
        key = {
            hdr.ipv4.src_addr : exact;
            hdr.roce.qp : exact;
        }

        actions = {
            get_sender_qp;
            get_sender_dummy_qp;
        }

        const default_action = get_sender_dummy_qp();
        size = 1024;
    }

    action convert_datapush_to_ack()
    {
        hdr.roce.opcode = ROCE_READ_RES;
        hdr.roce.reserved = 0;
        hdr.roce.ack_req = 0;
        hdr.roce_a.setValid();
        hdr.roce_r.setInvalid();
        // Adjust size here
        hdr.ipv4.total_len = ip_roce_ack_page_size;
        hdr.udp.hdr_length = udp_roce_ack_page_size;
    }

    action convert_datapush_to_write_ack()
    {
        hdr.roce.reserved = 0;
        hdr.roce.ack_req = 0;
        hdr.roce_r.setInvalid();
        hdr.ipv4.total_len = ip_roce_ack_size;
        hdr.udp.hdr_length = udp_roce_ack_size;
    }

    action convert_req_to_req()
    {
        hdr.ipv4.src_addr = hdr.ipv4.dst_addr;  // The original requester
        hdr.ipv4.dst_addr = CTRL_ADDR;          // Make it looking like from ctrl
        hdr.roce.opcode = ROCE_READ_REQ;
    }

    action get_mem_to_cpu_qp_idx_sharer()
    {
        // Set up QP here
        hdr.roce.qp = (bit<24>)hdr.roce_r.rkey[15:0];
    }

    action mark_last_ack()
    {
        ig_md.is_last_ack = (bit<1>)1;
    }

    action not_last_ack()
    {
        ig_md.is_last_ack = (bit<1>)0;
    }

    table isLastAckForModData{
        key = {
            hdr.recirc_data.next_sharer: exact;
        }

        actions = {
            mark_last_ack;
            not_last_ack;
        }

        const default_action = not_last_ack();
        size = 16; // (16 computing nodes)
    }

    // 3) Route based on final destination IP
    action route(PortId_t dst_port, mac_addr_t dst_mac) {
        ig_tm_md.ucast_egress_port = dst_port;
        hdr.ethernet.dst_addr = dst_mac;
    }

    table ipRoute {
        key = {
            hdr.ipv4.dst_addr : exact;
        }

        actions = {
            route;
            nop;
        }

        const default_action = nop();
        size = 32;
    }

    bit<2> forward_to_mem_code;
    bit<2> need_ack_code;
    bit<4> invalidation_code;

    apply {
        if (hdr.inval_roce.isValid())   // Invalidation-related packets
        {
            if (ig_intr_md.ingress_port == DISAGG_RECIRC)
            {
                // Convert it to the RoCE write ACK
                hdr.roce.qp = (bit<24>)hdr.roce_r.rkey;
                convert_datapush_to_write_ack();
                hdr.ipv4.dst_addr = hdr.ipv4.src_addr;
                hdr.ipv4.src_addr = CTRL_ADDR;
                hdr.roce.pkey = 0xffff;
                hdr.roce_a.setValid();
                hdr.roce_a.mseq_no = hdr.roce.psn;
                hdr.roce.opcode = ROCE_WRITE_RES;
                hdr.roce_a.syndrome = 0;
                hdr.inval_roce.setInvalid();
                ig_tm_md.bypass_egress = 1w1;
                ig_md.skip_in_routing = 1w0;
            }
            else
            {
                hdr.udp.dst_port = UDP_ROCE;
                hdr.ipv4.dst_addr = hdr.inval_roce.src_addr;
                hdr.roce.reserved = RESERVED_INVALIDATION;
                hdr.roce.qp[15:0] = 0xffff;     // Always forward
                ig_tm_md.bypass_egress = 1w0;
                cache_recirc();                 // To generate RDMA ACK back to the sender
            }
        }
        else if (hdr.roce_a.isValid())  // RoCE ACK
        {
            // Store the up-to-date psn
            roceAckDest.apply();
            // Assign actual values for ack
            roceAck.apply();
            // Load new psn (indexed by qp)
            hdr.roce.psn = (bit<24>)cpu_blade_psn_get_action.execute(ig_md.qp_idx);  // PSN back to cpu blade
            mem_blade_psn_set_action.execute(ig_md.qp_dummy_idx);      // The latest psn used to the memory blade (or dummy ack QP in cpu)
            // Fill the ack fields with initial value
            hdr.roce_a.mseq_no = hdr.roce.psn;
            ig_md.enable_mirror = 1w0;
            hdr.roce_a.syndrome = 0;
        }
        else if(ig_intr_md.ingress_port == DISAGG_RECIRC && ig_md.cache_has_state == 1w0)
        {
            if (ig_md.perm == CREATE_NACK)
            {
                // Routine to send NACK back to the CPU
                // Send it back to the requester
                roceDummyAck.apply();
                hdr.ipv4.dst_addr = hdr.ipv4.src_addr;
                ig_md.dest_matched = 1w1;
            }
            else
            {
                // Check permission for the RDMA requests
                // Enforce write permission to RDMA write
                if (ig_md.do_roce_req_w == 1w1)
                {
                    ig_md.perm = PERMISSION_WRITE;
                }
                // Check the access permission : protection table w/ exceptional translation embedded
                if (ig_md.do_roce_req == 1w1)
                {
                    addrExceptTrans.apply();
                }
            }
            if (ig_md.perm > 0) // Check for access control
            {
                if (ig_md.dest_matched == 1w0)
                    addrTrans.apply();  // General address translation table (fit into the big partion)
                roceReq.apply();
                ig_md.enable_mirror = 1w0;
            }
        }
        else if (ig_md.do_roce_req == 1w1)
        {
            // If this is not the last data
            findCacheSlot.apply();
            if (ig_md.cache_found == 1w1)   // If we found cache directory
            {
                if (ig_md.cache_has_state == 1w0 || hdr.recirc_data.code == RECIRC_RESET)
                {
                    
                    if (!hdr.recirc_data.isValid())
                        hdr.recirc_data.setValid();
                    
                    get_cache_dir_lock();           // Get dir size and locking status
                    get_current_cache_status();     // Get current state

                    // If the cache directory entries are being updated
                    if(ig_md.cache_dir_lock > 0 && ig_md.passthrough_pkt == 4w0)
                    {
                        // Data push (do not change state/sharer) & Fin ACK messages cannot be nacked
                        // Instead, we recirculate those packets internally
                        if (ig_md.do_roce_req_w == 1w1)
                        {
                            hdr.recirc_data.code = RECIRC_RESET;
                        }
                        // Send NACK in case of the other messages
                        else
                        {
                            roceSetQpIdx.apply();
                            hdr.recirc_data.code = RECIRC_CREATE_NACK;
                            hdr.roce_r.rkey[31:28] = CREATE_NACK;
                        }
                        cache_recirc_with_custom_header();
                    }
                    else if (hdr.recirc_data.cur_state == STATE_FAIL)
                    {
                        hdr.recirc_data.code = RECIRC_RESET;
                        cache_recirc_with_custom_header();
                    }
                    else
                    {
                        get_cache_dir_size();
                        get_current_cache_sharer();     // Get current sharer list (in ig_md.sharer_list)
                        cacheDirectoryPre2.apply();     // Calculate next state and opcodes (inval, sharer)
                        getRequesterNode.apply();       // Bit-mask of the src CN (in recirc_data.next_sharer)

                        // Set up invalidation mask first
                        hdr.recirc_data.invalidation_mask = (ig_md.sharer_list | hdr.recirc_data.next_sharer);
                        hdr.roce.pkey = hdr.recirc_data.next_sharer;    // Requester

                        // Calculate next sharer list
                        if (hdr.recirc_data.reset_sharer == CACHE_ADD_SHARER) 
                        {   // Add to the current sharer list
                            hdr.recirc_data.next_sharer = ig_md.sharer_list | hdr.recirc_data.next_sharer;
                        }
                        else if (hdr.recirc_data.reset_sharer == CACHE_DEL_SHARER) 
                        {   // Delete from the current sharer list (if already in the list)
                            hdr.recirc_data.next_sharer = ig_md.sharer_list & hdr.recirc_data.next_sharer;
                            if (hdr.recirc_data.next_sharer > 0)    // Check it is in the list
                            {
                                hdr.recirc_data.next_sharer = ig_md.sharer_list ^ hdr.recirc_data.next_sharer;
                                if (hdr.recirc_data.next_sharer == 0)
                                {
                                    hdr.recirc_data.next_state = STATE_IDLE;    // No one use this directory rigth now
                                }
                            }else{
                                // If itself is not in the sharer list (i.e., already deleted), do not change state
                                hdr.recirc_data.next_state = hdr.recirc_data.cur_state;
                            }
                        }
                        else if(hdr.recirc_data.reset_sharer == CACHE_SAME_SHARER)
                        {
                            hdr.recirc_data.next_sharer = ig_md.sharer_list;
                        }

                        // Asynchronous data flush (=sending the data back to the memory, MIGHT be false invalidation)
                        if ((hdr.recirc_data.cur_state == STATE_SHA_DATA) || (hdr.recirc_data.cur_state == STATE_MOD_DATA))
                        {
                            if (ig_md.perm == 0)
                            {
                                inc_inval_counter();
                            }
                        }
                        if ((hdr.recirc_data.verify_and_skip > 0) && (hdr.roce.pkey == hdr.recirc_data.next_sharer))  // if requester is the only owner after add it to the sharer list
                        {
                            // Exceptional case for: M -> S^D -> M or M -> M^D -> M
                            // - If the requester is the current exclusice owner, we gives exclusive writable permission
                            hdr.recirc_data.next_state = STATE_MOD;
                            hdr.recirc_data.send_inval = CACHE_NO_INVAL_FULL;   // No inval, need ack, send to memory
                        }
                        else if (hdr.recirc_data.next_state == STATE_MOD_DATA)
                        {
                            // Check whether this is the last ack
                            isLastAckForModData.apply();
                            // - modified_data (M^D) state always has more than one sharer: requester + old sharer(s)
                            // If it was the last old sharer
                            if (ig_md.is_last_ack == 1w1)
                            {
                                hdr.recirc_data.next_state = STATE_MOD;
                            }
                        }
                        // Enable recirculation with a custom header
                        hdr.recirc_data.code = RECIRC_FIRST_PASS;
                        cache_recirc_with_custom_header();
                    }
                }
                else if (hdr.recirc_data.code == RECIRC_CREATE_NACK)
                {
                    hdr.recirc_data.setInvalid();
                    cache_send_nack();  // Send this message as NACK (RDMA write)
                    cache_recirc();
                }
                else
                {
                    set_cache_sharer();     // Set sharer (from recirc_data.next_sharer)
                    set_cache_status();
                    forward_to_mem_code = hdr.recirc_data.send_inval[7:6];
                    need_ack_code = hdr.recirc_data.send_inval[5:4];
                    invalidation_code = hdr.recirc_data.send_inval[3:0];
                    hdr.roce.reserved = 8w0;
                    hdr.roce_r.rkey[27:24] = hdr.recirc_data.dir_size[3:0];

                    if (hdr.recirc_data.next_state != STATE_EMPTY)
                    {
                        // A. Do we need to send invalidation message(s)?
                        if (invalidation_code == CACHE_FORWARD_DATA_MEM)
                        {
                            cache_recirc_as_roce();
                            convert_req_to_req();
                            hdr.udp.checksum = 16w0;
                        }
                        else
                        {
                            getSenderQp.apply();
                            if (invalidation_code == CACHE_FORWARD_DATA_CPU)
                            {   // Recirc and forward: forward dirty data back to the new cache region owner
                                cache_recirc_as_roce();         // Send this packet as an ACK to requester
                                get_mem_to_cpu_qp_idx_sharer(); // Retrieve requester's QP
                                convert_datapush_to_ack();
                            }
                            else if (invalidation_code == CACHE_NO_INVAL)
                            {   // Only recirc: nothing to do here (default action)
                            }
                            else if (invalidation_code == CACHE_RETURN_WRITE_ACK)
                            {
                                cache_recirc_as_roce();
                                hdr.roce_r.rkey[31:28] = INVALIDATION_ACK;
                            }
                            else if (invalidation_code == CACHE_RETURN_READ_NACK)
                            {
                                hdr.roce_r.rkey[31:28] = CREATE_NACK;
                                cache_recirc_with_custom_header();
                            }
                            else
                            {   // Send invalidation requests
                                // - Single invalidation: Modified to Shared or Modified (dirty data must be sent)
                                // - Multiple invalidation: Shared to Modified
                                if (ig_md.perm == PERMISSION_WRITE_REQ) // to Modified state
                                {
                                    hdr.roce_r.rkey[11:8] = INVALIDATION_MODIFIED;
                                }else{
                                    hdr.roce_r.rkey[11:8] = INVALIDATION_SHARED;
                                }
                                // Set up multicast for invalidations
                                SendInvalidation.apply(hdr, ig_tm_md);
                                
                                // Forward request to memory if the current state is shared (= there is no dirty data)
                                if (invalidation_code == CACHE_MULTI_INVAL)
                                {
                                    hdr.roce_r.rkey = hdr.roce_r.rkey & ~INVALIDATION_DATA_REQ;
                                }else{
                                    hdr.roce_r.rkey = hdr.roce_r.rkey | INVALIDATION_DATA_REQ;
                                }
                                // Set bitmap/mask for multicasting (will be checked in egress)
                                hdr.roce.qp[15:0] = hdr.recirc_data.invalidation_mask;
                            }
                        }
                        cache_recirc();
                        // B. Do we need to send an ACk back to computing blade?
                        if (need_ack_code == CACHE_NEED_NACK)
                        {
                            // Recirculate then it will send back to the sender as a NACK
                            hdr.recirc_data.code = RECIRC_CREATE_NACK;
                            // Recirculation must be set up already @CACHE_RETURN_READ_NACK
                        }
                        else if(need_ack_code == CACHE_NEED_ACK)
                        {
                            cache_send_ack();
                        }
                        else if(need_ack_code == CACHE_NO_ACK)
                        {   // No ACK from the switch
                            // (e.g., for data flush to memory, NIC in the memory blade will create RDMA ACK)
                            cache_no_ack();
                        }
                        else
                        {   // Last case if skip ack, which is for forwarding data push
                            // - Simply invalidate recirculation header
                            cache_skip_ack();
                        }
                        // C. Do we need to send message to memory
                        if (forward_to_mem_code == CACHE_DROP_MEM)
                        {
                            ig_md.enable_mirror = 1w0;
                        }
                    }
                }
            }
        }
        // Errous packets would be forwarded to control plane

        if (ig_md.skip_in_routing == 1w0)
        {
            ipRoute.apply();    // Update egress port based on dest ip
        }

        // Finalize normal packet
        if (ig_tm_md.bypass_egress != 1w1)
        {
            hdr.bridged_md.setValid();
            hdr.bridged_md.pkt_type = PKT_TYPE_NORMAL;
        }else{
            hdr.bridged_md.setInvalid();
        }
    }
}
