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
        if fname[0:8] != 'progress':
            continue
        fname_split = fname.split('_')
        node_idx = fname_split[1]
        node_num = fname_split[3]
        if node_num not in res_time_list:
            res_time_list[node_num] = []

        fpath = res_dir + '/' + fname
        with open(fpath) as f:
            res_line = ""
            for line in f:
                res_line = line
            if res_line != "":
                res_str = res_line.split('||')[1]
                res_str = res_str.split('[')[1]
                res_str = res_str.split(']')[0]
                res_time_list[node_num].append(int(res_str))

    print("=== Result ===")
    for key in res_time_list:
        max_val = max(res_time_list[key])
        if max_val > 0:
            print(bcolors.OKGREEN + "Normalized Max for #blade[" + str(key) + "]: " + str(1. / max_val) + bcolors.ENDC)
        else:
            print(bcolors.WARNING + "Normalized Max for #blade[" + str(key) + "]: No data found")
    print("")

