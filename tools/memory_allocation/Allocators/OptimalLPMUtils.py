import math


class TrieNode:
    def __init__(self, left, right, label):
        self.left = left
        self.right = right
        self.label = label


class OptimalLPMUtils:
    def __init__(self):
        pass

    @staticmethod
    def fill_child(trie_node):
        if trie_node.left is None:
            trie_node.left = TrieNode(None, None, trie_node.label)
        else:
            if trie_node.label != -1 and trie_node.left.label == -1:
                trie_node.left.label = trie_node.label
            OptimalLPMUtils.fill_child(trie_node.left)

        if trie_node.right is None:
            trie_node.right = TrieNode(None, None, trie_node.label)
        else:
            if trie_node.label != -1 and trie_node.right.label == -1:
                trie_node.right.label = trie_node.label
            OptimalLPMUtils.fill_child(trie_node.right)

    @staticmethod
    def calculate_potential_hop(trie_node):
        if trie_node.left is None and trie_node.right is None:
            if trie_node.label == -1:
                trie_node.label = set()
            else:
                trie_node.label = set([trie_node.label])
            return trie_node.label

        left_set = OptimalLPMUtils.calculate_potential_hop(trie_node.left)
        right_set = OptimalLPMUtils.calculate_potential_hop(trie_node.right)
        intersect = left_set.intersection(right_set)
        if intersect == set():   # no intersection
            trie_node.label = left_set.union(right_set)
        else:
            trie_node.label = intersect
        return trie_node.label

    @staticmethod
    def choose_hop(trie_node, inherit):
        if trie_node.label == set():
            trie_node.label = None
        elif trie_node.label.intersection(inherit) == inherit:   # subset
            trie_node.label = None
        else:
            trie_node.label = trie_node.label.pop()
            inherit = set([trie_node.label])

        if trie_node.left is not None:
            OptimalLPMUtils.choose_hop(trie_node.left, inherit)

        if trie_node.right is not None:
            OptimalLPMUtils.choose_hop(trie_node.right, inherit)

    @staticmethod
    def count_routing_rules(trie_node, prefix=""):
        num = 0
        if trie_node.label is not None:
            num += 1
            # print(prefix, "->", trie_node.label)

        if trie_node.left is not None:
            num += OptimalLPMUtils.count_routing_rules(trie_node.left, prefix+"0")

        if trie_node.right is not None:
            num += OptimalLPMUtils.count_routing_rules(trie_node.right, prefix+"1")

        return num

    @staticmethod
    def last_power_of_two(size):
        return 2 ** int(math.log2(size))

    @staticmethod
    def calculate_power_of_two_rules(size):
        # Assume that size is already page aligned
        atleast_size = OptimalLPMUtils.last_power_of_two(size)
        if size == atleast_size:
            return 1
        else:
            return 1 + OptimalLPMUtils.calculate_power_of_two_rules(size - atleast_size)

    @staticmethod
    def get_power_of_two_rules(alloc_list_pids, verbose=False):
        alloc_list = []
        for pid in alloc_list_pids:
            for entry in alloc_list_pids[pid]:
                entry['pid'] = pid
                alloc_list.append(entry)
        # Assuming already sorted list
        idx = 0
        while idx < len(alloc_list) - 1:
            cur_node = alloc_list[idx]
            next_node = alloc_list[idx+1]
            if cur_node['va']['end'] == next_node['va']['start'] and cur_node['flag'] == next_node['flag']:
                if cur_node['pa']['mn_id'] == next_node['pa']['mn_id']:
                    # Merge
                    alloc_list[idx]['pa']['end'] += (next_node['pa']['end'] - next_node['pa']['start'])
                    alloc_list[idx]['va']['end'] = next_node['va']['end']
                    del alloc_list[idx+1]
                    continue
            idx += 1

        sum_rules = 0
        for entry in alloc_list:
            ssize = entry['va']['end'] - entry['va']['start']
            if ssize > 1:
                rule_size = OptimalLPMUtils.calculate_power_of_two_rules(ssize)
                sum_rules += rule_size
                if verbose:
                    print(entry, "::", rule_size)
        return sum_rules

    @staticmethod
    def get_optimal_alloc(alloc_list_pids, separate_protection_table=True):
        # alloc_list will be: ['pa': {'start', 'end', 'mn_i'}, 'va': {'start', 'end}]
        # We are going to aggregate route from 'va' to 'mn_id'
        root = TrieNode(None, None, -1)  # empty root
        alloc_list = []
        for pid in alloc_list_pids:
            for entry in alloc_list_pids[pid]:
                entry['pid'] = pid
                alloc_list.append(entry)

        print("* Total allocations in given granularity: %d" % len(alloc_list))

        # Generate basic trie for given entries
        for entry in alloc_list:
            # We assume non overlapping va
            if separate_protection_table:
                entry_str = "{0:044b}".format(entry['va']['start'])
            else:
                entry_str = "{0:044b}".format((1 if entry['flag'] == 'rw' else 0) * (2**43) + entry['pid'] * (2**40)
                                              + entry['va']['start'])

            cur_node = root
            for bit in entry_str:
                if bit == '0':
                    if cur_node.left is None:
                        cur_node.left = TrieNode(None, None, -1)
                    cur_node = cur_node.left
                elif bit == '1':
                    if cur_node.right is None:
                        cur_node.right = TrieNode(None, None, -1)
                    cur_node = cur_node.right
            if cur_node.label >= 0:
                if cur_node.label != entry['pa']['mn_id']:
                    pass    # duplicated entry
            cur_node.label = entry['pa']['mn_id']

        # Pass 1
        OptimalLPMUtils.fill_child(root)

        # Pass 2
        OptimalLPMUtils.calculate_potential_hop(root)

        # Pass 3
        root.label = root.label.pop()
        if root.left is not None:
            OptimalLPMUtils.choose_hop(root.left, set([root.label]))
        if root.right is not None:
            OptimalLPMUtils.choose_hop(root.right, set([root.label]))

        num_rules = OptimalLPMUtils.count_routing_rules(root)
        return num_rules

