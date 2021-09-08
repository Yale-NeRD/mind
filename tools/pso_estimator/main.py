import argparse
import os
import numpy as np
from copy import deepcopy
from multiprocessing import Process, Manager
import math

profile_overhead = float(0.1)   # in us
mem_lat = float(0.01)         # can be impacted by workload's locality, L3: ~40 cycles (~10ns), DRAM: < 15ns
# TF: 0.0081 for 10T,
_num_cdf_bucket = int(512)

class bcolors:
    HEADER = '\033[95m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'

def cdf_idx_to_latency(idx):
    if idx < 1:
        return mem_lat
    elif idx < 100:
        return max(mem_lat, idx - profile_overhead)
    elif idx < 190:
        return ((idx - (100 - .5)) * 10) + 100 - profile_overhead
    elif idx < 280:
        return ((idx - (190 - .5)) * 100) + 1000 - profile_overhead
    elif idx < 370:
        return ((idx - (280 - .5)) * 1000) + 10000 - profile_overhead
    elif idx < 460:
        return ((idx - (370 - .5)) * 10000) + 100000 - profile_overhead
    else:
        return 1000000  # 1 sec


def latency_to_cdf_idx(lat):
    if lat < 100:
        return int(lat)
    elif lat < 1000:
        return int(100 + ((int(lat) - 100) / 10))
    elif lat < 10000:
        return int(190 + ((int(lat) - 1000) / 100))
    elif lat < 100000:
        return int(280 + ((int(lat) - 10000) / 1000))
    elif lat < 1000000:
        return int(370 + ((int(lat) - 100000) / 10000))
    else:
        return _num_cdf_bucket - 1


def find_sim_impl_ratio(stat, impl_tot_key, impl_key, sim_key):
    # find local to remote ratio here
    stat[impl_tot_key] = stat[impl_key][0]
    stat_idx = int(1)
    while stat[impl_tot_key] + stat[impl_key][stat_idx] < stat[sim_key]:
        stat[impl_tot_key] += stat[impl_key][stat_idx]
        stat_idx += 1
    stat_ratio = (stat[sim_key] - stat[impl_tot_key]) / stat[impl_key][stat_idx]
    stat[impl_tot_key] += stat[impl_key][stat_idx] * stat_ratio
    return stat_idx, stat_ratio


def analysis_stat(stat, remote_start_idx, remote_adjust):
    stat['total_read'] = int(0)
    stat['total_write'] = int(0)

    for read_access in stat['read']:
        stat['total_read'] += read_access
    for write_access in stat['write']:
        stat['total_write'] += write_access
    stat['total_access'] = stat['total_read'] + stat['total_write']
    low_idx = int(math.floor(remote_start_idx))
    local_ratio = float(remote_start_idx - low_idx)
    read_low_idx = write_low_idx = low_idx
    read_ratio = write_ratio = local_ratio
    # read_low_idx, read_ratio = find_sim_impl_ratio(stat, 'total_local_read', 'read', 'sim_local_read')
    # write_low_idx, write_ratio = find_sim_impl_ratio(stat, 'total_local_write', 'write', 'sim_local_write')
    stat['total_local_read'] = sum(stat['read'][0:low_idx]) + (stat['read'][low_idx] * local_ratio)
    stat['total_local_write'] = sum(stat['write'][0:low_idx]) + (stat['write'][low_idx] * local_ratio)
    for idx, var in enumerate(stat['thread_read']):
        stat['thread_rw'][idx].append(sum(stat['thread_read'][idx][0:low_idx])
                                      + (stat['thread_read'][idx][low_idx] * local_ratio))
        stat['thread_rw'][idx].append(sum(stat['thread_write'][idx][0:low_idx])
                                      + (stat['thread_write'][idx][low_idx] * local_ratio))
        stat['thread_rw'][idx].append(sum(stat['thread_read'][idx][low_idx + 1:])
                                      + (stat['thread_read'][idx][low_idx] * (1. - local_ratio)))
        stat['thread_rw'][idx].append(sum(stat['thread_write'][idx][low_idx + 1:])
                                      + (stat['thread_write'][idx][low_idx] * (1. - local_ratio)))

    stat['total_remote_read'] = stat['total_read'] - stat['total_local_read']
    stat['total_remote_write'] = stat['total_write'] - stat['total_local_write']
    #
    stat['local_read_lat'] = 0
    stat['local_write_lat'] = 0
    stat['remote_read_lat'] = 0
    stat['remote_write_lat'] = 0
    for idx, read_access in enumerate(stat['read'][:read_low_idx]):
        stat['local_read_lat'] += (read_access * cdf_idx_to_latency(idx))
    stat['local_read_lat'] += stat['read'][read_low_idx] * read_ratio
    for idx, write_access in enumerate(stat['write'][:write_low_idx]):
        stat['local_write_lat'] += (write_access * cdf_idx_to_latency(idx))
    stat['local_write_lat'] += stat['write'][write_low_idx] * write_ratio
    for idx, read_access in enumerate(stat['read'][read_low_idx:]):
        stat['remote_read_lat'] += (read_access * cdf_idx_to_latency(idx + read_low_idx))
    stat['remote_read_lat'] += stat['read'][read_low_idx] * (1. - read_ratio)
    for idx, write_access in enumerate(stat['write'][write_low_idx:]):
        stat['remote_write_lat'] += (write_access * cdf_idx_to_latency(idx + write_low_idx))
    stat['remote_write_lat'] += stat['read'][write_low_idx] * (1. - write_ratio)
    if stat['total_local_read'] > 0:
        stat['local_read_lat'] /= float(stat['total_local_read'])
        stat['local_read_lat'] *= remote_adjust
    if stat['total_local_write'] > 0:
        stat['local_write_lat'] /= float(stat['total_local_write'])
        stat['local_write_lat'] *= remote_adjust
    if stat['total_remote_read'] > 0:
        stat['remote_read_lat'] /= float(stat['total_remote_read'])
        stat['remote_read_lat'] *= remote_adjust
    if stat['total_remote_write'] > 0:
        stat['remote_write_lat'] /= float(stat['total_remote_write'])
        stat['remote_write_lat'] *= remote_adjust


