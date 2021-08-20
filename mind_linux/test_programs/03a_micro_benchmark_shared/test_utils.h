#ifndef __TEST_UTILS_H__
#define __TEST_UTILS_H__
#include <stdbool.h>

#define max(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a > _b ? _a : _b; })
#define min(a,b) \
  ({ __typeof__ (a) _a = (a); \
      __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b; })


bool TrueOrFalse(double probability, unsigned int* seedp);
double Revise(double orig, int remaining, bool positive);
int CyclingIncr(int a, int cycle_size);
int GetRandom(int min, int max, unsigned int* seedp);

#endif

