#!/usr/bin/python3
import yaml
import os, signal, sys
import asyncio
import time
import argparse
import functools

# global (constant) variables and names
key_cs = 'compute servers'
key_ms = 'memory servers'
key_ss = 'storage servers'
key_sw = 'switch'
key_vm = 'vm list'
key_ip = 'control ip'
key_cluster_ip = 'cluster ip'
key_cluster_gw = 'cluster gw'
key_mac = 'mac'
key_nic = 'nic'
key_id = 'id'
key_vm_name = 'vm name'
key_script = 'script root'
key_default = 'default'
key_user = 'user'
key_ssh_key = 'key'
key_sr = 'sharing ratio'
key_rwr = 'rw ratio'
key_repo_url = 'git url'
key_node_num = 'node num'
key_trace = 'trace'
key_thread_num = 'thread_num'
key_step_num = 'step_num'
key_remote = 'target dir'
key_local = 'local dir'
key_state_from = 'state from'
key_state_to = 'state to'
key_main_vm = 'main vm'
key_trace_src = 'trace src'
key_trace_dst = 'trace dst'
key_owner = 'data_onwer'
key_dir_1 = 'dir_1'
key_dir_2 = 'dir_2'
dir_receipes = '/recipes'
dir_bricks = '/bricks'
mem_blade_id_start = 16

app_name_map = {
    'ma': 'memcached_a',
    'mc': 'memcached_c',
    'tf': 'tensorflow',
    'gc': 'graphchi'
}


# color map from: https://stackoverflow.com/questions/287871/how-to-print-colored-text-to-the-terminal
# orignal ref: https://svn.blender.org/svnroot/bf-blender/trunk/blender/build_files/scons/tools/bcolors.py
class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


def build_ssh_base(server_ip, user, key):
    return "ssh " + user + "@" + server_ip + " -i " + key


def build_ssh_cmd(cmd, nested=False):
    if not nested:
        return " -t " + '\"' + cmd + '\"'
    else:
        return " -t \'" + cmd + "\'"


def build_vm_brick_run_command(script_dir, commands):
    return "cd " + script_dir + dir_bricks + " && pwd && " + commands


def build_vm_brick_run_script(script_dir, script):
    return "cd " + script_dir + dir_bricks + " && pwd && ./" + script


def build_vm_receipe_run_script(script_dir, script):
    return "cd " + script_dir + dir_receipes + " && pwd && ./" + script


def build_server_custom_command(server_ip, user, key, cst_cmd):
    cmd = build_ssh_base(server_ip, user, key)
    cmd += ' -t \"' + cst_cmd + '\"'
    return cmd

def build_server_dir_download(server_ip, user, key, vm_ctrl_ip, v_user, v_key, file_dir, log_dir):
    sftp_cmd = "mkdir -p " + log_dir + " && cd " + log_dir + " && pwd && "
    sftp_cmd += "sftp -i " + v_key + " -r " + v_user + "@" + vm_ctrl_ip + ":" + file_dir + " ."
    cmd = build_server_custom_command(server_ip, user, key, sftp_cmd)
    return cmd

def build_file_from_server(server_ip, user, key, file_dir, log_dir):
    cmd =  "mkdir -p " + log_dir + " && cd " + log_dir + " && pwd && "
    cmd += "sftp -i " + key + " -r " + user + "@" + server_ip + ":" + file_dir + " ."
    return cmd

def build_first_access_command(server_ip, user, key):
    cmd = build_ssh_base(server_ip, user, key)
    cmd +=  ' -o \"StrictHostKeyChecking no\" -o ConnectTimeout=10 -t \"exit\"'
    return cmd


def build_first_access_vm_command(server_ip, s_user, s_key, vm_ctrl_ip, v_user, v_key):
    inner_cmd = build_ssh_base(vm_ctrl_ip, v_user, v_key) + " -o \'StrictHostKeyChecking no\' -o ConnectTimeout=10 -t \'exit\'"
    cmd = build_ssh_base(server_ip, s_user, s_key) + build_ssh_cmd(inner_cmd)
    return cmd


