import os
# import math
import argparse


class bcolors:
    HEADER = '\033[95m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("res_dir", type=str, help="Path to the directory including the result files", default="")
    args = parser.parse_args()

    res_dir = args.res_dir
    if not os.path.isdir(res_dir):
        print(bcolors.FAIL + "Wrong path: " + res_dir + bcolors.ENDC + "\n")
        exit()

    res_files = os.listdir(res_dir)
    print("Result files in " + res_dir + ": ")
    res_time_list = {}
    for fname in res_files:
        if fname[0:3] != 'res':
            continue
        fname_split = fname.split('_')
        node_idx = fname_split[1]
        sr = fname_split[2][2:5]
        rwr = fname_split[3][2:5]
        if sr not in res_time_list:
            res_time_list[sr] = {}
        if rwr not in res_time_list[sr]:
            res_time_list[sr][rwr] = []

        fpath = res_dir + '/' + fname
        with open(fpath) as f:
            res_line = ""
            for line in f:
                res_line = line
            if res_line != "":
                res_str = res_line.split(',')
                res_str = res_str[1].lstrip()
                res_str = res_str.split(' ')[0]
                res_time_list[sr][rwr].append(float(res_str))

    print("=== Result ===")
    for sr_key in res_time_list:
        for rwr_key in res_time_list[sr_key]:
            sum_val = sum(res_time_list[sr_key][rwr_key])
            header_str = "Throughput | sharing ratio[" + str(sr_key) + "], read ratio[" + str(rwr_key) + "]: "
            if sum_val > 0:
                print(bcolors.OKGREEN + header_str + str(sum_val) + " 4KB IOPS" + bcolors.ENDC)
            else:
                print(bcolors.WARNING + header_str + "No data found" + bcolors.ENDC)
    print("")
