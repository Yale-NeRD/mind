#include "network_server.h"
#include "memory_management.h"
#include "network_rdma.h"

#ifndef DEFAULT_PORT
#define DEFAULT_PORT _destport
#endif

#ifndef MODULE_NAME
#define MODULE_NAME "network_server"
#endif

#ifndef MAX_CONNS
#define MAX_CONNS 32
#endif

static unsigned char _ctrl_ip[5] = {10,10,10,1,'\0'};


MODULE_LICENSE("GPL");

static int tcp_listener_stopped = 0;
static int tcp_acceptor_stopped = 0;

DEFINE_SPINLOCK(tcp_server_lock);

struct tcp_conn_handler_data
{
    struct sockaddr_in *address;
    struct socket *accept_socket;
    int thread_id;
};

struct tcp_conn_handler
{
    struct tcp_conn_handler_data *data[MAX_CONNS+1];
    struct task_struct *thread[MAX_CONNS+1];
    int tcp_conn_handler_stopped[MAX_CONNS+1]; 
};

struct tcp_conn_handler *tcp_conn_handler;

struct tcp_server_service
{
    int running;  
    struct socket *listen_socket;
    struct task_struct *thread;
    struct task_struct *accept_thread;
};

struct tcp_server_service *tcp_server;

// handler for actual requests
int connection_handler(void *data);

char *inet_ntoa(struct in_addr *in)
{
    char *str_ip = NULL;
    u_int32_t int_ip = 0;
    
    str_ip = kmalloc(16 * sizeof(char), GFP_KERNEL);

    if(!str_ip)
        return NULL;
    else
        memset(str_ip, 0, 16);

    int_ip = in->s_addr;

    sprintf(str_ip, "%d.%d.%d.%d", (int_ip) & 0xFF, (int_ip >> 8) & 0xFF,
            (int_ip >> 16) & 0xFF, (int_ip >> 24) & 0xFF);
    
    return str_ip;
}

u32 inet_aton(u8 *ip)
{
    u32 addr = 0;
    int i;

    for(i=0; i<4; i++)
    {
        addr += ip[i];
        if(i==3)
                break;
        addr <<= 8;
    }
    return addr;
}

static int wait_socket_recv(struct socket *conn_socket)
{
    DECLARE_WAITQUEUE(recv_wait, current);
    int cnt = 0;
    
    if (!conn_socket)
        return -1;

    // spin_lock(&recv_msg_lock);
    add_wait_queue(&conn_socket->sk->sk_wq->wait, &recv_wait);  
    while(skb_queue_empty(&conn_socket->sk->sk_receive_queue) 
            && cnt <= _MAX_CHECK_COUNTER)
    {
        __set_current_state(TASK_INTERRUPTIBLE);
        cnt++;
        schedule_timeout(_RECV_CHECK_TIME_IN_JIFFIES);
    }
    __set_current_state(TASK_RUNNING);
    remove_wait_queue(&conn_socket->sk->sk_wq->wait, &recv_wait);
    // spin_unlock(&recv_msg_lock);
    return 0;
}

int tcp_initialize_conn(struct socket **conn_socket, 
                                u32 destip_32, u16 destport)
{
    int ret = -1;
    struct sockaddr_in saddr;
    int retry = 0;

    
    ret = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, conn_socket);
    if(ret < 0 || !(*conn_socket))
    {
        /* NULL POINTER ERROR */
        return -ERR_DISAGG_NET_CREATE_SOCKET;
    }
    // pr_info("Socket created for addr 0x%0x\n", destip_32);
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(destport);
    saddr.sin_addr.s_addr = htonl(destip_32);

    while (1)
    {
        ret = (*conn_socket)->ops->connect(*conn_socket, (struct sockaddr *)&saddr,
                                        sizeof(saddr), O_RDWR);
        pr_info("Try to connect...%d (%d/%d)\n", ret, retry, DISAGG_NET_TCP_MAX_RETRY);
        if(!ret || (ret == -EINPROGRESS))
        {
            break;  // success   
        }
        retry ++;

        if (retry >= DISAGG_NET_TCP_MAX_RETRY)
        {
            pr_info(" *** mtp | Error: %d while connecting using conn "
                    "socket. | setup_connection *** \n", ret);
            return -ERR_DISAGG_NET_CONN_SOCKET;
        }
        msleep(DISAGG_NET_SLEEP_RESET_IN_MS);
        
    }
    return 0;
}

