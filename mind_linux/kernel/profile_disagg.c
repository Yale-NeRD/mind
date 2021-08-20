#include <disagg/profile_points_disagg.h>

// Ported from LegoOS profiler
// https://github.com/WukLab/LegoOS
//
#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/atomic.h>
// #include <lego/profile.h>
#include <linux/time.h>

/* 
 * How to use profile pointer functions
 * = Inside header =
 * 1) Prototype of export function
 * -> PROTO_PROFILE_WITH_EXPORT(name)
 * 
 * = Inside source file =
 * 2) Body of export function WITH def. of profile point
 * -> DEFINE_PROFILE_WITH_EXPORT(name)
 * 
 * = Inside source file that will be measured =
 * 3) Def of profile point (not needed if you did 2) above)
 * -> DEFINE_PROFILE_POINT(name)
 * (OR)
 * 4) Def of profile point as a pointer, needed for 2)
 * -> static struct profile_point *_PP_NAME(name);
 * 4.1) Init pointer
 * -> _PP_NAME(name) = _PP_EXPORT_NAME(name)();
 * 
 * = Inside target function =
 * 5) Actual timer definition
 * -> PROFILE_POINT_TIME()
 * 6) Start timer
 * -> PROFILE_START(name)
 * 7) End timer: if use local variable as in 3)
 * -> PROFILE_LEAVE(name)
 * 8) End timer: if use pointer as in 2)
 * -> PROFILE_LEAVE_PTR(name)
 */

/* Profile Point */
extern struct profile_point __sprofilepoint[], __eprofilepoint[];

void print_profile_point(struct profile_point *pp)
{
	struct timespec ts = {0, 0};
	long nr = 0, avg_ns = 0, time_ns = 0;

	nr = atomic_long_read(&pp->nr);
	time_ns = atomic_long_read(&pp->time_ns);
	ts = ns_to_timespec(time_ns);

	if (!nr)
		goto print;

	avg_ns = DIV_ROUND_UP(time_ns, nr);

print:
	pr_info("%s  %35s  %6Ld.%09Ld  %16ld  %16ld\n",
			pp->enabled ? "     on" : "    off",
			pp->pp_name,
			(s64)ts.tv_sec, (s64)ts.tv_nsec,
			nr,
			avg_ns);
}
EXPORT_SYMBOL(print_profile_point);

void print_profile_points(void)
{
	struct profile_point *pp;
	int count = 0;

	pr_info("\n");
	pr_info("Kernel Profile Points\n");
	pr_info(" Status                                 Name          Total(s)                NR           Avg(ns)\n");
	pr_info("-------  -----------------------------------  ----------------  ----------------  ----------------\n");
	for (pp = __sprofilepoint; pp < __eprofilepoint; pp++)
	{
		print_profile_point(pp);
		count++;
	}
	pr_info("-------  -----------------------------------  ----------------  ----------------  ----------------\n");
	pr_info("\n");
}
EXPORT_SYMBOL(print_profile_points);

#ifdef CONFIG_PROFILING_POINTS
#define DEFINE_PROFILE_WITH_EXPORT(name)              \
	DEFINE_PROFILE_POINT(name)                        \
	struct profile_point *_PP_EXPORT_NAME(name)(void) \
	{                                                 \
		return &_PP_NAME(name);                       \
	}                                                 \
	EXPORT_SYMBOL(_PP_EXPORT_NAME(name));
#else
#define DEFINE_PROFILE_WITH_EXPORT(name)
#endif
/* predefined points for modules */
DEFINE_PROFILE_WITH_EXPORT(NET_total)
DEFINE_PROFILE_WITH_EXPORT(NET_send_rdma)
DEFINE_PROFILE_WITH_EXPORT(NET_send_rdma_lock)
DEFINE_PROFILE_WITH_EXPORT(NET_nic_prepare_rdma)
DEFINE_PROFILE_WITH_EXPORT(NET_nic_send_wq_rdma)
DEFINE_PROFILE_WITH_EXPORT(NET_nic_poll_cq_rdma)
DEFINE_PROFILE_WITH_EXPORT(NET_send_rdma_fit)
DEFINE_PROFILE_WITH_EXPORT(NET_nic_tot_rdma)
