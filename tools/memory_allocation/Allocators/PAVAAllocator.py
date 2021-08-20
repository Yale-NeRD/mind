from Allocators.OptimalLPMUtils import OptimalLPMUtils
import math

_mind_alloc_limit = 8 * 1024 * 1024 * 1024


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
                ssize = int(math.ceil(size / self.granularity) * self.granularity)
                alloc_size_list.append(ssize)
                # allocated = 0
                # while allocated < size:    
                #     ssize = min(self.granularity, size - allocated)
                #     alloc_size_list.append(ssize)    # Allocate in granularity
                #     allocated += ssize
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

            for _, ss in enumerate(alloc_size_list):
                # Make sure that there is a mapping list
                if (pid not in self.mapping.keys()) or (self.mapping[pid] is None):
                    self.mapping[pid] = list()

                pa = self.pa_alloc.get_next_address(pid, ss, self.mapping)
                if pa is not None:
                    if self.granularity > 0 and given_addr > 0:
                        # find first avaiable after given_addr
                        va = {}
                        va['start'] = int(math.ceil(given_addr / self.granularity) * self.granularity)
                        for map_rec in self.mapping[pid]:
                            if (map_rec['va']['start'] < va['start'] + ss)\
                                    and va['start'] < map_rec['va']['end']:   # overlapping mapping
                                # check same size & same flag => same mapping
                                if map_rec['flag'] == flag and ss == (map_rec['va']['end'] - map_rec['va']['start']):
                                    va = None
                                    break   # to go to next for loop of alloc_size_list
                                else:
                                    va['start'] = map_rec['va']['end']  # try next mapping
                        if va is not None:
                            va['end'] = va['start'] + ss    # not needed, actually
                    else:
                        va = self.va_alloc.get_next_address(pid, ss, self.mapping, pa)

                    if va is not None:
                        allocated = 0
                        alloc_cnt = 0
                        while allocated < ss:
                            _va = {}
                            if self.granularity > 0:
                                _va['start'] = va['start'] + (alloc_cnt * self.granularity)
                                _va['end'] = _va['start'] + self.granularity
                                allocated += self.granularity
                            else:
                                _va['start'] = va['start']
                                _va['end'] = va['start'] + ss
                                allocated += ss
                            new_map = {'va': _va, 'pa': pa, 'flag': flag, 'given': given_addr}
                            self.mapping[pid].append(new_map)
                            alloc_cnt += 1
                        # sort mapping
                        self.mapping[pid] = sorted(self.mapping[pid], key=lambda entry: entry['va']['start'])
                    else:
                        pass    # skip overlapping VA regions
                else:
                    raise RuntimeError("Cannot allocate physical address")
        return new_map

    def post_allocate(self, pid=0):
        # Process remaining allocation
        if self.second_layer and len(self.coarse_pgtable) > 0:
            for map_rec in self.coarse_pgtable:
                self.allocate(pid, map_rec['size'], map_rec['flag'], force_alloc=True)
            self.coarse_pgtable = []
