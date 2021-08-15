from Allocators.VirtualAddressAllocator import VirtualAddressAllocator


class FirstFitAllocator(VirtualAddressAllocator):
    def __init__(self, total_mem_size, mn_num=1):
        super().__init__(total_mem_size, mn_num)

    def get_next_address(self, pid, size, _alloc_list, hint):
        if size > self.total_size:
            return None

        alloc_list = _alloc_list[pid]
        if len(alloc_list) == 0:
            entry = {'start': 0, 'end': size}
            return entry

        if alloc_list[0]['va']['start'] >= size:
            entry = {'start': 0, 'end': size}
            return entry

        for i in range(1, len(alloc_list)):
            if alloc_list[i]['va']['start'] - alloc_list[i-1]['va']['end'] >= size:
                entry = {'start': alloc_list[i-1]['va']['end'],
                         'end': alloc_list[i-1]['va']['end'] + size}
                return entry

        if self.total_size - alloc_list[-1]['va']['end'] >= size:
            entry = {'start': alloc_list[-1]['va']['end'],
                     'end': alloc_list[-1]['va']['end'] + size}
            return entry

        return None
