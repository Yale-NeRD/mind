struct log_header_5B {
	char op;
        unsigned int usec;
}__attribute__((__packed__));

struct RWlog {
	char op;
	union {
		struct {
			char pad[6];
			unsigned long usec;
		}__attribute__((__packed__));
		unsigned long addr;
	}__attribute__((__packed__));
}__attribute__((__packed__));

struct Mlog {
	struct log_header_5B hdr;
	union {
		unsigned long start;
		struct {
			char pad[6];
        		unsigned len;
		}__attribute__((__packed__));
	}__attribute__((__packed__));
}__attribute__((__packed__));

struct Blog {
    char op;
    union { 
        struct {
            char pad[6];
            unsigned long usec;
        }__attribute__((__packed__));
        unsigned long addr;
    }__attribute__((__packed__));
}__attribute__((__packed__));

struct Ulog {
    struct log_header_5B hdr;
	union {
        unsigned long start;
		struct {
            char pad[6];
            unsigned len;
        }__attribute__((__packed__));
    }__attribute__((__packed__));
}__attribute__((__packed__));