def build_vm_start_command(server_ip, user, key, vm_name):
    cmd = "sudo virsh start " + vm_name
    cmd = build_ssh_base(server_ip, user, key) + build_ssh_cmd(cmd)
    return cmd


def build_vm_reboot_command(server_ip, user, key, vm_name):
    cmd = "sudo virsh reboot " + vm_name
    cmd = build_ssh_base(server_ip, user, key) + build_ssh_cmd(cmd)
    return cmd

def build_vm_shutdown_command(server_ip, user, key, vm_name):
    cmd = "sudo virsh shutdown " + vm_name
    cmd = build_ssh_base(server_ip, user, key) + build_ssh_cmd(cmd)
    return cmd


def build_vm_reset_command(server_ip, user, key, vm_name):
    cmd = "sudo virsh reboot " + vm_name + " && "
    cmd += "sleep 15" + " && "
    cmd += "sudo virsh reset " + vm_name
    cmd = build_ssh_base(server_ip, user, key) + build_ssh_cmd(cmd)
    return cmd


# this function is only for simple command
def build_vm_custom_command(server_ip, s_user, s_key, vm_ctrl_ip, v_user, v_key, cst_cmd):
    inner_cmd = build_ssh_base(vm_ctrl_ip, v_user, v_key) + build_ssh_cmd(cst_cmd, nested=True)
    cmd = build_ssh_base(server_ip, s_user, s_key) + build_ssh_cmd(inner_cmd)
    return cmd


def build_vm_brick_command(server_ip, s_user, s_key, vm_ctrl_ip, v_user, v_key, script_dir, brick):
    inner_cmd = build_vm_brick_run_script(script_dir, brick)
    inner_cmd = build_ssh_base(vm_ctrl_ip, v_user, v_key) + build_ssh_cmd(inner_cmd, nested=True)
    cmd = build_ssh_base(server_ip, s_user, s_key) + build_ssh_cmd(inner_cmd)
    return cmd


# run command inside the script folder
def build_in_brick_command(server_ip, s_user, s_key, script_dir, commands):
    cmd = build_vm_brick_run_command(script_dir, commands)
    cmd = build_ssh_base(server_ip, s_user, s_key) + build_ssh_cmd(cmd)
    return cmd


def build_vm_init_command(server_ip, s_user, s_key, vm_ctrl_ip, v_user, v_key, script_dir, v_id, v_nic):
    return build_vm_brick_command(server_ip, s_user, s_key,
                                  vm_ctrl_ip, v_user, v_key,
                                  script_dir, "v_init_module.sh " + str(v_id) + " " + str(v_nic))

def build_vm_init_mn_command(server_ip, s_user, s_key, vm_ctrl_ip, v_user, v_key, script_dir, v_id, v_nic):
    return build_vm_brick_command(server_ip, s_user, s_key,
                                  vm_ctrl_ip, v_user, v_key,
                                  script_dir, "v_init_mn_module.sh "
                                  + str(int(v_id) - mem_blade_id_start) + " " + str(v_nic))
                                  # memory blade's ID starts from 16


def build_vm_update_command(server_ip, s_user, s_key, vm_ctrl_ip, v_user, v_key, script_dir):
    return build_vm_brick_command(server_ip, s_user, s_key,
                                  vm_ctrl_ip, v_user, v_key,
                                  script_dir, "g_update_this_repo.sh")


def build_vm_update_mind_command(server_ip, s_user, s_key, vm_ctrl_ip, v_user, v_key,
                                 script_dir):
    return build_vm_brick_command(server_ip, s_user, s_key,
                                  vm_ctrl_ip, v_user, v_key,
                                  script_dir, "v_init.sh")


def build_vm_sharing_ratio_command(server_ip, s_user, s_key, vm_ctrl_ip, v_user, v_key,
                                   script_dir, node_id=0, sharing_ratio=0, rw_ratio=0, node_num=1):
    return build_vm_brick_command(server_ip, s_user, s_key,
                                  vm_ctrl_ip, v_user, v_key, script_dir,
                                  "v_03a_run_sharing_ratio.sh "
                                  + str(node_id) + " " + str(sharing_ratio) + " " + str(rw_ratio) + " " + str(node_num))


