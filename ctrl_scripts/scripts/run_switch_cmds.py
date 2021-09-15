import requests
import argparse
import socket
import time
import shutil

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--switch', type=str, help='IP address of the switch', default="127.0.0.1")
    parser.add_argument('--cmd', type=str, help='type of command = [restart_switch, download_log]', default="")
    parser.add_argument('--user', type=str, help='user name of the server', default="sslee_cs")
    parser.add_argument('--local', type=str, help='IP address of the local machine', default="")
    args = parser.parse_args()

    url="http://" + args.switch + ":9009/hooks/"
    restart_hook = "restart_switch"
    download_hook = "download_log"
    mind_switch_hook = "setup_mind_switch"
    normal_switch_hook = "setup_normal_switch"
    if args.local == "":
        my_ip = socket.gethostbyname(socket.gethostname())
        # my_ip = "172.29.249.30"
    else:
        my_ip = args.local

    if args.cmd == "restart_switch":
        req = requests.get(url = url + restart_hook, params={'remote-addr': str(my_ip)}) 
    elif args.cmd == "download_log":
        req = requests.get(url = url + download_hook, params={'remote-addr': str(my_ip), 'remote-user': args.user})
    elif args.cmd == "setup_mind_switch":
        req = requests.get(url = url + mind_switch_hook, params={'remote-addr': str(my_ip)})
    elif args.cmd == "setup_normal_switch":
        req = requests.get(url = url + normal_switch_hook, params={'remote-addr': str(my_ip)})
    else:
        exit(0)
    print("Please wait for 1 min to finish running the script", flush=True)
    time.sleep(60)

    # copy log file
    time_str = time.strftime("%Y-%m-%d_%H-%M-%S", time.gmtime())
    time_str = "latest_%s.log" % (time_str)
    download_str = "/home/" + args.user + "/Downloads"
    shutil.copyfile(download_str + "/latest.log", download_str + "/switch_logs/" + time_str)