int tcp_release_conn(struct socket *conn_socket)
{
    if(conn_socket != NULL)
    {
        sock_release(conn_socket);
    }
    return 0;
}

int tcp_finish_conn(struct socket *conn_socket)
{
    struct mem_header* hdr;
    void *msg = kmalloc(sizeof(*hdr), GFP_KERNEL);
    int dummy_id = 0;
    
	if (unlikely(!msg)) {
		tcp_release_conn(conn_socket);
        return -ENOMEM;
	}

    hdr = get_header_ptr(msg);
	hdr->opcode = DISSAGG_FIN_CONN;
    hdr->sender_id = get_local_node_id();

    // ignore error here, because we will relase the connection anyway
    tcp_server_send(conn_socket, dummy_id, msg, sizeof(*hdr), MSG_DONTWAIT);
    kfree(msg);
    return tcp_release_conn(conn_socket);
}

// Initialize out-going socket
int send_one_msg_to_ctrl(u32 msg_type, void *payload, u32 len_payload,
                        void *retbuf, u32 max_len_retbuf)
{
    int ret = 0;
    u32 tot_len;
    void *msg = NULL, *payload_msg;
    struct socket *conn_socket = NULL;
    struct mem_header* hdr;
    int dummy_id = 0;

    // pr_info("Try init conn\n");
    if (!retbuf)
        return -ERR_DISAGG_NET_INCORRECT_BUF;

    // initiate new connection
    ret = tcp_initialize_conn(&conn_socket, inet_aton(_ctrl_ip), _destport);
    if ((ret < 0) || !conn_socket)
    {
        return ret;
    }
    // pr_info("Init conn\n");

    // make header and attach payload
    tot_len = len_payload + sizeof(*hdr);
    msg = kmalloc(tot_len, GFP_KERNEL);
    if (!msg) {
		ret = -ENOMEM;
        goto out_sendmsg;
	}
    hdr = get_header_ptr(msg);
	hdr->opcode = msg_type;
    hdr->sender_id = get_local_node_id();

    payload_msg = get_payload_ptr(msg);
	memcpy(payload_msg, payload, len_payload);
    barrier();

    // send request
    ret = tcp_server_send(conn_socket, dummy_id, msg, tot_len, MSG_DONTWAIT);
    if (ret < tot_len){
        ret = -ERR_DISAGG_NET_FAILED_TX;
        goto out_sendmsg;
    }

    // polling response
    wait_socket_recv(conn_socket);    

    if(!skb_queue_empty(&conn_socket->sk->sk_receive_queue))
    {
        memset(retbuf, 0, max_len_retbuf);
        ret = tcp_server_receive(conn_socket, dummy_id, NULL, retbuf, 
                                 max_len_retbuf, MSG_DONTWAIT);
    }else{
        ret = -ERR_DISAGG_NET_TIMEOUT;
    }
    pr_info("Received msg [%d]\n", ((struct rdma_msg_struct *)retbuf)->ret);

    if (ret > 0 && ret >= (int)sizeof(struct rdma_msg_struct) &&
        !(((struct rdma_msg_struct *)retbuf)->ret))
    {
        //run kernel thread for this connection
        struct sockaddr_in *client;
        int addr_len;
        struct tcp_conn_handler_data * data;
        int id = MAX_CONNS; // last index is dedicated to connection with controller

        data = kmalloc(sizeof(struct tcp_conn_handler_data), GFP_KERNEL);
        memset(data, 0, sizeof(struct tcp_conn_handler_data));

        client = kmalloc(sizeof(struct sockaddr_in), GFP_KERNEL);
        memset(client, 0, sizeof(struct sockaddr_in));

        addr_len = sizeof(struct sockaddr_in);

        conn_socket->ops->getname(conn_socket,
                                  (struct sockaddr *)client,
                                  &addr_len, 2);

        data->address = NULL;
        data->accept_socket = conn_socket;
        data->thread_id = id;

        tcp_conn_handler->tcp_conn_handler_stopped[id] = 0;
        tcp_conn_handler->data[id] = data;
        tcp_conn_handler->thread[id] =
            kthread_run((void *)connection_handler, (void *)data, MODULE_NAME);

        pr_info("Thread for listening msg from controller: 0x%lx\n",
                (unsigned long)tcp_conn_handler->thread[id]);
    }
    else
    {
    out_sendmsg:
        // release connection
        tcp_finish_conn(conn_socket);
    }

    if (msg)
        kfree(msg);

    // ret >= 0: size of received data, ret < 0: errors
    return ret;
}

