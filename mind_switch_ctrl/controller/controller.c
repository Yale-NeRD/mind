// Ref: https://github.com/abysamross/simple-linux-kernel-tcp-client-server

#include "controller.h"
#include "cacheline_manager.h"
#include "memory_management.h"
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>	//close
#include <fcntl.h> 	//socket configuration
#include <pthread.h>
#include <arpa/inet.h>
#include "cache_manager_thread.h"

// #ifndef MODULE_NAME
// #define MODULE_NAME "controller"
// #endif

#ifndef MAX_CONNS
#define MAX_CONNS 32
#endif

#define kmalloc(X, Y) malloc(X)
#define vmalloc(X) malloc(X)
#define kfree free
#define vfree free

#define pr_info(...) printf(__VA_ARGS__)
#define pr_err(...) printf(__VA_ARGS__)
#define kthread_should_stop(X) 1

/* 
 *	For debugging connection handler
 */
#define n_debug_region 3
static long t_debug[n_debug_region] = {0}, t_debug_cnt[n_debug_region] = {0};
static void start_debug(struct timespec *t_rec)
{
	clock_gettime(CLOCK_MONOTONIC, t_rec);
}

static void end_debug(int debug_idx, struct timespec *t_rec_st, struct timespec *t_rec_end)
{
	clock_gettime(CLOCK_MONOTONIC, t_rec_end);
	t_debug[debug_idx] += (((t_rec_end->tv_sec - t_rec_st->tv_sec) * 1000 * 1000) + ((t_rec_end->tv_nsec - t_rec_st->tv_nsec) / 1000));
	t_debug_cnt[debug_idx]++;
}

// For thread management
static int tcp_listener_stopped = 0;
static int tcp_acceptor_stopped = 0;
static int udp_handler_stopped = 0;
int run_thread = 0;

struct conn_handler
{
	struct tcp_conn_handler_data data[MAX_CONNS];
	pthread_t thread[MAX_CONNS];
	int conn_handler_stopped[MAX_CONNS];
};

struct conn_handler *tcp_conn_handler;
struct server_service *tcp_server;
struct server_service *udp_server;
struct server_service *cache_man_server;

char *_inet_ntoa(struct in_addr *in)
{
	char *str_ip = NULL;

	str_ip = malloc(16 * sizeof(char));
	sprintf(str_ip, "%s", inet_ntoa(*in));
	return str_ip;
}

// ========== TCP server ===========//
int tcp_server_send(struct socket *sk, int id, const char *buf,
					const size_t length, unsigned long flags)
{
	int len, left = length; //written = 0,
	(void)id;
	int sock_fd = sk->sock_fd;

repeat_send:
	len = write(sock_fd, buf, left);

	if ((!(flags & MSG_DONTWAIT) && (len == -EAGAIN)))
		goto repeat_send;

	return len;
}

int tcp_server_receive(struct socket *sk, void *buf, int size, unsigned long flags)
{
	int len;
	int sock_fd = sk->sock_fd;
	len = read(sock_fd, buf, size);
	(void)flags;
	return len;
}

int send_msg_to_mem(uint32_t msg_type, void *buf, unsigned long tot_len,
					struct socket* sk,
					void *reply_buf, unsigned long reply_size)
{
	struct mem_header *hdr;
	int ret;
	int dummy_id = 0;
	int i = 0;

	hdr = get_header_ptr(buf);
	hdr->opcode = msg_type;
	hdr->sender_id = DISAGG_CONTROLLER_NODE_ID;

	ret = tcp_server_send(sk, dummy_id,
						  (const char *)buf, tot_len,
						  MSG_DONTWAIT);
	if (ret < (int)sizeof(struct memcpy_msg_struct))
	{
		printf("Cannot send message to memory node [%d]\n", ret);
		return -1;
	}

	while (1)
	{
		for (i = 0; i < DISAGG_NET_POLLING_SKIP_COUNTER; i++)
		{
			ret = tcp_server_receive(sk, reply_buf, reply_size, MSG_DONTWAIT);
			if (ret > 0)
				return ret;
		}
		usleep(1000);	// 1 ms
	}
	return -1;
}

