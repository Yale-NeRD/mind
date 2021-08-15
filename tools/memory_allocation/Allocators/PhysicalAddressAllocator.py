class PhysicalAddressAllocator:
    def __init__(self, total_pa_size, mn_num=1):
        self.total_size = total_pa_size
        self.mn_num = mn_num

    def get_next_address(self, pid, size, alloc_list):
        # base class: DUMMY
        entry = {'start': 0, 'end': size, 'mn_id': 0}
        return entry