int tcp_server_send(struct socket *sock, int id, const char *buf,
                const size_t length, unsigned long flags)
{
    struct msghdr msg;
    struct kvec vec;
    int len, written = 0, left =length;
    mm_segment_t oldmm;

    msg.msg_name    = 0;
    msg.msg_namelen = 0;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = flags;

    oldmm = get_fs(); set_fs(KERNEL_DS);

repeat_send:
    vec.iov_len = left;
    vec.iov_base = (char *)buf + written;

    len = kernel_sendmsg(sock, &msg, &vec, 1, left);
    
    if((len == -ERESTARTSYS) || (!(flags & MSG_DONTWAIT) &&
                            (len == -EAGAIN)))
        goto repeat_send;

    set_fs(oldmm);
    return len;
}

int tcp_server_receive(struct socket *sock, int id,struct sockaddr_in *address,
                       unsigned char *buf, int size, unsigned long flags)
{
    struct msghdr msg;
    struct kvec vec;
    int len;
    
    if(sock==NULL)
    {
        pr_info(" *** mtp | tcp server receive socket is NULL| "
                " tcp_server_receive *** \n");
        return -1;
    }

    msg.msg_name = 0;
    msg.msg_namelen = 0;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags = flags;

    vec.iov_len = size;
    vec.iov_base = buf;

read_again:
    len = kernel_recvmsg(sock, &msg, &vec, 1, size, flags);

    if(len == -EAGAIN || len == -ERESTARTSYS)
        goto read_again;
    
    // received message
    return len;
}

static int send_simple_ack(struct socket *accept_socket, int id, int ret)
{
    // TEMPORARY
    const int len = DISAGG_NET_SIMPLE_BUFFER_LEN;
    unsigned char out_buf[DISAGG_NET_SIMPLE_BUFFER_LEN+1];
    memset(out_buf, 0, len+1);
    sprintf(out_buf, "ACK %d", ret);
    tcp_server_send(accept_socket, id, out_buf,
                    strlen(out_buf), MSG_DONTWAIT);
    return 0;
}

