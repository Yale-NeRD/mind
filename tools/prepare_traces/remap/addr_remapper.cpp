#include "./addr_remapper.hpp"

using namespace std;

int LoadLogFile(void *arg) {
	
	int file_id = ((struct scanner_arg *)arg)->file_id;
	string &logFileName = *(((struct scanner_arg *)arg)->logFileName);
	set<unsigned long> &addr2offset = *(((struct scanner_arg *)arg)->addr2offset_perscan);

    assert(sizeof(RWlog) == sizeof(Mlog));
    assert(sizeof(RWlog) == sizeof(Blog));
    assert(sizeof(RWlog) == sizeof(Ulog));

    char *chunk = (char *)malloc(LOG_NUM_ONCE * sizeof(RWlog));
    char *buf = NULL;
    if (!chunk) {
        printf("fail to alloc buf to hold logs\n");
        return -1;
    }

    int fd = open(logFileName.c_str(), O_RDONLY);
    if (fd < 0) {
    	printf("fail to open trace file[%d] errno[%d]\n", file_id, errno);
        return fd;
    }

	unsigned long num_logs_loaded = 0;
    while (1) {
		memset(chunk, 0, LOG_NUM_ONCE * sizeof(RWlog));
        size_t size = read(fd, chunk, LOG_NUM_ONCE * sizeof(RWlog));
        if (size != LOG_NUM_ONCE * sizeof(RWlog)) {
			if ((ssize_t)size < 0) {
				printf ("error generating remapped file[%d] errno[%d] write size: %ld, expected size: %ld\n",
					file_id, errno, LOG_NUM_ONCE * sizeof(RWlog), size);
					break;
			}
		if (size == 0)
            break;
    	}

		for (buf = chunk; buf != chunk + size; buf += sizeof(RWlog)) {
			unsigned long addr;
			char op = buf[0];
			if (op == 'R' || op == 'W') {
				struct RWlog *log = (struct RWlog *)buf;
				addr = log->addr & MMAP_ADDR_MASK;
				//printf("%c %lu %lx\n", log->op, log->usec, log->addr & MMAP_ADDR_MASK);
			} else if (op == 'M') {
				struct Mlog *log = (struct Mlog *)buf;
				addr = log->start & MMAP_ADDR_MASK;
				//printf("M %u %lx %u\n", log->hdr.usec, log->start & MMAP_ADDR_MASK, log->len);
			} else if (op == 'B') {
				struct Blog *log = (struct Blog *)buf;
				addr = log->addr & MMAP_ADDR_MASK;
				//printf("B %lu %lx\n", log->usec, log->addr & MMAP_ADDR_MASK);
			} else if (op == 'U') {
				struct Ulog *log = (struct Ulog *)buf;
				addr = log->start & MMAP_ADDR_MASK;
				//printf("U %u %lx %u\n", log->hdr.usec, log->start & MMAP_ADDR_MASK, log->len);
			} else {
				printf("unexpected op or end of trace: %c\n", op);
				break;
			}
			addr2offset.insert(addr >> PAGE_SHIFT);
			++num_logs_loaded;
			if (num_logs_loaded % 100000000 == 0)
				printf("file[%d] %lu logs loaded\n", file_id, num_logs_loaded);
		}
    }

	free(chunk);
    close(fd);
    return 0;
}


