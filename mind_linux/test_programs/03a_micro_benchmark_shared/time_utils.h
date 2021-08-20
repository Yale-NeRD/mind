#include <time.h>
#include <sys/time.h>
#include <sched.h>


double get_time() {
  struct timespec cur;
  clock_gettime(CLOCK_MONOTONIC, &cur);
  return (cur.tv_sec * 1e9 + cur.tv_nsec) * 1e-9;
}


static inline unsigned long calculate_dt(struct timeval *ts)
{
	unsigned long old_t = ts->tv_sec * 1000000 + ts->tv_usec;
	gettimeofday(ts, NULL);
	return ts->tv_sec * 1000000 + ts->tv_usec - old_t;

}

static inline void measure_time_start(struct timeval *ts)
{
	gettimeofday(ts, NULL);
}

static inline unsigned long measure_time_end(struct timeval *ts)
{
	return calculate_dt(ts);
}

