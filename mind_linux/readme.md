# Modified Linux 4.15 for MIND
- Since this is a research prototype, the kernel itself can become unstable. Hence, we *recommend* building and using the kernel inside a virtual machine.
- Dependencies
  - `sudo apt-get install libncurses-dev gawk flex bison openssl libssl-dev dkms libelf-dev libudev-dev libpci-dev libiberty-dev autoconf`
    - Ref: https://wiki.ubuntu.com/Kernel/BuildYourOwnKernel 
- To compile the kernel, please use the script:
`./build_kernel.sh`
  - Note | This script will compile and install the kernel, then reboot the machine.