static int send_simple_ack(struct socket *accept_socket, int id, int ret)
{
	const int len = DISAGG_NET_SIMPLE_BUFFER_LEN;
	char out_buf[DISAGG_NET_SIMPLE_BUFFER_LEN + 1];
	memset(out_buf, 0, len + 1);
	sprintf(out_buf, "ACK %d", ret);
	tcp_server_send(accept_socket, id, out_buf,
					strlen(out_buf), MSG_DONTWAIT);
	return 0;
}

void *connection_handler(void *data)
{
	struct tcp_conn_handler_data *conn_data = (struct tcp_conn_handler_data *)data;
	struct socket *accept_socket = &conn_data->accept_socket;
	int id = conn_data->thread_id;
	char *client_ip_str = NULL;

	// Header for message
	struct mem_header *hdr;
	int ret;

	// Timers
	struct timespec t_st, t_end, t_debug_st[2], t_debug_end[2];
	long t_elapse = 0;
	unsigned long local_cnt = 0;
	void *buf = malloc(DISAGG_NET_MAXIMUM_BUFFER_LEN);
	if (!buf)
	{
		ret = -ENOMEM;
		goto out;
	}
	accept_socket->sock_is_out = 0;

	// Client's ip address
	client_ip_str = _inet_ntoa(&conn_data->address->sin_addr);

	tcp_conn_handler->conn_handler_stopped[id] = 0;	// started
	// Start timer: it just have a TCP connection
	clock_gettime(CLOCK_MONOTONIC, &t_st);
	// Set timeout
	fcntl(accept_socket->sock_fd, F_SETFL, O_NONBLOCK);
	memset(buf, 0, DISAGG_NET_MAXIMUM_BUFFER_LEN);

	while (run_thread)
	{
		if (accept_socket->sock_is_out)
		{
			usleep(1000000);	// 1 sec
			continue;
		}

		ret = tcp_server_receive(accept_socket, buf,
								 DISAGG_NET_MAXIMUM_CTRL_LEN,
								 MSG_DONTWAIT);
		if (ret > 0)
		{
			// Get header first
			if (ret >= (int)sizeof(*hdr))
			{
				int nid = get_nid_from_ip_str(client_ip_str, 0);
				hdr = get_header_ptr(buf);
				hdr->sender_id = nid;

				switch (hdr->opcode)
				{
				case DISSAGG_FORK:
					if (ret >= (int)(sizeof(*hdr) + sizeof(struct fork_msg_struct)))
					{
						pr_info("FORK: received %d\n", ret);
						ret = handle_fork(hdr, get_payload_ptr(buf), accept_socket, id);
					}
					else
					{
						ret = -1;
						send_simple_ack(accept_socket, id, ret);
					}
					break;

				case DISSAGG_EXEC:
					if (ret >= (int)(sizeof(*hdr) + sizeof(struct exec_msg_struct)))
					{
						// TODO: Apply the same 'check and receive more' scheme for all the other buf
						int recv = ret;
						void *payload = get_payload_ptr(buf);
						struct exec_msg_struct *exec_req = (struct exec_msg_struct *)payload;
						int expected_size = sizeof(struct exec_msg_struct);	// Should be short enough
						expected_size += (exec_req->num_vma - 1) * sizeof(struct exec_vmainfo);	// Size of VMA list
						expected_size += sizeof(*hdr);	// Size of header
						pr_info("EXEC: received %d, expected %d\n", recv, expected_size);
						while (recv < expected_size)
						{
							// Continue to recv
							ret = tcp_server_receive(accept_socket, (void *)((char *)buf + recv),
													 DISAGG_NET_MAXIMUM_BUFFER_LEN - recv,
													 MSG_DONTWAIT);
							if (ret > 0)
							{
								recv += ret;
							}else{
								usleep(1000000);	// 1 sec
							}
							pr_info("EXEC: received %d, expected %d\n", recv, expected_size);
						}
						ret = handle_exec(hdr, payload, accept_socket, id);
					}
					else
					{
						ret = -1;
						send_simple_ack(accept_socket, id, ret);
					}
					break;

				// Exit message
				case DISSAGG_EXIT:
					if (ret >= (int)(sizeof(*hdr) + sizeof(struct exit_msg_struct)))
					{
						pr_info("EXIT: received %d\n", ret);
						ret = handle_exit(hdr, get_payload_ptr(buf), accept_socket, id);
					}
					else
					{
						ret = -1;
						send_simple_ack(accept_socket, id, ret);
					}
					break;

				// ALLOCATION - mmap
				case DISSAGG_MMAP:
					if (ret >= (int)(sizeof(*hdr) + sizeof(struct mmap_msg_struct)))
					{
						// pr_info("MMAP: received %d\n", ret);
						// Send a reply inside the handler
						ret = handle_mmap(hdr, get_payload_ptr(buf), accept_socket, id);
					}
					else
					{
						ret = -1;
						send_simple_ack(accept_socket, id, -1);
					}
					break;

				// ALLOCATION - brk
				case DISSAGG_BRK:
					if (ret >= (int)(sizeof(*hdr) + sizeof(struct brk_msg_struct)))
					{
						// pr_info("BRK: received %d\n", ret);
						// Send a reply inside the handler
						ret = handle_brk(hdr, get_payload_ptr(buf), accept_socket, id);
					}
					else
					{
						ret = -1;
						send_simple_ack(accept_socket, id, -1);
					}
					break;

				// MUNMAP
				case DISSAGG_MUNMAP:
					if (ret >= (int)(sizeof(*hdr) + sizeof(struct munmap_msg_struct)))
					{
						// pr_info("MUNMAP: received %d\n", ret);
						// Send a reply inside the handler
						ret = handle_munmap(hdr, get_payload_ptr(buf), accept_socket, id);
					}
					else
					{
						ret = -1;
						send_simple_ack(accept_socket, id, -1);
					}
					break;

				// MREMAP
				case DISSAGG_MREMAP:
					if (ret >= (int)(sizeof(*hdr) + sizeof(struct mremap_msg_struct)))
					{
						// pr_info("MREMAP: received %d\n", ret);
						// Send a reply inside the handler
						ret = handle_mremap(hdr, get_payload_ptr(buf), accept_socket, id);
					}
					else
					{
						ret = -1;
						send_simple_ack(accept_socket, id, -1);
					}
					break;

				// RDMA
				case DISAGG_RDMA:
					if (ret >= (int)(sizeof(*hdr)))
					{
						int recv = ret;
						void *payload = get_payload_ptr(buf);
						int expected_size = sizeof(*hdr) + sizeof(struct rdma_msg_struct);
						pr_info("RDMA_INIT: received %d, expected %d\n", recv, expected_size);
						while (recv < expected_size)
						{
							// Continue to recv
							ret = tcp_server_receive(accept_socket, (void *)((char *)buf + recv),
													 DISAGG_NET_MAXIMUM_BUFFER_LEN - recv,
													 MSG_DONTWAIT);
							if (ret > 0)
							{
								recv += ret;
							}else{
								usleep(1000000);	// wait for the next packet(s)
							}
							pr_info("RDMA_INIT: received %d, expected %d\n", recv, expected_size);
						}

						// Send a reply inside the handler
						ret = sw_handle_rdma_init(hdr, payload, accept_socket, id, client_ip_str);
						if (ret)
						{
							send_simple_ack(accept_socket, id, -1);
						}
					}
					else
					{
						ret = -1;
						send_simple_ack(accept_socket, id, -1);
					}
					break;

				// DEBUG functions
				case DISSAGG_COPY_VMA:
					if (ret >= (int)(sizeof(*hdr) + sizeof(struct exec_msg_struct)))
					{
						pr_info("COPY_VMA - received: %d\n", ret);
						ret = handle_exec(hdr, get_payload_ptr(buf), accept_socket, id);
					}
					else
					{
						ret = -1;
					}
					send_simple_ack(accept_socket, id, ret);
					break;

				case DISSAGG_CHECK_VMA:
					start_debug(&t_debug_st[1]);
					if (ret >= (int)(sizeof(*hdr) + sizeof(struct exec_msg_struct)))
					{
						struct exec_reply_struct reply;
						struct exec_msg_struct *req = (struct exec_msg_struct *)get_payload_ptr(buf);
						uint64_t read_val;
						int direction = 0;
						if (req->brk)
						{
							pr_err("*** DISSAGG_CHECK_VMA - recv: %d, addr: 0x%lx, dir: 0x%lx, from %s\n",
								ret, req->stack_vm, req->brk, client_ip_str);
						}

						if (req->brk == CN_SWITCH_RST_ON_UPDATE)
						{
							direction = REG_RST_ON_UPDATE;
						}
						read_val = read_cache_dir_for_test(req->tgid, req->stack_vm, direction);
						reply.ret = (read_val & 0xffffffff);	// right 32 bits
						reply.vma_count = (read_val >> 32);		// left 32 bits
						tcp_server_send(accept_socket, id, (const char *)&reply, sizeof(reply), MSG_DONTWAIT);
					}
					else
					{
						ret = -1;
						send_simple_ack(accept_socket, id, ret);
					}
					end_debug(2, &t_debug_st[1], &t_debug_end[1]);
					break;

				case DISSAGG_FIN_CONN:
					// Finish connection and release
					clock_gettime(CLOCK_MONOTONIC, &t_end);
					t_elapse = (t_end.tv_sec - t_st.tv_sec) * 1000 * 1000;
					t_elapse += (t_end.tv_nsec - t_st.tv_nsec) / 1000;

					// Currently do not need to send another msg
					pr_info("FINISH connection: thread %d, %ld usec\n", id, t_elapse);
					goto out;

				default:
					pr_err("TCP: Cannot recognize opcode: %u from %s\n", hdr->opcode, client_ip_str);
					goto out;
				}
			}
			else
			{
				pr_err("Cannot retrieve a header from %s\n", client_ip_str);
			}

			// Reset used buffer space
			memset(buf, 0, DISAGG_NET_MAXIMUM_BUFFER_LEN);

			// To avoid watchdog
			local_cnt ++;
			if (local_cnt >= DISAGG_NET_POLLING_SKIP_COUNTER)
			{
				local_cnt = 0;
				goto sleep_and_continue;
			}
		}else{
sleep_and_continue:
			usleep(10);	// Penalty for control plane compared to data plane (UDP)
		}
	}

out:
	close(tcp_conn_handler->data[id].accept_socket.sock_fd);
	
	// Clean up data structures of this accepted connection
	if (tcp_conn_handler->data[id].address)
	{
		free(tcp_conn_handler->data[id].address);
		tcp_conn_handler->data[id].address = NULL;
	}
	memset(&tcp_conn_handler->data[id], 0, sizeof(struct tcp_conn_handler_data));

	if (client_ip_str)
		free(client_ip_str);

	if (buf)
		free(buf);
	buf = NULL;
	
	tcp_conn_handler->conn_handler_stopped[id] = 1;
	return NULL;
}