def print_stat(stat, ext_pass, ext_ratio):
    print("Local Read: %d accesses, %.5f us" % (stat['total_local_read'], stat['local_read_lat']))
    print("Remote Read: %d accesses, %.5f us" % (stat['total_remote_read'], stat['remote_read_lat']))
    print("Local Write: %d accesses, %.3f us" % (stat['total_local_write'], stat['local_write_lat']))
    print("Remote Write: %d accesses, %.3f us" % (stat['total_remote_write'], stat['remote_write_lat']))
    # print("Benefit: ", stat['remote_write_benefits'])
    # print("Est. time - PSO [%.2f]: " % (max(stat['est_time'])), stat['est_time'])
    # print("Est. time - SC  [%.2f]: " % (max(stat['est_time_sc'])), stat['est_time_sc'])
    print(bcolors.OKGREEN + "Est. Performance per blade - PSO for %d: [%f]"
            % (ext_pass, float(ext_pass) / (max(stat['est_time']) * ext_ratio))
            + bcolors.ENDC)
    print(bcolors.OKGREEN + "Est. Performance per blade - SC  for %d: [%f]"
            % (ext_pass, float(ext_pass) / (max(stat['est_time_sc']) * ext_ratio))
            + bcolors.ENDC)


def calculate_benefit_from_rec(loc_stat, rec):
    benefit = (int(rec[0]) * loc_stat['local_read_lat']) + (int(rec[1]) * loc_stat['local_write_lat'])
    benefit += (int(rec[2]) * loc_stat['remote_read_lat'])  # no remote write for now
    # return min(benefit, loc_stat['remote_write_lat'] * 2)
    return benefit



def calculate_est_time(stat, t_id):
    rec = stat['thread_rw'][t_id]
    est_time = (int(rec[0]) * stat['local_read_lat'])
    est_time += (int(rec[1]) * stat['local_write_lat'])
    est_time += (int(rec[2]) * stat['remote_read_lat'])
    est_time += (int(rec[3]) * stat['remote_write_lat'])
    stat['est_time_sc'][t_id] = est_time
    stat['est_time'][t_id] = est_time - stat['remote_write_benefits'][t_id]
                                        # (stat['remote_write_benefits'][t_id]
                                        #  * float(rec[3] / stat['rw_record'][t_id][3]))
    # print("Est[" + str(t_id) + "] .. done")
    pass