int connection_handler(void *data)
{
    struct tcp_conn_handler_data *conn_data = 
            (struct tcp_conn_handler_data *)data;

    struct sockaddr_in *address = conn_data->address;
    struct socket *accept_socket = conn_data->accept_socket;
    int id = conn_data->thread_id;

    // header for message
    struct mem_header* hdr;
    int ret; 
    DECLARE_WAITQUEUE(recv_wait, current);

    //timer
    struct timespec t_st, t_end;
    long t_elapse = 0;
    void* buf = vmalloc(DISAGG_NET_MAXIMUM_BUFFER_LEN);
    if (!buf)
    {
        ret = -ENOMEM;
        goto out;
    }

    allow_signal(SIGKILL|SIGSTOP);

    // Start timer: it just have a TCP connection
    getnstimeofday(&t_st);

    /*
     *  Those messages are control plane messages
     *  So, no need to busy polling
     *  Those routines are only for supporting
     *  memory initialization (fork, exec, ...)
     */
    while(1)
    {
        add_wait_queue(&accept_socket->sk->sk_wq->wait, &recv_wait);  

        while(skb_queue_empty(&accept_socket->sk->sk_receive_queue))
        {
            __set_current_state(TASK_INTERRUPTIBLE);
            schedule_timeout(_RECV_CHECK_TIME_IN_JIFFIES);

            if(kthread_should_stop())
            {
                pr_info(" *** mtp | tcp server handle connection "
                "thread stopped | connection_handler *** \n");

                tcp_conn_handler->tcp_conn_handler_stopped[id]= 1;

                __set_current_state(TASK_RUNNING);
                remove_wait_queue(&accept_socket->sk->sk_wq->wait,
                                        &recv_wait);
                
                kfree(tcp_conn_handler->data[id]->address);
                kfree(tcp_conn_handler->data[id]);
                sock_release(tcp_conn_handler->data[id]->accept_socket);
                return 0;
            }

            if(signal_pending(current))
            {
                __set_current_state(TASK_RUNNING);
                remove_wait_queue(&accept_socket->sk->sk_wq->wait,
                                        &recv_wait);
                goto out;
            }
        }
        __set_current_state(TASK_RUNNING);
        remove_wait_queue(&accept_socket->sk->sk_wq->wait, &recv_wait);

        memset(buf, 0, DISAGG_NET_MAXIMUM_BUFFER_LEN);
        ret = tcp_server_receive(accept_socket, id, address, 
                                        buf, DISAGG_NET_MAXIMUM_BUFFER_LEN,
                                        MSG_DONTWAIT);
        if (ret > 0)
        {
            // get header first
            if (ret >= sizeof(*hdr)){
                hdr = get_header_ptr(buf);

                switch(hdr->opcode){
                // reuqest for memory management
                case DISAGG_MEM_INIT:
                    if (ret >= sizeof(*hdr) + sizeof(struct meminit_msg_struct)){
                        // pr_info("MEM_INIT: received %d\n", ret);
                        ret = handle_mem_init(hdr, get_payload_ptr(buf), accept_socket, id);
                    }else{
                        ret = -1;
                        send_simple_ack(accept_socket, id, ret);
                    }
                    break;

                case DISAGG_MEM_COPY:
                    if (ret >= sizeof(*hdr) + sizeof(struct memcpy_msg_struct))
                    {
                        // pr_info("MEM_COPY: received %d\n", ret);
                        ret = handle_mem_cpy(hdr, get_payload_ptr(buf), accept_socket, id);
                    }
                    else
                    {
                        ret = -1;
                        send_simple_ack(accept_socket, id, ret);
                    }
                    break;

                case DISSAGG_FIN_CONN:
                    // finish connection and release
                    getnstimeofday(&t_end);
                    t_elapse = (t_end.tv_sec - t_st.tv_sec) * 1000 * 1000;
                    // minus even value will be fine
                    t_elapse += (t_end.tv_nsec - t_st.tv_nsec) / 1000;  
                    
                    // Currently do not need to send another msg
                    pr_info("FINISH connection: thread %d, %ld usec\n", id, t_elapse);
                    goto out;

                default:
                    pr_err("TCP: Cannot recognize opcode: %u\n", hdr->opcode);
                    goto out;
                }

            }else{
                pr_err("Cannot retrieve a header\n");
            }
        }
    }

out:
    tcp_conn_handler->tcp_conn_handler_stopped[id]= 1;
    kfree(tcp_conn_handler->data[id]->address);
    sock_release(tcp_conn_handler->data[id]->accept_socket);
    kfree(tcp_conn_handler->data[id]);
    tcp_conn_handler->data[id] = NULL;
    tcp_conn_handler->thread[id] = NULL;
    if (buf)
        vfree(buf);
    buf = NULL;
    do_exit(0);
}