void tcp_server_accept(void)
{
	int acc_sock_fd;
	int id = 0;

	for (id = 0; id < MAX_CONNS; id++)
	{
		tcp_conn_handler->conn_handler_stopped[id] = 1;
	}

	while (run_thread)
	{
		struct sockaddr_in *client = NULL;
		int addr_len = sizeof(struct sockaddr_in);

		// Accept socket here - blocking
		client = malloc(sizeof(struct sockaddr_in));
		if ((acc_sock_fd = accept(tcp_server->listen_socket.sock_fd,
								  (struct sockaddr *)client,
								  (socklen_t *)&addr_len)) < 0)
		{
			printf("Error - cannot accept\n");
			goto err;
		}

		for (id = 1; id < MAX_CONNS; id++)
		{
			if (tcp_conn_handler->conn_handler_stopped[id])
				break;
		}

		if (id == MAX_CONNS)
		{
			// cannot accept this connection
			close(acc_sock_fd);
			fprintf(stderr, "Cannot accept new conn (%d)\n", MAX_CONNS);
			continue;
		}

		tcp_conn_handler->conn_handler_stopped[id] = 0;
		tcp_conn_handler->data[id].accept_socket.sock_fd = acc_sock_fd;
		tcp_conn_handler->data[id].thread_id = id;
		tcp_conn_handler->data[id].address = client;
		tcp_conn_handler->data[id].is_send_conn = 0;

		// Initialize and set up spinlock
		pthread_spin_init(&(tcp_conn_handler->data[id].send_msg_lock), PTHREAD_PROCESS_PRIVATE);
		tcp_conn_handler->data[id].accept_socket.sk_lock = &(tcp_conn_handler->data[id].send_msg_lock);
		pthread_create(&tcp_conn_handler->thread[id], NULL, connection_handler,
					   (void *)&tcp_conn_handler->data[id]);
		usleep(1000);
	}

	if (tcp_conn_handler->data[id].address)
	{
		free(tcp_conn_handler->data[id].address);
		tcp_conn_handler->data[id].address = NULL;
	}

	tcp_acceptor_stopped = 1;
	return;
err:
	tcp_acceptor_stopped = 1;
	return;
}