def parse_cdf_file(filep, tar_pass, stat, t_id):
    while True:
        data_line = filep.readline()
        if len(data_line) <= 0:
            break
        if data_line[0:4] == "Pass":
            pass_num = int(data_line[6:-1])
            if tar_pass == pass_num or tar_pass - 1 == pass_num:    # for some variant
                data_line = filep.readline()
                if data_line[:-1] == "Read:":
                    bkt_idx = 0
                    data_line = filep.readline()
                    while data_line[:-1] != "Write:":
                        stat['read'][bkt_idx] += int(data_line[:-1])
                        stat['thread_read'][t_id][bkt_idx] = int(data_line[:-1])
                        data_line = filep.readline()
                        bkt_idx += 1
                if data_line[:-1] == "Write:":
                    bkt_idx = 0
                    data_line = filep.readline()
                    while data_line != "\n":
                        stat['write'][bkt_idx] += int(data_line[:-1])
                        stat['thread_write'][t_id][bkt_idx] = int(data_line[:-1])
                        if int(data_line[:-1]) > 0:
                            pass
                        data_line = filep.readline()
                        bkt_idx += 1
                break
    filep.close()


def calculate_difference_cdf(unlimit_stat):
    unlimit_stat['cdf_diff'] = {}
    unlimit_stat['cdf_diff']['read'] = []
    unlimit_stat['cdf_diff']['write'] = []
    # unlimit_stat['cdf_diff']['thread_read'] = [[0 for _ in t_rec] for t_rec in unlimit_stat['unlimit']['thread_read']]
    # unlimit_stat['cdf_diff']['thread_write'] = [[0 for _ in t_rec] for t_rec in unlimit_stat['unlimit']['thread_write']]

    for acc_type in ['read', 'write']:
        pdf_sum = {'unlimit': sum(unlimit_stat['unlimit'][acc_type]), 'limit': sum(unlimit_stat['limit'][acc_type])}
        cdf_list = {'unlimit': [float(rec / pdf_sum['unlimit']) for rec in unlimit_stat['unlimit'][acc_type]],
                    'limit': [float(rec / pdf_sum['limit']) for rec in unlimit_stat['limit'][acc_type]]}
        # now we have cdf
        cur_unlimit_idx = 0
        for jdx in range(len(cdf_list['limit'])):   # over buckets
            remain = cdf_list['limit'][jdx]
            while remain > 0 and cur_unlimit_idx < len(cdf_list['unlimit']):
                if remain > cdf_list['unlimit'][cur_unlimit_idx]:
                    unlimit_stat['cdf_diff'][acc_type].append(
                        [cdf_list['unlimit'][cur_unlimit_idx], cur_unlimit_idx - jdx])
                    remain -= cdf_list['unlimit'][cur_unlimit_idx]
                    cdf_list['unlimit'][cur_unlimit_idx] = 0
                    cur_unlimit_idx += 1
                else:
                    cdf_list['unlimit'][cur_unlimit_idx] -= remain
                    unlimit_stat['cdf_diff'][acc_type].append([remain, cur_unlimit_idx - jdx])
                    remain = 0
                    break
        #     print(idx, end=' ', flush=True)
        # print('', flush=True)


def apply_unlimited_expectation(stat, unlimit_stat):
    new_cnt = {'read': [0 for _ in stat['read']], 'write': [0 for _ in stat['write']]}
    for acc_type in ['read', 'write']:
        for idx, stat_rec in enumerate(stat['thread_' + acc_type]):  # over thread
            new_dist = [0 for _ in stat_rec]
            pdf_sum = sum(stat['thread_' + acc_type][idx])
            pdf_list = [float(rec / pdf_sum) for rec in stat['thread_' + acc_type][idx]]
            cur_diff_idx = 0
            cdf_diff = deepcopy(unlimit_stat['cdf_diff'][acc_type])
            for jdx, rec in enumerate(stat['thread_' + acc_type][idx]):   # over bucket
                cur_prob = pdf_list[jdx]
                remain = cur_prob
                while remain > 0 and cur_diff_idx < len(cdf_diff):
                    if remain > cdf_diff[cur_diff_idx][0]:
                        new_dist[max(0, min(jdx + cdf_diff[cur_diff_idx][1], len(new_dist) - 1))] += \
                            float(cdf_diff[cur_diff_idx][0] / cur_prob) * stat['thread_' + acc_type][idx][jdx]
                        remain -= cdf_diff[cur_diff_idx][0]
                        cdf_diff[cur_diff_idx][0] = 0
                        cur_diff_idx += 1
                    else:
                        cdf_diff[cur_diff_idx][0] -= remain
                        new_dist[max(0, min(jdx + cdf_diff[cur_diff_idx][1], len(new_dist) - 1))] += \
                            float(remain / cur_prob) * stat['thread_' + acc_type][idx][jdx]
                        remain = 0
                        break
            stat['thread_' + acc_type][idx] = new_dist[:]
        #     print(idx, end=' ', flush=True)
        # print('', flush=True)
    # recount sum
    for acc_type in ['read', 'write']:
        for idx, read_num in enumerate(stat['thread_' + acc_type]):  # over thread
            for jdx, _ in enumerate(read_num):  # over bucket
                new_cnt[acc_type][jdx] += stat['thread_' + acc_type][idx][jdx]
    stat['read'] = new_cnt['read']
    stat['write'] = new_cnt['write']