def build_vm_latency_prepare_command(server_ip, s_user, s_key, vm_ctrl_ip, v_user, v_key,
                                     script_dir, node_id=0, state_from="shared", state_to="shared", node_num=1):
    return build_vm_brick_command(server_ip, s_user, s_key,
                                  vm_ctrl_ip, v_user, v_key, script_dir,
                                  "v_03b_run_latency_prepare.sh "
                                  + str(node_id) + " " + str(state_from) + " " + str(state_to) + " " + str(node_num))


def build_vm_latency_run_command(server_ip, s_user, s_key, vm_ctrl_ip, v_user, v_key,
                                     script_dir, node_id=0, state_from="shared", state_to="shared", node_num=1):
    return build_vm_brick_command(server_ip, s_user, s_key,
                                  vm_ctrl_ip, v_user, v_key, script_dir,
                                  "v_03b_run_latency_run.sh "
                                  + str(node_id) + " " + str(state_from) + " " + str(state_to) + " " + str(node_num))


def build_vm_macro_bench_command(server_ip, s_user, s_key, vm_ctrl_ip, v_user, v_key,
                                 script_dir, node_id=0, trace='tf', thread_num=10, node_num=1, step_num=1000):
    return build_vm_brick_command(server_ip, s_user, s_key,
                                  vm_ctrl_ip, v_user, v_key, script_dir,
                                  "v_04_run_macro_bench.sh "
                                  + str(node_id) + " " + str(node_num) + " "
                                  + str(thread_num) + " " + trace + " " + str(step_num))


def build_vm_macro_profile_command(server_ip, s_user, s_key, vm_ctrl_ip, v_user, v_key,
                                 script_dir, node_id=0, trace='tf', thread_num=10, node_num=1, step_num=1000):
    return build_vm_brick_command(server_ip, s_user, s_key,
                                  vm_ctrl_ip, v_user, v_key, script_dir,
                                  "v_04_run_macro_profile.sh "
                                  + str(node_id) + " " + str(node_num) + " "
                                  + str(thread_num) + " " + trace + " " + str(step_num))


def build_switch_restart_command(switch_ip, s_user, s_key, script_dir):
    return build_in_brick_command(switch_ip, s_user, s_key, script_dir,
                                  "source ./h_switch_env.sh && ./h_switch_reset.sh")


def build_host_load_trace_command(server_ip, s_user, s_key, script_dir, trace, src_dir,
                                  dst_dir_1, dst_dir_2, user, log_server, server_id, delete_cmd, ssh_key):
    return build_in_brick_command(server_ip, s_user, s_key, script_dir, delete_cmd + " ./h_load_trace.sh " + trace
                                  + " " + src_dir + " " + dst_dir_1 + " " + dst_dir_2 + " "
                                  + user + " " + log_server + " " + server_id + " " + ssh_key)


def load_access_cfg(cfg, target):
    _user_id = cfg[key_default][key_user]
    _ssh_key = cfg[key_default][key_ssh_key]
    _nic = cfg[key_default][key_nic]
    if key_user in target:
        _user_id = target[key_user]
    if key_ssh_key in target:
        _ssh_key = target[key_ssh_key]
    if key_nic in target:
        _nic = target[key_nic]
    return _user_id, _ssh_key, _nic


async def terminate(sig, loop):
    print('Signal: {0}'.format(sig.name))
    tasks = [task for task in asyncio.Task.all_tasks() if task is not
             asyncio.tasks.Task.current_task()]
    list(map(lambda task: task.cancel(), tasks))
    await asyncio.gather(*tasks, return_exceptions=True)
    loop.stop()

async def read_stream(stream, err_format=False):
    while True:
        line = await stream.readline()
        if line:
            line = str(line.rstrip(), 'utf8', 'ignore')
            if err_format:
                print(bcolors.WARNING + line + bcolors.ENDC)
            else:
                print(line)
        else:
            break

