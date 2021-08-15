import csv

# import numpy as np

# /* type of messages */
DISAGG_FORK = 1
DISAGG_EXEC = 2
DISAGG_EXIT = 3
DISAGG_MMAP = 5
DISAGG_BRK = 6
DISAGG_MUNMAP = 7
DISAGG_MREMAP = 8
DISAGG_PFAULT = 11
DISAGG_DATA_PUSH = 21
DISAGG_RDMA = 51
DISAGG_FIN_CONN = 99

STR_MMAP = 'mmap'
STR_BRK = 'brk'

# type of page fault messages
DISAGG_FAULT_WRITE = 1
DISAGG_FAULT_READ = 2

# memory node set-up
num_memory_node = 1

# optional
byte_to_mb = float(1. / 1024 / 1024)
_2mb = (2 * 1024 * 1024)

control_opcodes = set(['brk', 'mmap', 'unmap'])
data_opcodes = set(['read', 'write'])

class Parser:
    def __init__(self):
        pass

    def parse_file(self, filename, va_size, mn_num, print_stat=True, enforce_fix_addr=False):
        try:
            alloc_inst_list = []
            with open(filename, newline='') as csvfile:
                reader = csv.reader(csvfile, delimiter=',', quotechar='|', skipinitialspace=True)

                for row in reader:
                    row = str(row[0]).split(' ')
                    opcode = row[0]
                    # the opcode
                    if opcode == STR_BRK:
                        length = int(row[1])
                        permission = str(row[2])
                        alloc_inst_list.append({'opcode': 'brk', 'len': length, 'given_addr': 0, 'flag': permission})

                    elif opcode == STR_MMAP:
                        length = int(row[1])
                        file_backed = True if int(row[2]) > 0 else False
                        hint_addr = int(row[3], base=16)
                        if enforce_fix_addr:
                            given_addr = hint_addr
                        elif file_backed:
                            given_addr = hint_addr
                        else:
                            given_addr = 0  # just a hint for anon (not fixed)
                        permission = str(row[4])
                        alloc_inst_list.append({'opcode': 'mmap', 'len': length,
                                                'given_addr': given_addr, 'file_backed': file_backed,
                                                'flag': permission})

                    elif opcode == DISAGG_MUNMAP:
                        pass

                    elif opcode == DISAGG_PFAULT:
                        if int(row[2]) == DISAGG_FAULT_READ:
                            addr = int(row[3], base=16) & 0xFFFFFFFFFFFF000
                            alloc_inst_list.append({'opcode': 'read', 'address': addr})
                        elif int(row[2]) == DISAGG_FAULT_WRITE:
                            addr = int(row[3], base=16) & 0xFFFFFFFFFFFF000
                            alloc_inst_list.append({'opcode': 'write', 'address': addr})

                    elif opcode == DISAGG_EXIT:
                        addr = [int(row[2], base=16), int(row[3], base=16)]
                        alloc = bool(row[5] != '0')
                        file = bool(row[6] != '0')
                        alloc_inst_list.append({'opcode': 'vma', 'address': addr, 'alloc': alloc, 'file': file})

                    else:
                        print('|'.join(row))
            if enforce_fix_addr:  # initial mappings as fixed address
                last_addr = va_size
                for idx in range(len(alloc_inst_list)):
                    if alloc_inst_list[idx]['opcode'] == 'mmap' and alloc_inst_list[idx]['given_addr'] == 0:
                        if alloc_inst_list[idx]['len'] > _2mb:
                            allocated_len = alloc_inst_list[idx]['len'] - (alloc_inst_list[idx]['len'] % _2mb) + _2mb
                        else:
                            allocated_len = _2mb
                        last_addr -= allocated_len
                        alloc_inst_list[idx]['given_addr'] = last_addr
            for idx in range(len(alloc_inst_list)):
                if alloc_inst_list[idx]['flag'] == '--':
                    if enforce_fix_addr:
                        alloc_inst_list[idx]['len'] = 0  # just hole, do not assign
                    else:
                        alloc_inst_list[idx]['flag'] = 'rw'  # memory alloc requests in runtime

            if print_stat:
                self.print_statistics(filename, alloc_inst_list)
            alloc_inst_list = sorted(alloc_inst_list, key=lambda entry: entry['given_addr'], reverse=True)
            return alloc_inst_list

        except FileNotFoundError:
            print("Cannot find the file: " + filename)
            return None

    @staticmethod
    def _find_vma(addr, vma_list):
        for ops in vma_list:
            if ops['address'][0] <= addr < ops['address'][1]:
                if not ops['alloc']:
                    print("Err: Access to the un-allocated VMA")
                return True

        return False

    def print_statistics(self, filename, alloc_inst_list):
        control_counter = 0
        stat_counters = {'vma_total': 0, 'vma_file': 0, 'vma_access': 0, 'vma_fixed_anon': 0, 'vma_file_alloc': 0,
                         'vma_alloc_total': 0, 'size_alloc_total': 0, 'size_anon': 0, 'size_file': 0}

        for ops in alloc_inst_list:
            if ops['opcode'] in control_opcodes:
                control_counter += 1
                if ops['opcode'] == 'mmap':
                    stat_counters['vma_alloc_total'] += 1
                    if ops['file_backed']:
                        stat_counters['size_file'] += ops['len']
                        stat_counters['vma_file_alloc'] += 1
                    else:
                        stat_counters['size_anon'] += ops['len']
                        if ops['given_addr'] > 0:
                            stat_counters['vma_fixed_anon'] += 1
                elif ops['opcode'] == 'brk':
                    stat_counters['size_anon'] += ops['len']
                    stat_counters['vma_alloc_total'] += 1
                stat_counters['size_alloc_total'] += ops['len']

        # remaining stats
        stat_counters['vma_anon_alloc'] = stat_counters['vma_alloc_total'] - stat_counters['vma_file_alloc']
        total_size_in_mb = stat_counters['size_anon'] / 1024. / 1024.
        pg_size_in_entry = stat_counters['size_anon'] / 4096 / 1024. / 1024.

        print("\n*** Stat for " + filename + " ***")
        # total number of allocations
        print("— Total: %d (%.2f MB), if fixed 4 KB pages: %.2f million entries" %
              (stat_counters['vma_alloc_total'], total_size_in_mb, pg_size_in_entry))
        # Allocation with fixed address would be exceptional entries
        print("- Alloc in partitions: %d" % stat_counters['vma_anon_alloc'])
        pass
