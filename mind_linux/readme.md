# Modified Linux 4.15 for MIND
- Since this is a research prototype, the kernel itself can become unstable. Hence, we *recommend* building and using the kernel inside a virtual machine.
- Dependencies
  - We tested building process on Ubuntu 18.04 (*Linux 4.15.0*)
  - `sudo apt-get install libncurses-dev gawk flex bison openssl libssl-dev dkms libelf-dev libudev-dev libpci-dev libiberty-dev autoconf`
    - Ref: https://wiki.ubuntu.com/Kernel/BuildYourOwnKernel
- To compile the kernel, please use the script:
`./build_kernel.sh`
  - Note) This script will compile and install the kernel, then reboot the machine.
  - Note) If you are using newer version of gcc (higher than gcc-7), you may experience some warnings/errors. Please try again with gcc-7.
    - How to install and set up symlink to gcc-7 ([link](https://gist.githubusercontent.com/jlblancoc/99521194aba975286c80f93e47966dc5/raw/d8fcab3ba6b89b840dea10a523d69b0f3e64409c/Install_gcc7_ubuntu_16.04.md))
      ```
      sudo apt-get install -y software-properties-common
      sudo add-apt-repository ppa:ubuntu-toolchain-r/test
      sudo apt update
      sudo apt install g++-7 -y
      sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 \
                               --slave /usr/bin/g++ g++ /usr/bin/g++-7 
      sudo update-alternatives --config gcc
      gcc --version
      g++ --version
      ```
- To compile RoCE module:
  - `cd roce_modules && make && sudo insmod roce4disagg.ko`
  - Dependencies: we assumed that the kernel header files for ofed kernel in `/usr/src/ofa_kernel` (from the NIC driver, mlnx_ofed 5.0-1)
