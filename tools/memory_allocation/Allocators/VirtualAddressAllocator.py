class VirtualAddressAllocator:
    def __init__(self, total_va_size, mn_num=1):
        self.total_size = total_va_size
        self.mn_num = mn_num

    def get_next_address(self, pid, size, alloc_list, hint):
        pass