def calculate_benefit_with_prob(stat, benefit_pdf, remote_start_idx, is_ideal=False):
    low_idx = int(math.floor(remote_start_idx))
    benefit_sum = sum(benefit_pdf)  # total number of benefit logs
    # stat['rw_record'][t_id][3]: number of total remote write
    for idx, val in enumerate(benefit_pdf):
        benefit_pdf[idx] = float(val / benefit_sum)  # as a pdf

    benefit = 0
    for i, x in enumerate(stat['thread_write']):
        if i < low_idx:
            continue
        if is_ideal:
            benefit += x * cdf_idx_to_latency(int(i))  # for debugging
        else:
            for j, y in enumerate(benefit_pdf):
                # x: number of remote write, y: pdf of benefit, i and j: bucket
                benefit += x * y * cdf_idx_to_latency(int(min(i, j)))

    return benefit


def calculate_benefit_prob_best_match(stat, benefit_cnt, remote_start_idx):
    low_idx = int(math.floor(remote_start_idx))
    benefit_sum = sum(benefit_cnt)  # total number of benefit logs
    write_sum = sum(stat['thread_write'][low_idx:])
    # stat['rw_record'][t_id][3]: number of total remote write
    benefit_pdf = []
    for idx, val in enumerate(benefit_cnt):
        benefit_pdf.append(float(val / benefit_sum))  # as a pdf
    benefit = 0
    last_pdf_idx = len(benefit_pdf) - 1
    assigned_check = 0
    for i in range(len(stat['thread_write'])-1, low_idx-1, -1):
        # benefit += x * cdf_idx_to_latency(int(i)) # for debugging
        remain = float(stat['thread_write'][i] / write_sum)
        while remain > 0. and last_pdf_idx >= 0:
            if benefit_pdf[last_pdf_idx] > remain:
                assigned = remain
                benefit_pdf[last_pdf_idx] -= remain
            else:
                assigned = benefit_pdf[last_pdf_idx]
                benefit_pdf[last_pdf_idx] = 0
                last_pdf_idx -= 1
            benefit += assigned * cdf_idx_to_latency(int(min(last_pdf_idx, i)))     # portion * latency
            remain -= assigned
            assigned_check += assigned
            # print("Assigned: ", assigned_check)

    # print("Assigned: ", assigned_check)
    return benefit * write_sum  # avg benefit per access x number of accesses


def parse_pso_file(filep, t_id, stat, remote_start_idx, remote_adjust, is_best_match=False, is_ideal=False):
    benefit_sum = 0
    benefit_cnt = [0 for _ in range(_num_cdf_bucket)]
    loc_stat = {'local_read_lat': stat['local_read_lat'], 'local_write_lat': stat['local_write_lat'],
                'remote_read_lat': stat['remote_read_lat'], 'remote_write_lat': stat['remote_write_lat'],
                # 'thread_read_local': stat['thread_read'][t_id],
                'thread_write': stat['thread_write'][t_id]}
    while True:
        data_line = filep.readline()
        if len(data_line) <= 0:
            break
        data_line = data_line.split()
        if len(data_line) == 4:
            rec = [int(x) for x in data_line]
            benefit_cnt[min(latency_to_cdf_idx(calculate_benefit_from_rec(loc_stat, rec)), _num_cdf_bucket - 1)] += 1

    if t_id == 0:
        # print(loc_stat['thread_write'])
        # print(benefit_cnt)
        pass

    if is_best_match:
        stat['remote_write_benefits'][t_id] = calculate_benefit_prob_best_match(loc_stat, benefit_cnt, remote_start_idx)
    else:
        stat['remote_write_benefits'][t_id] = calculate_benefit_with_prob(
                                                                    loc_stat, benefit_cnt, remote_start_idx, is_ideal)
    stat['remote_write_benefits'][t_id] *= remote_adjust
    # we use pdf based calculation, so target_ratio is not needed
    # stat['remote_write_benefits'][t_id] = benefit_sum * stat['target_ratio'][t_id]
    # print("B[" + str(t_id) + "] .. done: %.3f" % stat['target_ratio'][t_id], ", ",
    #        stat['remote_write_benefits'][t_id], " / ", stat['thread_rw'][t_id][3])