int GenLogFile(void *arg) {

	int file_id = ((struct generator_arg *)arg)->file_id;
	string &logFileName = *(((struct generator_arg *)arg)->logFileName);
	string &outFileName = *(((struct generator_arg *)arg)->outFileName);
	map<unsigned long, unsigned long> &addrRemap = *(((struct generator_arg *)arg)->addrRemap);

    char *chunk = (char *)malloc(LOG_NUM_ONCE * sizeof(RWlog));
    char *buf = NULL;
    if (!chunk) {
        printf("fail to alloc buf to hold traces\n");
        return -1;
    }

    int ifd = open(logFileName.c_str(), O_RDONLY);
    if (ifd < 0) {
        printf("fail to open original trace file[%d] errno[%d]\n", file_id, errno);
        return ifd;
    }
	int ofd = open(outFileName.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
	if (ofd < 0) {
		printf("fail to open remapped trace file[%d] errno[%d]\n", file_id, errno);
		return ofd;
	}

	unsigned long num_logs_processed = 0;
    while (1) {

		memset(chunk, 0, LOG_NUM_ONCE * sizeof(RWlog));
        size_t size = read(ifd, chunk, LOG_NUM_ONCE * sizeof(RWlog));

        if (size != LOG_NUM_ONCE * sizeof(RWlog)) {
			if ((ssize_t)size < 0) {
				printf ("error generating remapped file[%d] errno[%d] write size: %ld, expected size: %ld\n",
					file_id, errno, LOG_NUM_ONCE * sizeof(RWlog), size);
				break;
			}
			if (size == 0)
            	break;
    	}

		for (buf = chunk; buf != chunk + size; buf += sizeof(RWlog)) {
			unsigned long addr;
			char op = buf[0];
			if (op == 'R' || op == 'W') {
				struct RWlog *log = (struct RWlog *)buf;
				unsigned long usec = log->usec;
				addr = log->addr & MMAP_ADDR_MASK;
				unsigned int page_off = addr & PAGE_MASK;
				unsigned long pfn = addr >> PAGE_SHIFT;
				log->addr = (addrRemap[pfn] << PAGE_SHIFT) | page_off; 
				log->usec = usec;
				//printf("%c %lu %lx\n", log->op, log->usec, log->addr & MMAP_ADDR_MASK);
			} else if (op == 'M') {
				struct Mlog *log = (struct Mlog *)buf;
				unsigned int len = log->len;
				addr = log->start & MMAP_ADDR_MASK;
				unsigned int page_off = addr & PAGE_MASK;
				unsigned long pfn = addr >> PAGE_SHIFT;
				log->start = (addrRemap[pfn] << PAGE_SHIFT) | page_off;
				log->len = len;
				//printf("M %u %lx %u\n", log->hdr.usec, log->start & MMAP_ADDR_MASK, log->len);
			} else if (op == 'B') {
				struct Blog *log = (struct Blog *)buf;
				unsigned long usec = log->usec;
				addr = log->addr & MMAP_ADDR_MASK;
				unsigned int page_off = addr & PAGE_MASK;
				unsigned long pfn = addr >> PAGE_SHIFT;
				log->addr = (addrRemap[pfn] << PAGE_SHIFT) | page_off;
				log->usec = usec;
				//printf("B %lu %lx\n", log->usec, log->addr & MMAP_ADDR_MASK);
			} else if (op == 'U') {
				struct Ulog *log = (struct Ulog *)buf;
				unsigned int len = log->len;
				addr = log->start & MMAP_ADDR_MASK;
				unsigned int page_off = addr & PAGE_MASK;
				unsigned long pfn = addr >> PAGE_SHIFT;
				log->start = (addrRemap[pfn] << PAGE_SHIFT) | page_off;
				log->len = len;
				//printf("U %u %lx %u\n", log->hdr.usec, log->start & MMAP_ADDR_MASK, log->len);
			} else {
				printf("unexpected op or end of trace: %c\n", op);
				break;
			}

			++num_logs_processed;
			if (num_logs_processed % 100000000 == 0)
				printf("file[%d] %lu logs processed\n", file_id, num_logs_processed);
		}

		size_t put_size = write(ofd, chunk, size);
        if (size != put_size) {
            printf ("error generating remapped file[%d] errno[%d] write size: %lu, expected size: %lu\n",
				file_id, errno, put_size, size);
            break;
        }
    }

	free(chunk);
	close(ofd);
    close(ifd);
    return 0;
}

int main(int argc, char *argv[]) {

	if (argc < 2) {
		printf("too few arguments\n");
		return -1;
	}
	int num_files = atoi(argv[1]);

	if (argc != num_files * 2 + 2) {
		printf("Incorrect file number\n");
		return -1;
	}

	//each thread scan one file, then write to temporary file, put together
	printf("remapper reading original traces...\n");
	set<unsigned long> *addr2offset_perscans = new set<unsigned long>[num_files];
	pthread_t *scanners = new pthread_t[num_files];
	string *logFileNames = new string[num_files];
	struct scanner_arg *scanner_args = new struct scanner_arg[num_files];
	for (int i = 0; i < num_files; ++i) {
		logFileNames[i] = string(argv[i * 2 + 2]);
		scanner_args[i].file_id = i;
		scanner_args[i].logFileName = &logFileNames[i];
		scanner_args[i].addr2offset_perscan = &addr2offset_perscans[i];
		pthread_create(&scanners[i], NULL, (void*(*)(void*))LoadLogFile, &scanner_args[i]);
		//LoadLogFile(string(argv[i*2]));
	}
	for (int i = 0; i < num_files; ++i) {
		if (pthread_join(scanners[i], NULL)) {
    			printf("Error joining thread\n");
    			return -1;
    		}
	}

	//merge to generate a global remap
	printf("remapper start remapping addr...\n");
	set<unsigned long> &addr2offset = *(new set<unsigned long>);
	map<unsigned long, unsigned long> &addrRemap = *(new map<unsigned long, unsigned long>);
	for (int i = 0; i < num_files; ++i) {
		for (auto itr = addr2offset_perscans[i].begin(); itr != addr2offset_perscans[i].end(); ++itr) {
         	addr2offset.insert(*itr);
		}
	}
	//write intermidiate result
	unsigned long cnt = 0;
	FILE *p = fopen("addr_remap.txt", "w");
	for (auto itr = addr2offset.begin(); itr != addr2offset.end(); ++itr, ++cnt) {
		addrRemap[*itr] = cnt;
		fprintf(p, "%lx %lx\n", *itr, cnt);
	}
	fclose(p);
	printf("remapper finish remapping addr\n");


	printf("remapper start generating remapped traces...\n");
	string *outFileNames = new string[num_files];
	pthread_t *generators = new pthread_t[num_files];
	struct generator_arg *generator_args = new struct generator_arg[num_files];
	for (int i = 0; i < num_files; ++i) {
		outFileNames[i] = string(argv[i * 2 + 3]);
		generator_args[i].file_id = i;
		generator_args[i].logFileName = &logFileNames[i];
		generator_args[i].outFileName = &outFileNames[i];
		generator_args[i].addrRemap = &addrRemap;
		pthread_create(&generators[i], NULL, (void*(*)(void*))GenLogFile, &generator_args[i]);
		//LoadLogFile(string(argv[i*2]));
	}
	for (int i = 0; i < num_files; ++i) {
		if (pthread_join(generators[i], NULL)) {
    		printf("Error joining thread\n");
    		return 2;
    	}
	}

	printf("remapper done\n");
	delete[] logFileNames;
	delete[] outFileNames;
	delete[] scanners;
	delete[] generators;
	delete[] scanner_args;
	delete[] generator_args;
	delete[] addr2offset_perscans;
	delete &addr2offset;
	delete &addrRemap;

	return 0;
}