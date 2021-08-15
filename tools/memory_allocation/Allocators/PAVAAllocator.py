from Allocators.OptimalLPMUtils import OptimalLPMUtils

_mind_alloc_limit = 1 * 1024 * 1024 * 1024


class PAVAAllocator:
    def __init__(self, pa_alloc, va_alloc, granularity=0, second_layer=False):
        self.pa_alloc = pa_alloc
        self.va_alloc = va_alloc
        self.mapping = {0: []}   # Sorted by ascending order of virtual address
        self.pa_alloc_list = []
        self.granularity = granularity
        self.coarse_pgtable = []
        self.second_layer = second_layer
        pass

    def allocate(self, pid=0, size=0, flag='rw', force_alloc=False, given_addr=0, power_of_two=False):
        new_map = None
        pa = None
        if size > 0:
            # Check given addr
            if given_addr > 0:
                force_alloc = True

            # Check hierarchical allocation
            if self.second_layer and not force_alloc:
                find_idx = -1
                for cidx in range(len(self.coarse_pgtable)):
                    if self.coarse_pgtable[cidx]['flag'] == flag:
                        # Aggregate allocations in given granularity
                        if self.coarse_pgtable[cidx]['size'] + size <= self.granularity:
                            find_idx = cidx
                            break
                if find_idx >= 0:
                    self.coarse_pgtable[find_idx]['size'] += size
                else:
                    new_rec = {'size': size, 'flag': flag}
                    self.coarse_pgtable.append(new_rec)
                return new_map

            alloc_size_list = []
            if self.granularity > 0:
                allocated = 0
                while allocated < size:
                    ssize = min(self.granularity, size - allocated)
                    alloc_size_list.append(ssize)    # Allocate in granularity
                    allocated += ssize
            elif given_addr == 0 and power_of_two:
                if size <= _mind_alloc_limit:
                    ssize = OptimalLPMUtils.last_power_of_two(size)
                    if size > ssize:
                        ssize *= 2
                    alloc_size_list.append(ssize)
                else:
                    remain = size
                    while remain > _mind_alloc_limit:
                        alloc_size_list.append(_mind_alloc_limit)
                        remain -= _mind_alloc_limit
                    if remain > 0:
                        ssize = OptimalLPMUtils.last_power_of_two(remain)
                        if remain > ssize:
                            ssize *= 2
                        alloc_size_list.append(size - remain + ssize)
                    else:
                        alloc_size_list.append(size)
            else:
                alloc_size_list.append(size)

            # print("** Allocation size: %d, mapping size: %d" % (len(alloc_size_list), len(self.mapping[pid])))
            addr_in_granularity = 0
            if self.granularity > 0 and given_addr > 0:
                addr_in_granularity = given_addr - int(given_addr % self.granularity)
                for map_rec in self.mapping[pid]:
                    if map_rec['va']['start'] == addr_in_granularity:   # existing mapping
                        if map_rec['flag'] == flag:
                            return new_map
                        else:
                            addr_in_granularity += self.granularity

            for sidx, ss in enumerate(alloc_size_list):
                # Make sure that there is a mapping list
                if (pid not in self.mapping.keys()) or (self.mapping[pid] is None):
                    self.mapping[pid] = list()
                pa = self.pa_alloc.get_next_address(pid, ss, self.mapping)
                if pa is not None:
                    va = self.va_alloc.get_next_address(pid, ss, self.mapping, pa)
                    if va is not None:
                        if given_addr > 0:
                            if self.granularity > 0:
                                va['start'] = addr_in_granularity
                                va['end'] = addr_in_granularity + ((sidx + 1) * self.granularity)
                            else:
                                va['start'] = given_addr
                                va['end'] = given_addr + ss
                        new_map = {'va': va, 'pa': pa, 'flag': flag, 'given': given_addr}
                        # print(new_map)
                        self.mapping[pid].append(new_map)
                        # sort mapping
                        self.mapping[pid] = sorted(self.mapping[pid], key=lambda entry: entry['va']['start'])
                    else:
                        raise RuntimeError("Cannot allocate virtual address")
                else:
                    raise RuntimeError("Cannot allocate physical address")
        return new_map

    def post_allocate(self, pid=0):
        # Process remaining allocation
        if self.second_layer and len(self.coarse_pgtable) > 0:
            for map_rec in self.coarse_pgtable:
                self.allocate(pid, map_rec['size'], map_rec['flag'], force_alloc=True)
            self.coarse_pgtable = []

    def free(self, pid=0, start=0, end=0):
        if (pid not in self.mapping.keys()) or (self.mapping[pid] is None):
            return

        for i in range(0, len(self.mapping[pid])):
            if self.mapping[pid][i]['va']['start'] >= end:
                pass

            if self.mapping[pid][i]['va']['end'] <= start:
                pass

            # If it has a overlapped region
            free_range = [max(self.mapping[pid][i]['va']['start'], start), min(self.mapping[pid][i]['va']['end'], end)]
            remaining = []

            # Remaining head
            if free_range[0] > self.mapping[pid][i]['va']['start']:
                va = {'start': self.mapping[pid][i]['va']['start'], 'end': free_range[0]}

                pa = {'start': self.mapping[pid][i]['pa']['start'],
                      'end': self.mapping[pid][i]['pa']['start'] + free_range[0] - self.mapping[pid][i]['va']['start'],
                      'mn_id': self.mapping[pid][i]['pa']['mn_id']}

                remaining.append({'pa': pa, 'va': va})

            # Remaining tail
            if free_range[1] < self.mapping[pid][i]['va']['end']:
                va = {'start': free_range[1], 'end': self.mapping[pid][i]['va']['end']}

                pa = {'start': self.mapping[pid][i]['pa']['end'] - (self.mapping[pid][i]['va']['end'] - free_range[1]),
                      'end': self.mapping[pid][i]['pa']['end'],
                      'mn_id': self.mapping[pid][i]['pa']['mn_id']}

                remaining.append({'pa': pa, 'va': va})

            if len(remaining) > 0:
                self.mapping[pid] = self.mapping[pid][:i] + remaining + self.mapping[pid][i + 1:]
            else:
                self.mapping[pid] = self.mapping[pid][:i] + self.mapping[pid][i + 1:]

