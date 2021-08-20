#include <stdio.h>
#include <iostream>
#include <fstream>
#include <cstring>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "config.h"

#define FLAG 0x0
#define FLAG_ANON MAP_PRIVATE|MAP_ANONYMOUS
#define FLAG_FILE MAP_PRIVATE|MAP_FILE
#define PROT_WRITABLE PROT_READ | PROT_WRITE
#define PROT_READONLY PROT_READ

using namespace std;

int main(int argc, char *argv[]) {
    string alloc_info;
    unsigned long addr, len;
    long offset = 0;
    int is_file, dummy_fd, prot, flags;
    char prot_str[8] = {};
    enum {phase_init = 0, phase_run = 1, phase_exit = 2};
    int phase = phase_init;
    unsigned int idx = 0;

    if (argc < 3)
    {
        cerr << "Not enough input arguments" << endl;
        return -1;
    }

    ifstream fin(argv[1]), frun(argv[2]);
    if (fin.bad() || frun.bad()) {
        cerr << "fail to open alloc lists: " << argv[1] << ", " << argv[2] << endl;
        return -1;
    }

    dummy_fd = open("/dev/null", O_RDONLY);
    if (dummy_fd < 0)
    {
        cerr << "fail to open dummy file" << endl;
        return -1;
    }

    while (phase < phase_exit)
    {
        if (phase == phase_init)
            getline(fin, alloc_info);
        else if (phase == phase_run)
            getline(frun, alloc_info);
        if (alloc_info.compare(0, 4, "mmap") == 0)
        {
            memset(prot_str, 0, 8);
            sscanf(alloc_info.c_str(), "mmap %lu %d %lu %s",
                   &len, &is_file, &addr, prot_str);

            prot = PROT_READONLY;
            if (strcmp(prot_str, "rw") && is_file == 0) // for the dummy file mapping, we disabed writable mappings
                prot = PROT_WRITABLE;

            flags = is_file > 0 ? TEST_ALLOC_FILE_FLAG : TEST_ALLOC_FLAG;

            cout << "MMAP [" << idx << "]: " << addr << ", len: " << len << ", prot: " << prot << ", file: " << is_file << endl;
            if (len > 0)
                mmap((void *)addr, len, prot, flags, is_file > 0 ? dummy_fd : -1, offset);
            idx ++;
        }
        else if (alloc_info.compare(0, 3, "brk") == 0)
        {
            // ignore BRK, since it will only increase the number of existing VMA
        }

        if (fin.eof() && phase == phase_init)
        {
            phase = phase_run;
        }

        if(frun.eof() && phase == phase_run)
        {
            phase = phase_exit;
            break;
        }
    }
    return 0;
}