void *tcp_server_listen(void* args)
{
	struct sockaddr_in address;
	int sock_fd;
	int opt = 1;
	int error;
	(void)args;

	sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd <= 0)
	{
		printf(" *** mtp | Error: %d while creating tcp server "
				"listen socket | tcp_server_listen *** \n",
				sock_fd);
		goto err;
	}

	error = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
					   &opt, sizeof(opt));
	if (error)
	{
		printf("Error: cannot setsockopt - %d\n", error);
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(DEFAULT_PORT);

	// Bind
	error = bind(sock_fd, (struct sockaddr *)&address,
				 sizeof(address));
	if (error < 0)
	{
		printf("Error: cannot bind - %d\n", error);
		exit(EXIT_FAILURE);
	}

	// Listen
	error = listen(sock_fd, MAX_CONNS);
	if (error < 0)
	{
		printf("Error: cannot listen - %d\n", error);
		exit(EXIT_FAILURE);
	}

	tcp_server->listen_socket.sock_fd = sock_fd;
	tcp_server->address = &address;

	tcp_server_accept();
	run_thread = 0;

	close(sock_fd); // Listening socket
	tcp_listener_stopped = 1;
	exit(0);
err:
	tcp_listener_stopped = 1;
	exit(0);
}
// ========== TCP server ===========//

