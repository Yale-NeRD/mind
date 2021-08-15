from Allocators.VirtualAddressAllocator import VirtualAddressAllocator


class UVAAllocator(VirtualAddressAllocator):
    def __init__(self, total_mem_size, mn_num=1):
        super().__init__(total_mem_size, mn_num)
        self.uva_region_size = self.total_size // self.mn_num

    def get_next_address(self, pid, size, _alloc_list, hint):
        range_start = self.uva_region_size * hint['mn_id']
        range_end = range_start + self.uva_region_size
        first_in_range = True
        alloc_list = _alloc_list[pid]

        # Check whether the given pa is good
        target_va = range_start + hint['start']
        pa_ok = True
        for entry in alloc_list:
            if entry['pa']['mn_id'] == hint['mn_id']:
                if ((entry['va']['start'] < target_va + size) and (entry['va']['end'] > target_va)) \
                        or ((entry['va']['start'] > target_va) and (entry['va']['end'] < target_va + size)):
                    pa_ok = False
                    break

        if pa_ok:
            return {'start': target_va, 'end': target_va + size}

        # No entry after range_start
        if len(alloc_list) == 0 or alloc_list[-1]['va']['end'] <= range_start:
            if range_end - range_start >= size:
                return {'start': range_start, 'end': range_start + size}
            else:
                return None

        # Check existing entries
        for i, entry in enumerate(alloc_list):
            if entry['va']['end'] <= range_start:
                continue

            if entry['va']['start'] >= range_end:
                if first_in_range:  # Still have not met any previous mappings in the range
                    if range_end - range_start >= size:
                        return {'start': range_start, 'end': range_start + size}
                else:
                    prev_end = alloc_list[i - 1]['va']['end']
                    if range_end - prev_end >= size:
                        return {'start': prev_end, 'end': prev_end + size}
                return None

            if first_in_range:
                if entry['va']['start'] - range_start >= size:
                    return {'start': range_start, 'end': range_start + size}
                first_in_range = False
                continue

            if entry['va']['start'] - alloc_list[i - 1]['va']['end'] >= size:
                return {'start': alloc_list[i - 1]['va']['end'],
                        'end': alloc_list[i - 1]['va']['end'] + size}

        # Check the last one
        if not first_in_range:
            if range_end - alloc_list[-1]['va']['end'] >= size:
                return {'start': alloc_list[-1]['va']['end'],
                        'end': alloc_list[-1]['va']['end'] + size}
        else:
            raise AttributeError  # bug

        return None
