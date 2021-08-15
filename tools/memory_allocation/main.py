from Allocators.UVAAllocator import UVAAllocator
from Allocators.FirstFitAllocator import FirstFitAllocator
from Allocators.SizeBalancingAllocator import SizeBalancingAllocator
from Parser import Parser
from Allocators.PAVAAllocator import PAVAAllocator
from Allocators.OptimalLPMUtils import OptimalLPMUtils
import argparse
import os


def print_load_balancing(algo, mn_num):
    size_list = SizeBalancingAllocator.get_size_list_from_alloc_list(algo.mapping, mn_num, algo.granularity)
    proc_size_list = SizeBalancingAllocator.get_process_size_list_from_alloc_list(algo.mapping, mn_num, algo.granularity)

    # We will use Jain's fairness here
    tot_size = 0
    tot_size_sqr = 0

    for entry in size_list:
        tot_size += entry
        tot_size_sqr += entry * entry

    fairness = tot_size * tot_size / (mn_num * tot_size_sqr)
    print("* Jain's fairness index: %.5lf" % fairness)
    pass


def do_power_of_two_aggregation(algo, mn_num, verbose=False):     # prefix matching
    agg_addr_rules = OptimalLPMUtils.get_optimal_alloc(algo.mapping, type(algo.va_alloc) == UVAAllocator)
    print("* Aggregation for prefix-match")
    if type(algo.va_alloc) == UVAAllocator:
        # With verbose=True, it will print out allocated memory lists
        exact_protection_rules = OptimalLPMUtils.get_power_of_two_rules(algo.mapping, verbose=verbose)
        # Results
        print("  - Address translation entries: ", agg_addr_rules)
        print("  - Memory protection entries:", exact_protection_rules)
        # This is the value after optimization of merging exceptional tables and protection tables
        # => current implementation in the switch
        print("  - [Result] MIND entries:", (mn_num + max(agg_addr_rules - mn_num, exact_protection_rules)))
    else:
        print("  - [Result] Address translation / protection entries: ", agg_addr_rules)
    pass


def print_estimation_result(algo, mn_num, verbose=False):

    print("\n==Simulation results==")
    print("", type(algo.pa_alloc).__name__, "&", type(algo.va_alloc).__name__,
          "— granularity:", algo.granularity, ", second-layer-pgtable:", algo.second_layer)
    print_load_balancing(algo, mn_num)
    do_power_of_two_aggregation(algo, mn_num, verbose=verbose)


if __name__ == '__main__':
    # a) args
    parser = argparse.ArgumentParser()
    parser.add_argument('--file_name', type=str, help='name of the log file for runtime mappings')
    parser.add_argument('--init_file_name', type=str, help='name of the log file for initial mappings', default='test')
    parser.add_argument('--mem_size', type=int, help='size of physical memory per memory node', default=0xF00000000)
    parser.add_argument('--va_size', type=int, help='total size of virtual memory', default=0xF000000000)
    parser.add_argument('--mn_num', type=int, help='number of disaggregated memory node', default=8) # up to 16 (4 bits)
    parser.add_argument('--skip_simulation', type=bool, help='skip simulation and do analysis only', default=False)
    parser.add_argument('--second_layer_pgtable', type=bool, help='is there a second-layer page table?', default=False)
    parser.add_argument('--verbose', type=bool, help='print detailed statistics on logs', default=False)
    args = parser.parse_args()
    print("Input file: " + args.file_name)

    # b) files
    logParser = Parser()
    run_file = 'input/' + args.file_name
    init_file = 'input/' + args.init_file_name
    file_list = {'runtime': [], 'init': []}
    if os.path.isfile(run_file):
        file_list['runtime'] = [run_file]
    if os.path.isfile(init_file):
        file_list['init'] = [init_file]

    # Major for loop
    for fidx, filename in enumerate(file_list['runtime']):
        if len(file_list['init']) <= fidx:
            print('Cannot find init mappings for: ', filename)
            continue
        initfile_name = file_list['init'][fidx]

        # 1) Parse the input to generate list of requests
        inst_list = {}
        inst_list['runtime'] = logParser.parse_file(filename, args.va_size, args.mn_num, args.verbose)
        # All init mappings are forced to be fixed (we cannot remap those mappings)
        inst_list['init'] = logParser.parse_file(initfile_name, args.va_size, args.mn_num,
                                                 args.verbose, enforce_fix_addr=True)
        if inst_list is None or args.skip_simulation:
            continue
        merged_inst = inst_list['init'] + inst_list['runtime']

        # 2) Allocate algorithms
        algos = [
                 # NOTE)
                 # Since it takes too much time to process allocations in 2MB granularity, it is disabled by default
                 # It is straightforward that 2MB pages would have almost perfect load balancing
                 # with very large amount of entries (which can be calculated by total allocation size / 2MB)

                 # 2MB page allocation (with load balancing)
                 # PAVAAllocator(pa_alloc=SizeBalancingAllocator(args.mem_size * args.mn_num, args.mn_num),
                 #               va_alloc=FirstFitAllocator(args.va_size, args.mn_num),
                 #               granularity=2 * 1024 * 1024, second_layer=False),
                 # 1 GB huge page (with load balancing)
                 PAVAAllocator(pa_alloc=SizeBalancingAllocator(args.mem_size * args.mn_num, args.mn_num),
                               va_alloc=FirstFitAllocator(args.va_size, args.mn_num),
                               granularity=1024 * 1024 * 1024, second_layer=True),
                 # Proposed: first-fit + pre partitioning + aggregation for TCAM match
                 PAVAAllocator(pa_alloc=SizeBalancingAllocator(args.mem_size * args.mn_num, args.mn_num),
                               va_alloc=UVAAllocator(args.va_size, args.mn_num))
                 ]

        # 3) Process captured syscalls
        for algo in algos:
            pid = 0
            for inst_idx, inst in enumerate(merged_inst):
                if inst['opcode'] == 'mmap' or inst['opcode'] == 'brk':
                        algo.allocate(pid, inst['len'], inst['flag'],
                                      # since brk only extend the existing VMA, allocation always can be merged with
                                      # the existing mapping
                                      given_addr=0 if inst['opcode'] == 'brk' else inst['given_addr'],
                                      power_of_two=True)
            # Allocation of all instructions is finished
            algo.post_allocate(pid)

        # 4) print out load balance and result
        for algo in algos:
            print_estimation_result(algo, args.mn_num, args.verbose)
        pass
