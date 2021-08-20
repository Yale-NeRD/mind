#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include "../../include/disagg/config.h"
#include "test_utils.h"

static int calc_mask_sum(unsigned int mask) {
  int sum = 0;
  while (mask > 0) {
    if (mask & 0x1)
      sum++;
    mask >>= 1;
  }
  return sum;
}

bool TrueOrFalse(double probability, unsigned int *seedp) {
  return (rand_r(seedp) % 100) < probability;
}

double Revise(double orig, int remaining, bool positive) {
  if (positive) {  //false positive
    return (remaining * orig - 1) / remaining;
  } else {  //false negative
    return (remaining * orig + 1) / remaining;
  }
}

int CyclingIncr(int a, int cycle_size) {
  return ++a == cycle_size ? 0 : a;
}

int GetRandom(int min, int max, unsigned int *seedp) {
  int ret = (rand_r(seedp) % (max - min)) + min;
  return ret;
}

int pin_to_core(int core_id) {
  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (core_id < 0 || core_id >= num_cores)
    return -1;

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);

  pthread_t current_thread = pthread_self();
  return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

