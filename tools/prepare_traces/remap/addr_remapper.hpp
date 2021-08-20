#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <string.h>
#include <algorithm>
#include <cassert>
#include <pthread.h>
#include <sys/stat.h>
#include "../include/trace_def.hpp"

#define LOG_NUM_ONCE 1000000
#define MMAP_ADDR_MASK 0xffffffffffff
#define TIME_WINDOW 100000000
#define PAGE_MASK 0x1fffff
#define PAGE_SHIFT 21

struct scanner_arg {
    int file_id;
	std::string *logFileName;
	std::set<unsigned long> *addr2offset_perscan;
};

struct generator_arg {
    int file_id;
	std::string *logFileName;
	std::string *outFileName;
	std::map<unsigned long, unsigned long> *addrRemap;
};