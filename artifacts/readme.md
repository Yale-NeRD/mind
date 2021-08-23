# Artifact evaulation
### NDA Requirements
- To compile and run the programmable switch side programs by your own, NDA with Intel is required.
- To build *MIND*'s RoCE (RDMA over Converged Ethernet) module, you must agree with End-User Agreement for Mellanox OFED Software [(link to agreement and driver download)](https://www.mellanox.com/page/mlnx_ofed_eula?mtag=linux_sw_drivers&mrequest=downloads&mtype=ofed&mver=MLNX_OFED-5.0-1.0.0.0&mname=MLNX_OFED_LINUX-5.0-1.0.0.0-ubuntu18.04-x86_64.tgz)

### HW Requirements
- To run simulations, one may need up to 120 threads running in parallel.
- Default cluster environment consists of 8 compute blade VMs, 1 memory blade VM, and 1 programmable switch. We used 100 Gbps link for all connections between VM and the switch; VMs are distributed over 4 compute servers and 1 memory server.
- We enabled IO-MMU and used PCIe passthrough to attach NIC to VMs.

### Notes
- Since we used very huge memory traces (around 8 TB), simulator will take a few days to create a data point / log.
Therefore, we also included sample output files from the simulator, so that you can quickly run analyzer scripts with the sample files to see the results.
- To mitigate dependency and configuration complexicy (NIC drivers, IO-MMU anc VM setup), we are preparing VMs on CloudLab which can run *MIND*'s kernel.
As the VM also contains *THIS* source code, you can pull the recent commit and build our own kernel on the VMs.

## Artifact evaulation instructions
- *Update) When CloudLab experiment setup is ready (with a programmable switch), we will add CloudLab specific instructions and scripts.*

### How to build MIND
- Please find the link to each system component below.
1. [MIND kernel](https://github.com/shsym/mind/tree/master/mind_linux)
2. Switch program
    - [Dependencies](https://github.com/shsym/mind/blob/master/mind_switch_ctrl/dependencies.md)
    - [Control plane C/C++ program](https://github.com/shsym/mind/tree/master/mind_switch_ctrl)
    - [Data plane P4 program](https://github.com/shsym/mind/tree/master/mind_p4)

### How to run experiments
We listed up the list of evaluations included in this repo and *a link to more specific instructions* as follows:

1. [Performance with Tensorflow, GracphChi, and Memcached memory access traces](https://github.com/shsym/mind/tree/master/ctrl_scripts)
    - [Modified Intel PIN tool for collecting memory traces](https://github.com/shsym/mind/tree/master/tools/prepare_traces)
      - *You can skip this preparation step by directly using the memory traces provided via CloudLab or our cluster at Yale*
    - In the compute blade, end-to-end running time will be presented (Fig. 6)
    - In the programmable switch, number of directory entries over time will be presented (Fig. 8, left)
    - [PSO/PSO+ simulator](https://github.com/shsym/mind/tree/master/tools/pso_estimator) (part of Fig. 6)
2. [Latency measurements for state transision cases (Fig. 7, left)](https://github.com/shsym/mind/tree/master/ctrl_scripts)
3. [Benchmark with various sharing and read/write ratios (Fig. 7, right)](https://github.com/shsym/mind/tree/master/ctrl_scripts)
4. [Memory allocation test with memory allocation traces (Fig. 8, center and right)](https://github.com/shsym/mind/tree/master/tools/memory_allocation)
5. [Cache coherence protocol simulations (for the measurement under infeasible configurations in real switch / x86 architecture)](https://github.com/shsym/mind/tree/master/tools/cache_coherence_sim)
    - Sensitivity analysis for bounded splitting algorithm (Fig. 9)
    - PSO and PSO+ simulation (Part of Fig. 7, right)
