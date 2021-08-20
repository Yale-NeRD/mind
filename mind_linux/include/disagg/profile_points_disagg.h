/*
 * Profiling points
 *
 * We borrowed profiling system from LegoOS.
 * LegoOS: https://github.com/WukLab/LegoOS
 * 
 * We added profiling outside of kernel itself.
 * For example, kernel module which is not compiled with 
 * the kernel can use profiling points defined in the kernel
 * and use PROFILE_LEAVE_PTR instead of PROFILE_LEAVE.
 * Those profiling points used outside of the kernel
 * should be exported in this file by using 
 * PROTO_PROFILE_WITH_EXPORT()
 */

#ifndef __PROFILE_POINT_DISAGGREGATION_H__
#define __PROFILE_POINT_DISAGGREGATION_H__

#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/sched/clock.h>
#include <linux/stringify.h>
#ifndef __MEMORY_NODE__
#include <disagg/print_disagg.h>
#else
//do not define CONFIG_PROFILING_POINTS
#endif
struct profile_point {
	bool		enabled;
	char		pp_name[64];
	atomic_long_t	nr;
	atomic_long_t	time_ns;
} ____cacheline_aligned;

#define __profile_point		__section(.profile.point)

#define _PP_TIME(name)	__profilepoint_start_ns_##name
#define _PP_NAME(name)	__profilepoint_##name
#define _PP_EXPORT_NAME(name) __profilepoint_export_##name

/*
 * Define a profile point
 * It is ON by default.
 */
#define DEFINE_PROFILE_POINT_EN(name)                          \
	struct profile_point _PP_NAME(name) __profile_point = { \
		.enabled = true,                                    \
		.pp_name = __stringify(name),                       \
	};

/*
 * This is just a solution if per-cpu is not used.
 * Stack is per-thread, thus SMP safe.
 */
#define PROFILE_POINT_TIME_EN(name) \
	unsigned long _PP_TIME(name) __maybe_unused;

#define profile_point_start_EN(name)           \
	do                                      \
	{                                       \
		if (_PP_NAME(name).enabled)         \
			_PP_TIME(name) = sched_clock(); \
	} while (0)

#define profile_point_leave_EN(name)                                       \
	do                                                                  \
	{                                                                   \
		if (_PP_NAME(name).enabled)                                     \
		{                                                               \
			unsigned long __PP_end_time;                                \
			unsigned long __PP_diff_time;                               \
			__PP_end_time = sched_clock();                              \
			__PP_diff_time = __PP_end_time - _PP_TIME(name);            \
			atomic_long_inc(&(_PP_NAME(name).nr));                      \
			atomic_long_add(__PP_diff_time, &(_PP_NAME(name).time_ns)); \
		}                                                               \
	} while (0)

#define PROFILE_START_EN(name)             \
	do                                  \
	{                                   \
		_PP_TIME(name) = sched_clock(); \
	} while (0)

#define PROFILE_LEAVE_EN(name)                                         \
	do                                                              \
	{                                                               \
		unsigned long __PP_end_time;                                \
		unsigned long __PP_diff_time;                               \
		__PP_end_time = sched_clock();                              \
		__PP_diff_time = __PP_end_time - _PP_TIME(name);            \
		atomic_long_inc(&(_PP_NAME(name).nr));                      \
		atomic_long_add(__PP_diff_time, &(_PP_NAME(name).time_ns)); \
	} while (0)

#define PROFILE_ADD_MEASUREMENT_EN(name, __diff_time_in_ns)               \
	do                                                                 \
	{                                                                  \
		atomic_long_inc(&(_PP_NAME(name).nr));                         \
		atomic_long_add(__diff_time_in_ns, &(_PP_NAME(name).time_ns)); \
	} while (0)

// pointer version
#define PROFILE_LEAVE_PTR_EN(name_ptr)                                      \
	do                                                                   \
	{                                                                    \
		unsigned long __PP_end_time;                                     \
		unsigned long __PP_diff_time;                                    \
		__PP_end_time = sched_clock();                                   \
		__PP_diff_time = __PP_end_time - _PP_TIME(name_ptr);             \
		atomic_long_inc(&(_PP_NAME(name_ptr)->nr));                      \
		atomic_long_add(__PP_diff_time, &(_PP_NAME(name_ptr)->time_ns)); \
	} while (0)

#ifdef CONFIG_PROFILING_POINTS

#define DEFINE_PROFILE_POINT(name)	DEFINE_PROFILE_POINT_EN(name)
#define PROFILE_POINT_TIME(name)	PROFILE_POINT_TIME_EN(name)
#define profile_point_start(name)	profile_point_start_EN(name)
#define profile_point_leave(name)	profile_point_leave_EN(name)
#define PROFILE_START(name)			PROFILE_START_EN(name)
#define PROFILE_LEAVE(name)			PROFILE_LEAVE_EN(name)
#define PROFILE_ADD_MEASUREMENT(name, __diff_time_in_ns)	PROFILE_ADD_MEASUREMENT_EN(name, __diff_time_in_ns)
#define PROFILE_LEAVE_PTR(name_ptr)	PROFILE_LEAVE_PTR_EN(name_ptr)	
#define PROTO_PROFILE_WITH_EXPORT(name) struct profile_point *_PP_EXPORT_NAME(name)(void);

#else

#define DEFINE_PROFILE_POINT(name)
#define PROFILE_POINT_TIME(name)
#define profile_point_start(name)	do { } while (0)
#define profile_point_leave(name)	do { } while (0)
#define PROFILE_START(name)		do { } while (0)
#define PROFILE_LEAVE(name)		do { } while (0)
#define PROFILE_ADD_MEASUREMENT(name, __diff_time_in_ns)		do { } while (0)
#define PROFILE_LEAVE_PTR(name_ptr)		do { } while (0)
#define PROTO_PROFILE_WITH_EXPORT(name)

#endif

// Always included for eval
#define DEFINE_PROFILE_POINT_EVAL(name)		DEFINE_PROFILE_POINT_EN(name)
#define PROFILE_POINT_TIME_EVAL(name)		PROFILE_POINT_TIME_EN(name)
#define profile_point_start_EVAL(name)		profile_point_start_EN(name)
#define profile_point_leave_EVAL(name)		profile_point_leave_EN(name)
#define PROFILE_START_EVAL(name)			PROFILE_START_EN(name)
#define PROFILE_LEAVE_EVAL(name)			PROFILE_LEAVE_EN(name)

//
void print_profile_point(struct profile_point *pp);
void print_profile_points(void);

PROTO_PROFILE_WITH_EXPORT(NET_total)
PROTO_PROFILE_WITH_EXPORT(NET_send_rdma)
PROTO_PROFILE_WITH_EXPORT(NET_send_rdma_lock)
PROTO_PROFILE_WITH_EXPORT(NET_nic_prepare_rdma)
PROTO_PROFILE_WITH_EXPORT(NET_nic_send_wq_rdma)
PROTO_PROFILE_WITH_EXPORT(NET_nic_poll_cq_rdma)
PROTO_PROFILE_WITH_EXPORT(NET_send_rdma_fit)
PROTO_PROFILE_WITH_EXPORT(NET_nic_tot_rdma)

#endif /* __PROFILE_POINT_DISAGGREGATION_H__ */