def load_pso_file(filename, stat, t_id, remote_start_idx, remote_adjust, is_best_match, is_ideal):
    fp = open(filename, "r")
    # print("[" + str(t_id) + "] " + filename + "..started", flush=True)
    parse_pso_file(fp, t_id, stat, remote_start_idx, remote_adjust, is_best_match, is_ideal)
    # print("[" + str(t_id) + "] " + filename + "..done", flush=True)
    fp.close()


def parse_rw_file(filename, stat, t_id, target_line):
    fp = open(filename, "r")
    last_rec = []
    line_no = 0
    while True:
        line_no += 1    # log start from pass 1 not 0
        data_line = fp.readline()
        if len(data_line) <= 0:
            break
        if (line_no > target_line) and (len(last_rec) > 0):
            continue
        data_line = data_line.split()
        if len(data_line) == 4:
            last_rec = [int(x) for x in data_line]

    stat['rw_record'][t_id] = deepcopy(last_rec)
    stat['target_ratio'][t_id] = float(target_line / max(1., line_no))
    # print("[" + str(t_id) + "] " + filename + "..done", flush=True)
    fp.close()


def update_sim_params(stat):
    sim_record = np.asarray(stat['rw_record'][:])
    stat['sim_local_read'] = np.sum(sim_record[:, 0])
    stat['sim_local_write'] = np.sum(sim_record[:, 1])
    stat['sim_remote_read'] = np.sum(sim_record[:, 2])
    stat['sim_remote_write'] = np.sum(sim_record[:, 3])


def sim_versus_impl_analysis(stat):
    stat['sim_ratio'] = [float(stat['total_local_read']/stat['sim_local_read']),
                         float(stat['total_local_write']/stat['sim_local_write']),
                         float(stat['total_remote_read']/stat['sim_remote_read']),
                         float(stat['total_remote_write']/stat['sim_remote_write'])]


def print_sim_versus_impl(stat):
    print("Local read: impl[%d] sim[%d] || err[%.3f%%]" % (stat['total_local_read'], stat['sim_local_read'],
                                                         (stat['sim_local_read'] - stat['total_local_read'])
                                                           / stat['total_local_read'] * 100))
    print("Local write: impl[%d] sim[%d] || err[%.3f%%]" % (stat['total_local_write'], stat['sim_local_write'],
                                                         (stat['sim_local_write'] - stat['total_local_write'])
                                                           / stat['total_local_write'] * 100))
    print("Remote read: impl[%d] sim[%d] || err[%.3f%%]" % (stat['total_remote_read'], stat['sim_remote_read'],
                                                         (stat['sim_remote_read'] - stat['total_remote_read'])
                                                           / stat['total_remote_read'] * 100))
    print("Remote write: impl[%d] sim[%d] || err[%.3f%%]" % (stat['total_remote_write'], stat['sim_remote_write'],
                                                         (stat['sim_remote_write'] - stat['total_remote_write'])
                                                           / stat['total_remote_write'] * 100))


def check_and_rename_files(dir_name):
    for filename in os.listdir(dir_name):
        split_list = deepcopy(filename)
        split_list = split_list.split("_")
        if int(split_list[0]) < 10:
            if (int(split_list[0]) == 0 and split_list[0] == "0") or (split_list[0][0] != "0"):
                new_name = '0' + filename
                command = 'mv ' + dir_name + filename + ' ' + dir_name + new_name
                # print(command, " for ", split_list)
                os.system(command)