// ========== UDP server ===========//
void *udp_handler(struct socket *sk)
{
	char *client_ip_str = NULL;
	int ret;
	unsigned long cnt = 0;
	void *buf = malloc(DISAGG_NET_MAXIMUM_BUFFER_LEN);
	if (!buf)
	{
		ret = -ENOMEM;
		goto udp_out;
	}

	// Non blocking
	fcntl(sk->sock_fd, F_SETFL, O_NONBLOCK);
	memset(buf, 0, DISAGG_NET_MAXIMUM_BUFFER_LEN);

	while (run_thread)
	{
		sk->caddr_len = sizeof(sk->client_addr);
		ret = recvfrom(sk->sock_fd, buf, DISAGG_NET_MAXIMUM_CTRL_LEN,
					   MSG_DONTWAIT, &(sk->client_addr),
					   &sk->caddr_len);
		if (ret > 0)
		{
			// client's ip address
			client_ip_str = _inet_ntoa(&(((struct sockaddr_in *)&sk->client_addr)->sin_addr));
			if (client_ip_str)
			{
				cacheline_miss_handler(sk, client_ip_str, buf, ret);
				
				if (client_ip_str)
					free(client_ip_str);
			}
			else
			{
				printf("Cannot retrieve client's IP\n");
			}
			client_ip_str = NULL;

			// Reset used buffer space
			memset(buf, 0, DISAGG_NET_MAXIMUM_BUFFER_LEN);
		}
		cnt++;

		if (cnt > DISAGG_NET_POLLING_SKIP_COUNTER)
		{
			usleep(10);
			cnt = 0;
		}
	}
udp_out:
	// Clean up data structures of this handler
	if (buf)
		free(buf);
	buf = NULL;
	udp_handler_stopped = 1;
	printf("Terminate UDP server\n");
	return NULL;
}

void *udp_server_listen(void *args)
{
	struct sockaddr_in address;
	int sock_fd;
	int opt = 1;
	int error;
	(void)args;

	sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock_fd <= 0)
	{
		printf(" *** mtp | Error: %d while creating tcp server "
			   "listen socket | udp_server_listen *** \n",
			   sock_fd);
		goto err;
	}

	error = setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT | SO_NO_CHECK,
					   &opt, sizeof(opt));
	if (error)
	{
		printf("Error: UDP - cannot setsockopt - %d\n", error);
		exit(EXIT_FAILURE);
	}
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(DEFAULT_UDP_PORT);

	// Bind
	error = bind(sock_fd, (struct sockaddr *)&address, sizeof(address));
	if (error < 0)
	{
		printf("Error: UDP - cannot bind - %d\n", error);
		exit(EXIT_FAILURE);
	}

	udp_server->listen_socket.sock_fd = sock_fd;
	udp_server->address = &address;

	udp_handler(&udp_server->listen_socket);
	run_thread = 0;
	udp_handler_stopped = 1;
	udp_server->running = 0;
	exit(0);