int tcp_server_accept(void)
{
    int accept_err = 0;
    struct socket *socket;
    struct socket *accept_socket = NULL;
    struct inet_connection_sock *isock; 
    int id = 0;
    DECLARE_WAITQUEUE(accept_wait, current);

    allow_signal(SIGKILL|SIGSTOP);
    //spin_unlock(&tcp_server_lock);

    socket = tcp_server->listen_socket;
    pr_info(" *** mtp | creating the accept socket | tcp_server_accept "
            "*** \n");

    while(1)
    {
        struct tcp_conn_handler_data *data = NULL;
        struct sockaddr_in *client = NULL;
        char *tmp;
        int addr_len;

        accept_err =  
            sock_create_lite(socket->sk->sk_family, socket->type,\
                            socket->sk->sk_protocol, &accept_socket);

        if(accept_err < 0 || !accept_socket)
        {
            pr_info(" *** mtp | accept_error: %d while creating "
                    "tcp server accept socket | "
                    "tcp_server_accept *** \n", accept_err);
            goto err;
        }

        accept_socket->type = socket->type;
        accept_socket->ops  = socket->ops;

        isock = inet_csk(socket->sk);
        add_wait_queue(&socket->sk->sk_wq->wait, &accept_wait);
        while(reqsk_queue_empty(&isock->icsk_accept_queue))
        {
            __set_current_state(TASK_INTERRUPTIBLE);

            //change this HZ to about 5 mins in jiffies
            schedule_timeout(HZ);
            if(kthread_should_stop())
            {
                pr_info(" *** mtp | tcp server acceptor thread "
                        "stopped | tcp_server_accept *** \n");
                tcp_acceptor_stopped = 1;
                __set_current_state(TASK_RUNNING);
                remove_wait_queue(&socket->sk->sk_wq->wait,\
                                &accept_wait);
                sock_release(accept_socket);
                return 0;
            }

            if(signal_pending(current))
            {
                __set_current_state(TASK_RUNNING);
                remove_wait_queue(&socket->sk->sk_wq->wait,\
                                &accept_wait);
                goto release;
            }

        } 
        __set_current_state(TASK_RUNNING);
        remove_wait_queue(&socket->sk->sk_wq->wait, &accept_wait);

        accept_err = 
                socket->ops->accept(socket, accept_socket, O_NONBLOCK, true);

        if(accept_err < 0)
        {
            pr_info(" *** mtp | accept_error: %d while accepting "
                    "tcp server | tcp_server_accept *** \n",
                    accept_err);
            goto release;
        }

        client = kmalloc(sizeof(struct sockaddr_in), GFP_KERNEL);   
        memset(client, 0, sizeof(struct sockaddr_in));

        addr_len = sizeof(struct sockaddr_in);

        accept_err = 
            accept_socket->ops->getname(accept_socket,
                            (struct sockaddr *)client,
                            &addr_len, 2);

        if(accept_err < 0)
        {
            pr_info(" *** mtp | accept_error: %d in getname "
                    "tcp server | tcp_server_accept *** \n",
                    accept_err);
            goto release;
        }

        /* should I protect this against concurrent access? */
        for(id = 0; id < MAX_CONNS; id++)
        {
            if(tcp_conn_handler->thread[id] == NULL)
                break;
        }

        // print out connection info
        tmp = inet_ntoa(&(client->sin_addr));
        pr_info("New conn: connection from %s:%d - thread id %d\n",
                tmp, ntohs(client->sin_port), id);
        kfree(tmp);

        if(id == MAX_CONNS)
            goto release;

        data = kmalloc(sizeof(struct tcp_conn_handler_data), GFP_KERNEL);
        memset(data, 0, sizeof(struct tcp_conn_handler_data));

        data->address = client;
        data->accept_socket = accept_socket;
        data->thread_id = id;

        tcp_conn_handler->tcp_conn_handler_stopped[id] = 0;
        tcp_conn_handler->data[id] = data;
        tcp_conn_handler->thread[id] = 
        kthread_run((void *)connection_handler, (void *)data, MODULE_NAME);

        if(kthread_should_stop())
        {
            pr_info(" *** mtp | tcp server acceptor thread stopped"
                    " | tcp_server_accept *** \n");
            tcp_acceptor_stopped = 1;
            return 0;
        }
                
        if(signal_pending(current))
        {
                break;
        }
    }

    if (tcp_conn_handler->data[id])
    {
        if (tcp_conn_handler->data[id]->address)
            kfree(tcp_conn_handler->data[id]->address);
        kfree(tcp_conn_handler->data[id]);
    }
    tcp_acceptor_stopped = 1;
    do_exit(0);
release: 
    sock_release(accept_socket);
err:
    tcp_acceptor_stopped = 1;
    do_exit(0);
}