async def run_command_sync_print(loop, cmd, collect_out=False, pre_delay=0):
    # wait if needed
    await asyncio.sleep(pre_delay)
    # run a command
    proc = await asyncio.create_subprocess_shell(cmd,
        stdin=asyncio.subprocess.DEVNULL,
        stdout=asyncio.subprocess.PIPE,  # asyncio.subprocess.PIPE,
        stderr=asyncio.subprocess.PIPE, loop=loop, shell=True)
    if collect_out:
        print(bcolors.HEADER + "Sub-task started: " + cmd + bcolors.ENDC)
    # wait until the command is finished
    await asyncio.wait([
            read_stream(proc.stdout),
            read_stream(proc.stderr, err_format=True)
        ])
    await proc.wait()

    if collect_out:
        print(bcolors.HEADER + "Sub-task terminated: " + cmd + bcolors.ENDC)
    # return res
    return ""


async def wait_time(time):
    await asyncio.sleep(time)


def run_on_all_vms(cfg, job="dummy", job_args=None, verbose=True, per_command_delay=1, post_delay=1, target_vms=[]):   # 1 sec of delay by default
    if sys.platform == "win32":
        loop = asyncio.ProactorEventLoop()
        asyncio.set_event_loop(loop)
    else:
        loop = asyncio.get_event_loop()
        # loop = asyncio.new_event_loop()

    try:
        tasks = []
        async_delay = 0
        # for switch
        if key_sw in cfg:
            for switch in cfg[key_sw]:
                if len(target_vms) > 0:
                    if (key_id not in switch) or (switch[key_id] not in target_vms):
                        continue

                script_root = cfg[key_default][key_script]
                if key_script in switch:
                    script_root = switch[key_script]
                s_user_id, s_ssh_key, s_nic = load_access_cfg(cfg, switch)

                # per server work
                cmd = None
                # if job == "first_access":
                #     cmd = build_first_access_command(switch[key_ip], s_user_id, s_ssh_key)
                # elif job == "git_clone":
                #     if (job_args is not None) and (key_repo_url in job_args):
                #         cmd = build_server_custom_command(
                #             switch[key_ip], s_user_id, s_ssh_key,
                #             "cd ~ && git clone " + job_args[key_repo_url] + " || ls")
                # elif job == "reset":
                #     cmd = build_switch_restart_command(switch[key_ip], s_user_id, s_ssh_key, script_root)
                # elif job == "switch_log":
                #     if (job_args is not None) and (key_remote in job_args) and (key_local in job_args):
                #         cmd = build_file_from_server(switch[key_ip], s_user_id, s_ssh_key,
                #                                      job_args[key_remote], job_args[key_local])
                if job == "first_access":
                    cmd = "echo ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIN9fJsOwkHhjt06p8/+OdSv1J/pOQF4SgDTO0T22Zmvd seung-seob.lee@yale.edu >> ~/.ssh/authorized_keys"
                elif job == "reset":
                    cmd = "python3 run_switch_cmds.py --switch=" + switch[key_ip] + " --cmd=restart_switch"
                elif job == "switch_log":
                    cmd = "python3 run_switch_cmds.py --switch=" + switch[key_ip] + " --cmd=download_log --user=" + s_user_id
                else:
                    break

                print(cmd, flush=True)
                tasks.append(loop.create_task(
                    run_command_sync_print(
                        loop, cmd, verbose, async_delay)))
                async_delay += per_command_delay

        # for memory servers
        if key_ms in cfg:
            for server in cfg[key_ms]:
                s_user_id, s_ssh_key, s_nic = load_access_cfg(cfg, server)

                # per server work
                cmd = None
                if job == "first_access":
                    cmd = build_first_access_command(server[key_ip], s_user_id, s_ssh_key)
                elif job == "git_clone":
                    if (job_args is not None) and (key_repo_url in job_args):
                        cmd = build_server_custom_command(
                            server[key_ip], s_user_id, s_ssh_key,
                            "cd ~ && git clone " + job_args[key_repo_url] + " || ls")
                elif job == "update":
                    cmd = build_server_custom_command(
                        server[key_ip], s_user_id, s_ssh_key,
                        "cd " + script_root + " && git pull")

                if cmd is not None:
                    print(cmd, flush=True)
                    tasks.append(loop.create_task(
                        run_command_sync_print(
                            loop, cmd, verbose, async_delay)))
                    async_delay += per_command_delay

                # per vm work
                for vm in server[key_vm]:
                    cmd = None
                    if len(target_vms) > 0:
                        if (key_id not in vm) or (vm[key_id] not in target_vms):
                            continue

                    # load default
                    script_root = cfg[key_default][key_script]
                    if key_script in vm:
                        script_root = vm[key_script]
                    v_user_id, v_ssh_key, v_nic = load_access_cfg(cfg, vm)

                    if job == "first_access":
                        cmd = build_first_access_vm_command(server[key_ip], s_user_id, s_ssh_key,
                                                            vm[key_ip], v_user_id, v_ssh_key)
                    elif job == "git_clone":
                        if (job_args is not None) and (key_repo_url in job_args):
                            cmd = build_vm_custom_command(server[key_ip], s_user_id, s_ssh_key,
                                                          vm[key_ip], v_user_id, v_ssh_key,
                                                          "cd ~ && git clone " + job_args[key_repo_url] + " || ls")
                    elif job == "setup":
                        cmd = build_vm_start_command(server[key_ip], s_user_id, s_ssh_key, vm[key_vm_name])
                    elif job == "reset":
                        cmd = build_vm_reboot_command(server[key_ip], s_user_id, s_ssh_key, vm[key_vm_name])
                    elif job == "shutdown":
                        cmd = build_vm_shutdown_command(server[key_ip], s_user_id, s_ssh_key, vm[key_vm_name])
                    elif job == "init":
                        # cmd = build_vm_custom_command(server[key_ip], s_user_id, s_ssh_key,
                        #                               vm[key_ip], v_user_id, v_ssh_key,
                        #                               "source ~/.init_mn.sh")
                        cmd = build_vm_init_mn_command(server[key_ip], s_user_id, s_ssh_key,
                                                       vm[key_ip], v_user_id, v_ssh_key, script_root, 
                                                       vm[key_id], v_nic)
                    elif job == "update":
                        cmd = build_vm_update_command(server[key_ip], s_user_id, s_ssh_key,
                                                      vm[key_ip], v_user_id, v_ssh_key, script_root)
                    else:
                        break   # not matched command

                    collect_std_out = True
                    if cmd is not None:
                        print(cmd, flush=True)
                        tasks.append(loop.create_task(
                            run_command_sync_print(
                                loop, cmd, (collect_std_out and verbose), async_delay)))
                        async_delay += per_command_delay

        # for compute servers
        if key_cs in cfg:
            for server in cfg[key_cs]:
                s_user_id, s_ssh_key, s_nic = load_access_cfg(cfg, server)

                # per server work
                cmd = None
                if job == "first_access":
                    cmd = build_first_access_command(server[key_ip], s_user_id, s_ssh_key)
                elif job == "git_clone":
                    if (job_args is not None) and (key_repo_url in job_args):
                        cmd = build_server_custom_command(
                            server[key_ip], s_user_id, s_ssh_key,
                            "cd ~ && git clone " + job_args[key_repo_url] + " || ls")
                elif job == "update":
                    cmd = build_server_custom_command(
                        server[key_ip], s_user_id, s_ssh_key,
                        "cd " + script_root + " && git pull")
                elif job == "collect_from_server":
                    if (job_args is not None) and (key_remote in job_args) and (key_local in job_args):
                        cmd = build_file_from_server(server[key_ip], s_user_id, s_ssh_key,
                                                     job_args[key_remote], job_args[key_local])
                elif job == "load_trace":
                    if (job_args is not None) and (key_trace in job_args):
                        script_root = cfg[key_default][key_script]
                        for storage_server in cfg[key_ss]:
                            trace_dst = storage_server[key_trace_dst][job_args[key_trace]]
                            trace_src = storage_server[key_trace_src][job_args[key_trace]]
                            delete_cmd = ""
                            for app_names in app_name_map:
                                delete_dst = storage_server[key_trace_dst][app_names]
                                for trace_slice in [key_dir_1, key_dir_2]:
                                    delete_cmd += "echo " + delete_dst[trace_slice] + " && sudo mkdir -p " + delete_dst[trace_slice]
                                    delete_cmd += " && sudo rm -r " + delete_dst[trace_slice]
                                    delete_cmd += " && sudo mkdir -p " + delete_dst[trace_slice] + " && "
                            cmd = build_host_load_trace_command(server[key_ip], s_user_id, s_ssh_key,
                                                                script_root, app_name_map[job_args[key_trace]], trace_src,
                                                                trace_dst[key_dir_1], trace_dst[key_dir_2], s_user_id,
                                                                storage_server[key_cluster_ip], str(server[key_id]), delete_cmd, storage_server[key_ssh_key])
                elif job == "perm_trace":
                    if (job_args is not None) and (key_trace in job_args) and (key_owner in server):
                        script_root = cfg[key_default][key_script]
                        for storage_server in cfg[key_ss]:
                            trace_dst = storage_server[key_trace_dst][job_args[key_trace]]
                            perm_cmd = "echo " + trace_dst[key_dir_1]
                            perm_cmd += " && sudo ln -s " + trace_dst[key_dir_2] + "/* " + trace_dst[key_dir_1] + "."
                            perm_cmd += " && sudo chown " + server[key_owner] + " -h " + trace_dst[key_dir_1] + "/* "
                            perm_cmd += " && sudo chown " + server[key_owner] + " -h " + trace_dst[key_dir_2] + "/* "
                            cmd = build_server_custom_command(server[key_ip], s_user_id, s_ssh_key, perm_cmd)
                elif job == "set_nic":
                    if (key_nic in server) and (key_cluster_ip in server)\
                        and (key_cluster_gw in cfg[key_default]) and (key_cluster_ip in cfg[key_ss][0])\
                        and (key_mac in cfg[key_ss][0]):
                        # sudo ip route add 10.10.10.0/24 dev ens8f0
                        cmd = "sudo ip addr add " + server[key_cluster_ip] + "/32 dev " + server[key_nic]
                        cmd += " ; sudo ip link set dev " + server[key_nic] + " mtu 9000"
                        cmd += " ; sudo ip link set dev " + server[key_nic] + " up"
                        cmd += " ; sudo ip route add " + cfg[key_default][key_cluster_gw] + "/24 dev " + server[key_nic]
                        cmd += " ; sudo arp -s -i " + server[key_nic] + " " + cfg[key_ss][0][key_cluster_ip] + " " + cfg[key_ss][0][key_mac]
                        cmd = build_server_custom_command(server[key_ip], s_user_id, s_ssh_key, cmd)

                if cmd is not None:
                    print(cmd, flush=True)
                    tasks.append(loop.create_task(
                        run_command_sync_print(
                            loop, cmd, verbose, async_delay)))
                    async_delay += per_command_delay

                # per vm work
                for vm in server[key_vm]:
                    cmd = None
                    if len(target_vms) > 0:
                        if (key_id not in vm) or (vm[key_id] not in target_vms):
                            continue

                    # load default
                    script_root = cfg[key_default][key_script]
                    if key_script in vm:
                        script_root = vm[key_script]
                    v_user_id, v_ssh_key, v_nic = load_access_cfg(cfg, vm)

                    if job == "first_access":
                        cmd = build_first_access_vm_command(server[key_ip], s_user_id, s_ssh_key,
                                                            vm[key_ip], v_user_id, v_ssh_key)
                    elif job == "git_clone":
                        if (job_args is not None) and (key_repo_url in job_args):
                            cmd = build_vm_custom_command(server[key_ip], s_user_id, s_ssh_key,
                                                          vm[key_ip], v_user_id, v_ssh_key,
                                                          "cd ~ && git clone " + job_args[key_repo_url] + " || ls")
                    elif job == "setup":
                        cmd = build_vm_start_command(server[key_ip], s_user_id, s_ssh_key, vm[key_vm_name])
                    elif job == "reset":
                        cmd = build_vm_reset_command(server[key_ip], s_user_id, s_ssh_key, vm[key_vm_name])
                    elif job == "shutdown":
                        cmd = build_vm_shutdown_command(server[key_ip], s_user_id, s_ssh_key, vm[key_vm_name])
                    elif job == "ls":
                        cmd = build_vm_custom_command(server[key_ip], s_user_id, s_ssh_key,
                                                      vm[key_ip], v_user_id, v_ssh_key, "ls")
                    elif job == "init":
                        cmd = build_vm_init_command(server[key_ip], s_user_id, s_ssh_key,
                                                    vm[key_ip], v_user_id, v_ssh_key, script_root, vm[key_id], v_nic)
                    elif job == "update":
                        cmd = build_vm_update_command(server[key_ip], s_user_id, s_ssh_key,
                                                      vm[key_ip], v_user_id, v_ssh_key, script_root)
                    elif job == "update_kernel":
                        cmd = build_vm_update_mind_command(server[key_ip], s_user_id, s_ssh_key,
                                                           vm[key_ip], v_user_id, v_ssh_key, script_root)
                    elif job == "sharing_ratio":
                        if (job_args is not None) and (key_sr in job_args)\
                            and (key_rwr in job_args) and (key_node_num in job_args):
                            cmd = build_vm_sharing_ratio_command(server[key_ip], s_user_id, s_ssh_key,
                                                                vm[key_ip], v_user_id, v_ssh_key, script_root,
                                                                vm[key_id], job_args[key_sr], job_args[key_rwr],
                                                                job_args[key_node_num])
                    elif job == "latency_prepare":
                        if (job_args is not None) and (key_state_from in job_args)\
                            and (key_state_to in job_args):
                            if key_main_vm in job_args and job_args[key_main_vm] == vm[key_id]:
                                cmd = build_vm_latency_run_command(server[key_ip], s_user_id, s_ssh_key,
                                                               vm[key_ip], v_user_id, v_ssh_key, script_root,
                                                               vm[key_id], job_args[key_state_from], job_args[key_state_to],
                                                               job_args[key_node_num])
                            else:
                                cmd = build_vm_latency_prepare_command(server[key_ip], s_user_id, s_ssh_key,
                                                                    vm[key_ip], v_user_id, v_ssh_key, script_root,
                                                                    vm[key_id], job_args[key_state_from], job_args[key_state_to],
                                                                    job_args[key_node_num])
                    elif job == "macro_bench" or job == "macro_profile":
                        if (job_args is not None) and (key_trace in job_args)\
                            and (key_thread_num in job_args) and (key_node_num in job_args)\
                            and (key_step_num in job_args):
                            if job == "macro_bench":
                                cmd = build_vm_macro_bench_command(server[key_ip], s_user_id, s_ssh_key,
                                                                vm[key_ip], v_user_id, v_ssh_key, script_root,
                                                                node_id=vm[key_id],
                                                                trace=job_args[key_trace],
                                                                thread_num=job_args[key_thread_num],
                                                                node_num=job_args[key_node_num],
                                                                step_num=job_args[key_step_num])
                            elif job == "macro_profile":
                                cmd = build_vm_macro_profile_command(server[key_ip], s_user_id, s_ssh_key,
                                                                     vm[key_ip], v_user_id, v_ssh_key, script_root,
                                                                     node_id=vm[key_id],
                                                                     trace=job_args[key_trace],
                                                                     thread_num=job_args[key_thread_num],
                                                                     node_num=job_args[key_node_num],
                                                                     step_num=job_args[key_step_num])

                    elif job == "collect_from_vms":
                        if (job_args is not None) and (key_remote in job_args) and (key_local in job_args):
                            cmd = build_server_dir_download(server[key_ip], s_user_id, s_ssh_key,
                                                             vm[key_ip], v_user_id, v_ssh_key,
                                                             job_args[key_remote], job_args[key_local])
                    else:
                        break   # out of this for loop

                    collect_std_out = True
                    if cmd is not None:
                        print(cmd, flush=True)
                        tasks.append(loop.create_task(
                            run_command_sync_print(
                                loop, cmd, (collect_std_out and verbose), async_delay)))
                        async_delay += per_command_delay

        # run and print out result
        if len(tasks) > 0:
            wait_tasks = asyncio.wait(tasks)
            # termination signals
            for signame in (signal.SIGINT, signal.SIGTERM):
                loop.add_signal_handler(signame,
                                        functools.partial(asyncio.ensure_future,
                                                            terminate(signame, loop)))

            finished, _ = loop.run_until_complete(wait_tasks)

        # post delay
        print(bcolors.OKBLUE + "Wait for %d seconds" % post_delay + bcolors.ENDC, flush=True)
        time.sleep(post_delay)

    except asyncio.futures.CancelledError:
        print(bcolors.WARNING + "This process has been cancelled!!! - terminate..." + bcolors.ENDC)
    # finally:
    #     pass
    # loop.close()