err:
	udp_handler_stopped = 1;
	udp_server->running = 0;
	exit(0);
}
// ========== End of UDP server ===========//

// =========== Cache size manager ============//
void *cache_manager_main(void *args)
{
	(void)args;	// Make compiler happy
	run_cache_size_thread(cache_man_server);
	cache_man_server->running = 0;
	exit(0);
}
// ========== End of cache size manager ======//

static int server_start(void)
{
	run_thread = 1;
	tcp_server->running = 1;
	pthread_create(&tcp_server->thread, &tcp_server->pth_attr, &tcp_server_listen, NULL);
	
	udp_server->running = 1;
	pthread_create(&udp_server->thread, &udp_server->pth_attr, &udp_server_listen, NULL);

	cache_man_server->running = 1;
	pthread_create(&cache_man_server->thread, &cache_man_server->pth_attr, &cache_manager_main, NULL);
	
	return 0;
}

int network_server_init(void)
{
	tcp_server = malloc(sizeof(struct server_service));
	memset(tcp_server, 0, sizeof(struct server_service));
	tcp_conn_handler = malloc(sizeof(struct conn_handler));
	memset(tcp_conn_handler, 0, sizeof(struct conn_handler));

	// udp server for cache coherence
	udp_server = malloc(sizeof(struct server_service));
	memset(udp_server, 0, sizeof(struct server_service));

	// Cache management server (dynamic cache resizing)
	cache_man_server = malloc(sizeof(struct server_service));
	memset(cache_man_server, 0, sizeof(struct server_service));

	initialize_node_list();
	init_mn_man();
	cacheline_init();	// Data structure for cacheline manager
	cache_man_init();

	run_controller_test();

	server_start();
	return 0;
}

static void print_exit_stat(void)
{
	int i = 0;
	for (i = 0; i < n_debug_region; i++)
	{
		if (t_debug_cnt[i] == 0)
			t_debug_cnt[i] = 1;
		pr_info("Debuging routine #%d: Total %ld usec, Avg %ld usec, Cnt %ld\n",
				i, t_debug[i], t_debug[i] / t_debug_cnt[i], t_debug_cnt[i]);
	}
}

void network_server_exit(void)
{
	int ret;
	int id;

	run_thread = 0;
	print_bfrt_addr_trans_rule_counters();

	if (!tcp_server->running)
		pr_info(" *** mtp | No kernel thread to kill | "
				"network_server_exit *** \n");
	else
	{
		for (id = 0; id < MAX_CONNS; id++)
		{
			if (!tcp_conn_handler->conn_handler_stopped[id])
			{
				ret = pthread_cancel(tcp_conn_handler->thread[id]);
				if (!ret)
					pr_info("[STOP] connection handler thread: %d\n", id);
			}
		}

		if (!tcp_listener_stopped)
		{
			ret = pthread_cancel(tcp_server->thread);
			if (!ret)
				pr_info("[STOP] tcp-server listening thread\n");
		}

		if (tcp_server != NULL && !tcp_listener_stopped)
		{
			close(tcp_server->listen_socket.sock_fd);
		}
	}
	if (tcp_conn_handler)
		free(tcp_conn_handler);
	tcp_conn_handler = NULL;
	if (tcp_server)
		free(tcp_server);
	tcp_server = NULL;

	if (!udp_server->running){
		;
	}
	else
	{
		if (!udp_handler_stopped)
		{
			ret = pthread_cancel(udp_server->thread);
			if (!ret)
				pr_info("[STOP] udp-server listening thread\n");
		}

		if (udp_server != NULL && !udp_handler_stopped)
		{
			// No close for UDP
		}
	}
	if (udp_server)
		free(udp_server);
	udp_server = NULL;

	if (!cache_man_server->running){
		;
	}
	else
	{
		ret = pthread_cancel(cache_man_server->thread);
		if (!ret)
			pr_info(" *** mtp | tcp server listening thread"
					" stopped | network_server_exit *** \n");
	}
	if (cache_man_server)
		free(cache_man_server);
	cache_man_server = NULL;

	// Clear data structures
	cacheline_clear();
	clear_mn_man();
	print_exit_stat();

	pr_info("network_server_exit *** \n");
}