if __name__ == '__main__':
    # print("start parsing cdf logs")
    # example
    '''
    --dir=/home/sslee/workspace/disaggregated_mem_python/profile/ma/t20_tso/
    --pso_dir=/home/sslee/workspace/Disaggregated_mem_benchmark/simulator/logs/logs.ma_80t_pso/pso/
    --rw_dir=/home/sslee/workspace/Disaggregated_mem_benchmark/simulator/logs/logs.ma_80t_pso/rwcnt/
    --tar=35000
    --ext=35000
    --unlimited_dir_sim=True
    --unlimit_cdf_dir=/home/sslee/workspace/Disaggregated_mem_benchmark/simulator/logs/logs.ma_80t_pso.unlimited/cdf/
    --limit_cdf_dir=/home/sslee/workspace/Disaggregated_mem_benchmark/simulator/logs/logs.ma_80t_pso.limited/cdf/
    --unlimit_tar=5000
    '''

    # a) args
    parser = argparse.ArgumentParser()
    parser.add_argument('--dir', type=str, help='path to the directory storing the cdf logs', default='./input')
    parser.add_argument('--thread_per_blade', type=int, help='number of threads in a blade', default=int(10))
    parser.add_argument('--pso_dir', type=str, help='path to the directory storing the pso logs', default='./input_pso')
    parser.add_argument('--rw_dir', type=str, help='path to the directory storing the rw logs', default='./input_pso')
    parser.add_argument('--unlimited_dir_sim', type=bool, default=False)
    parser.add_argument('--unlimit_cdf_dir', type=str, default='./input_pso')
    parser.add_argument('--limit_cdf_dir', type=str, default='./input_pso')
    parser.add_argument('--unlimit_tar', type=int, help='target number of passes for unlimited logs', default=int(3000))
    parser.add_argument('--tar', type=int, help='target number of passes to parse', default=int(50000))
    parser.add_argument('--ext', type=int, help='number of passes for extrapolation', default=int(50000))
    parser.add_argument('--bkt_size', type=int, help='size of bucket', default=int(512))
    parser.add_argument('--profile_overhead', type=float, help='profiling overhead in CDF', default=float(0.2)) # 0.96
    parser.add_argument('--proc_num', type=int, help='number of workers we will use in parallel', default=80)
    parser.add_argument('--local_th', type=float, help='maximum latency considered as local access', default=float(4))
    parser.add_argument('--remote_adjust', type=float,
                        help='adjust latency (profile to actual run, not only for remote)',
                        default=float(0.74))
    # Only one of the following two can be true
    # DEFAULT: both of them are disabled
    parser.add_argument('--best_match', type=bool, help='use best match mode instead of pdf summation', default=False)
    parser.add_argument('--ideal', type=bool, help='performance when no remote write is visible', default=False)

    # 4 us for all
    args = parser.parse_args()
    profile_overhead = args.profile_overhead

    # print(args)

    # a2) init
    proc_num = args.proc_num
    manager = Manager()
    stat = manager.dict()
    stat['remote_write_list'] = manager.dict()

    # b) files
    print("Input directory: " + args.dir)
    if not os.path.isdir(args.dir):
        print("Error: " + args.dir + " is not directory!")
        exit()
    print("Input PSO directory: " + args.pso_dir)
    if not os.path.isdir(args.pso_dir):
        print("Error: " + args.pso_dir + " is not directory!")
        exit()
    check_and_rename_files(args.pso_dir)
    print("Input RW directory: " + args.rw_dir)
    if not os.path.isdir(args.rw_dir):
        print("Error: " + args.rw_dir + " is not directory!")
        exit()
    check_and_rename_files(args.rw_dir)

    if args.unlimited_dir_sim:
        print("Input unlimit CDF directory: " + args.unlimit_cdf_dir)
        if not os.path.isdir(args.unlimit_cdf_dir):
            print("Error: " + args.unlimit_cdf_dir + " is not directory!")
            exit()
        print("Input limit CDF directory: " + args.limit_cdf_dir)
        if not os.path.isdir(args.limit_cdf_dir):
            print("Error: " + args.limit_cdf_dir + " is not directory!")
            exit()
        # cdf files are already sorted

    # load cdf directly from MIND
    filelist = [args.dir + f for f in os.listdir(args.dir)]
    filelist.sort()
    rw_stat = dict()
    num_thread = len(filelist)
    rw_stat['read'] = [0 for _ in range(args.bkt_size)]
    rw_stat['write'] = [0 for _ in range(args.bkt_size)]
    rw_stat['thread_read'] = list(
        [list([0 for _1 in range(args.bkt_size)]) for _ in range(len(filelist))])
    rw_stat['thread_write'] = list(
        [list([0 for _1 in range(args.bkt_size)]) for _ in range(len(filelist))])
    for idx, filename in enumerate(filelist):
        fp = open(filename, "r")
        parse_cdf_file(fp, args.tar, rw_stat, idx)
        # print("[%d]CDF file: " % idx, filename)

    # load cdf and calculate difference: limit versus unlimit from simulator
    if args.unlimited_dir_sim:
        unlimit_stat = {'unlimit': {'filelist': [args.unlimit_cdf_dir + f for f in os.listdir(args.unlimit_cdf_dir)]},
                        'limit': {'filelist': [args.limit_cdf_dir + f for f in os.listdir(args.limit_cdf_dir)]}}
        for stat_rec in [unlimit_stat['unlimit'], unlimit_stat['limit']]:
            stat_rec['read'] = [0 for _ in range(args.bkt_size)]
            stat_rec['write'] = [0 for _ in range(args.bkt_size)]
            stat_rec['thread_read'] = list(
                [list([0 for _1 in range(args.bkt_size)]) for _ in range(num_thread)])
            stat_rec['thread_write'] = list(
                [list([0 for _1 in range(args.bkt_size)]) for _ in range(num_thread)])
            for idx in range(num_thread):
                fp = open(stat_rec['filelist'][idx], "r")
                parse_cdf_file(fp, args.unlimit_tar, stat_rec, idx)
                # print("[%d]CDF file: " % idx, filename)
        calculate_difference_cdf(unlimit_stat)
        # apply calculated difference in CDF
        apply_unlimited_expectation(rw_stat, unlimit_stat)
    # print_cdf_from_stat(rw_stat)

    # single thread -> multiple threads
    stat['read'] = manager.list(rw_stat['read'])
    stat['write'] = manager.list(rw_stat['write'])
    stat['thread_read'] = manager.list([manager.list([x for x in th_rec]) for th_rec in rw_stat['thread_read']])
    stat['thread_write'] = manager.list([manager.list([x for x in th_rec]) for th_rec in rw_stat['thread_write']])

    # load rwcnt file
    filelist = [args.rw_dir + f for f in os.listdir(args.rw_dir)]
    filelist.sort()
    stat['rw_record'] = manager.list()
    for _ in range(num_thread):
        stat['rw_record'].append(0)
    stat['target_ratio'] = manager.list([float(1.0) for _ in range(num_thread)])
    proc_list = [[] for _ in range(proc_num)]
    for idx in range(num_thread):
        p = Process(target=parse_rw_file, args=(filelist[idx], stat, idx, args.tar,))
        proc_list[idx % proc_num].append(p)
    for p in proc_list:
        for pp in p:
            pp.start()
        for pp in p:
            pp.join()

    # analyze loaded rwcnt
    update_sim_params(stat)
    stat['thread_rw'] = manager.list([manager.list() for _ in rw_stat['thread_read']])
    analysis_stat(stat, args.local_th, args.remote_adjust)
    sim_versus_impl_analysis(stat)

    # load pso files
    filelist = [args.pso_dir + f for f in os.listdir(args.pso_dir)]
    filelist.sort()
    stat['remote_write_benefits'] = manager.list([0 for _ in range(num_thread)])
    proc_list = [[] for _ in range(proc_num)]
    for idx in range(num_thread):
        p = Process(target=load_pso_file, args=(filelist[idx], stat, idx, args.local_th, args.remote_adjust,
                                                args.best_match, args.ideal))
        proc_list[idx % proc_num].append(p)
    for idx in range(num_thread):
        for p in proc_list:
            if len(p) > idx:
                p[idx].start()
        for p in proc_list:
            if len(p) > idx:
                p[idx].join()

    # caculate estimated time
    proc_list = [[] for _ in range(proc_num)]
    stat['est_time'] = manager.list([0 for _ in range(num_thread)])
    stat['est_time_sc'] = manager.list([0 for _ in range(num_thread)])
    for t_id in range(len(stat['est_time'])):
        p = Process(target=calculate_est_time, args=(stat, t_id,))
        proc_list[idx % proc_num].append(p)
    for p in proc_list:
        for pp in p:
            pp.start()
        for pp in p:
            pp.join()

    # d) print result
    # print_sim_versus_impl(stat)
    print_stat(stat, args.ext, float(args.ext / args.tar))
    print("")
    pass
