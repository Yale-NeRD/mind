# Modified Linux 4.15 for MIND
- Since this is a research prototype, the kernel itself can become unstable. Hence, we *recommend* building and using the kernel inside a virtual machine.
- Dependencies
  - openssl `sudo apt-get install libssl-dev`
- To compile the kernel, please use the script:
`./build_kernel.sh`
  - Note | This script will compile and install the kernel, then reboot the machine.
