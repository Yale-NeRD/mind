from Allocators.PhysicalAddressAllocator import PhysicalAddressAllocator


class SizeBalancingAllocator(PhysicalAddressAllocator):
    def __init__(self, total_pa_size, mn_num=1):
        super().__init__(total_pa_size, mn_num)

    @staticmethod
    def get_size_list_from_alloc_list(alloc_list, mn_num, granularity=0):
        size_list = [0] * mn_num
        for (k, alloc_list_k) in alloc_list.items():
            for entry in alloc_list_k:
                if 0 <= entry['pa']['mn_id'] < mn_num:
                    if granularity > 0:
                        size_list[entry['pa']['mn_id']] += granularity
                    else:
                        size_list[entry['pa']['mn_id']] += entry['pa']['end'] - entry['pa']['start']
        return size_list

    @staticmethod
    def get_process_size_list_from_alloc_list(alloc_list, mn_num, granularity=0):
        proc_size_list = [[0] * mn_num for _ in range(len(alloc_list.items()))]
        for (k, alloc_list_k) in alloc_list.items():
            for entry in alloc_list_k:
                if 0 <= entry['pa']['mn_id'] < mn_num:
                    if granularity > 0:
                        proc_size_list[k][entry['pa']['mn_id']] += granularity
                    else:
                        proc_size_list[k][entry['pa']['mn_id']] += entry['pa']['end'] - entry['pa']['start']
        return proc_size_list

    def get_next_address(self, pid, size, alloc_list):
        size_list = self.get_size_list_from_alloc_list(alloc_list, self.mn_num)
        min_size_entry = min(size_list)
        min_size_index = size_list.index(min_size_entry)
        next_pa = 0
        if len(alloc_list) > 0:
            for alloc_keys in alloc_list.keys():
                for alloc_entry in alloc_list[alloc_keys]:
                    if alloc_entry['pa']['mn_id'] == min_size_index:
                        if alloc_entry['pa']['end'] > next_pa:
                            next_pa = alloc_entry['pa']['end']

        if 0 <= min_size_index < self.mn_num:
            return {'start': next_pa, 'end': next_pa + size, 'mn_id': min_size_index}

        # Default
        return {'start': 0, 'end': size, 'mn_id': 0}