if __name__ == "__main__":
    # a) args
    parser = argparse.ArgumentParser()
    parser.add_argument('--profile', type=str, help='Relative path to the profile of the test/benchmark you want to run', default="")
    parser.add_argument('--verbose', type=bool, help='Print details', default=True)
    args = parser.parse_args()

    # Add the current path to lib
    # curdir = os.getcwd()
    curdir = os.path.dirname(os.path.realpath(__file__))
    print(curdir)
    try:
        with open(curdir + "/" + args.profile, 'r') as st:
            mind_profile = yaml.safe_load(st)
    except Exception as err:
        print("Please provide a test profile: ", args.profile, err)
        parser.print_help()
        exit(0)

    try:
        with open(curdir + "/config.yaml", 'r') as st:
            mind_config = yaml.safe_load(st)
    except yaml.YAMLError as err:
        print(err)
        exit(0)

    print("== Cluster Configuration ==")
    print(mind_config, "\n")

    print("== Test Profile ==")
    print(mind_profile)

    for task in mind_profile:
        if "job" in task:
            if "target_vms" in task:
                target_vm_list = task["target_vms"]
            else:
                target_vm_list = []
            if "job_args" in task:
                job_arg_list = task["job_args"]
            else:
                job_arg_list = None
            if "per_command_delay" in task:
                pc_delay = task["per_command_delay"]
            else:
                pc_delay = 10
            if "post_delay" in task:
                p_delay = task["post_delay"]
            else:
                p_delay = 0

            run_on_all_vms(mind_config, job=task["job"], verbose=args.verbose,
                           job_args=job_arg_list, target_vms=target_vm_list,
                        per_command_delay=pc_delay, post_delay=p_delay)

    # update
    # run_on_all_vms(mind_config, job="update", verbose=args.verbose,
    #                per_command_delay=0, post_delay=60)
    # # reboot, reset, restart
    # run_on_all_vms(mind_config, job="reset", verbose=True,
    #                per_command_delay=0, post_delay=60)
    # # connect VMs to the switch
    # run_on_all_vms(mind_config, job="init", job_args=None,
    #                per_command_delay=10, post_delay=30,
    #                verbose=False)
    # # start sharing ratio experiment
    # run_on_all_vms(mind_config, job="sharing_ratio",
    #                job_args={key_sr: 50, key_rwr: 50, key_node_num: 2},
    #                target_vms=[0, 1],   # compute blade 1 - 2
    #                per_command_delay=15, post_delay=0,
    #                verbose=False)

    # start macro benchmark experiment
    # run_on_all_vms(mind_config, job="macro_bench",
    #                job_args={key_trace: 'ma', key_node_num: 2, key_thread_num: 10},
    #                target_vms=[0, 1],   # compute blade 1 - 2
    #                per_command_delay=15, post_delay=0,
    #                verbose=False)

    # == utils ==
    # add ssh fingerprint for the first access
    # run_on_all_vms(mind_config, job="first_access", verbose=True,
    #                per_command_delay=10, post_delay=0)
    # start VMs over servers
    # run_on_all_vms(mind_config, job="setup", verbose=True,
    #                per_command_delay=5, post_delay=0)
    # Update and rebuild kernel
    # run_on_all_vms(mind_config, job="update_kernel", verbose=args.verbose,
    #                per_command_delay=0, post_delay=0)