int tcp_server_listen(void)
{
    int server_err;
    struct socket *conn_socket;
    struct sockaddr_in server;
    DECLARE_WAIT_QUEUE_HEAD(wq);

    allow_signal(SIGKILL|SIGTERM);         
    server_err = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP,
                            &tcp_server->listen_socket);
    if(server_err < 0)
    {
        pr_info(" *** mtp | Error: %d while creating tcp server "
                "listen socket | tcp_server_listen *** \n", server_err);
        goto err;
    }

    conn_socket = tcp_server->listen_socket;
    tcp_server->listen_socket->sk->sk_reuse = 1;

    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_family = AF_INET;
    server.sin_port = htons(DEFAULT_PORT);

    server_err = 
    conn_socket->ops->bind(conn_socket, (struct sockaddr*)&server,\
                    sizeof(server));

    if(server_err < 0)
    {
        pr_info(" *** mtp | Error: %d while binding tcp server "
                "listen socket | tcp_server_listen *** \n", server_err);
        goto release;
    }

    server_err = conn_socket->ops->listen(conn_socket, 16);
    if(server_err < 0)
    {
        pr_info(" *** mtp | Error: %d while listening in tcp "
                "server listen socket | tcp_server_listen "
                "*** \n", server_err);
                goto release;
    }

    tcp_server->accept_thread = 
            kthread_run((void*)tcp_server_accept, NULL, MODULE_NAME);

    while(1)
    {
        wait_event_timeout(wq, 0, 3*HZ);

        if(kthread_should_stop())
        {
            pr_info(" *** mtp | tcp server listening thread"
                    " stopped | tcp_server_listen *** \n");
            return 0;
        }
        if(signal_pending(current))
            goto release;
    }
    sock_release(conn_socket);
    tcp_listener_stopped = 1;
    do_exit(0);
release:
    sock_release(conn_socket);
err:
    tcp_listener_stopped = 1;
    do_exit(0);
}

int tcp_server_start(void)
{
    tcp_server->running = 1;
    tcp_server->thread = kthread_run((void *)tcp_server_listen, NULL,\
                                    MODULE_NAME);
    return 0;
}

// this will be linked from thpool.o
static int __init network_server_init(void)
{
    pr_info(" *** mtp | network_server initiated | "
            "network_server_init ***\n");
    tcp_server = kmalloc(sizeof(struct tcp_server_service), GFP_KERNEL);
    memset(tcp_server, 0, sizeof(struct tcp_server_service));

    tcp_conn_handler = kmalloc(sizeof(struct tcp_conn_handler), GFP_KERNEL);
    memset(tcp_conn_handler, 0, sizeof(struct tcp_conn_handler));

    init_mn_man();
    rdma_library_init();

    // start server module
    tcp_server_start();
    return 0;
}

static void __exit network_server_exit(void)
{
    int ret;
    int id;

    if(tcp_server->thread == NULL)
            pr_info(" *** mtp | No kernel thread to kill | "
                    "network_server_exit *** \n");
    else
    {
        for(id = 0; id < MAX_CONNS + 1; id++)   // include controller session
        {
            if(tcp_conn_handler->thread[id] != NULL)
            {

                if(!tcp_conn_handler->tcp_conn_handler_stopped[id])
                {
                    ret = 
                    kthread_stop(tcp_conn_handler->thread[id]);

                    if(!ret)
                            pr_info(" *** mtp | tcp server "
                            "connection handler thread: %d "
                            "stopped | network_server_exit "
                            "*** \n", id);
                }
            }
        }

        if(!tcp_acceptor_stopped)
        {
            ret = kthread_stop(tcp_server->accept_thread);
            if(!ret)
                pr_info(" *** mtp | tcp server acceptor thread"
                        " stopped | network_server_exit *** \n");
        }

        if(!tcp_listener_stopped)
        {
            ret = kthread_stop(tcp_server->thread);
            if(!ret)
                pr_info(" *** mtp | tcp server listening thread"
                        " stopped | network_server_exit *** \n");
        }

        if(tcp_server->listen_socket != NULL && !tcp_listener_stopped)
        {
            sock_release(tcp_server->listen_socket);
            tcp_server->listen_socket = NULL;
        }

        kfree(tcp_conn_handler);
        kfree(tcp_server);
        tcp_server = NULL;
    }

    clear_mn_man();

    pr_info(" *** mtp | network server module unloaded | "
            "network_server_exit *** \n");
}
module_init(network_server_init)
module_exit(network_server_exit)
