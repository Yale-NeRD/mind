# MIND In-Network Memory Management for Disaggregated Data Centers
In this research project, we leverage programmable network switches to enable an efficient shared memory abstraction for disaggregated data centers by placing memory management logic in the network fabric.

## Artifact Evaluation
### [→ Please find our instructions here](https://github.com/shsym/mind/tree/main/artifacts)

## Dependency & Requirements
- System architecture: x86-64bit
- NIC: Mellanox ConnectX-5
  - To compile MIND's RDMA/RoCE kernel module, NIC driver (mlnx_ofed 5.0-1) must be installed first: [please use archive version in this link](https://www.mellanox.com/products/infiniband-drivers/linux/mlnx_ofed) 
- Base OS: we tested MIND with Ubuntu server 18.04 LTS of which the default kernel is 4.15.x. 
- Since this is a research prototype and can become unstable, we *recommend* building and using the kernel inside a virtual machine.
