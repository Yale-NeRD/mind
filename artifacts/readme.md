# Artifact evaulation

## Summary
- *Please first check NDA and HW requirements*: a programmable switch, Mellanox NICs and corresponding NDA.
- We recommend using our VMs on Yale cluster to minimize effort to create VMs and manually build kernel on them.
  - You can try to rebuild kernel inside those VMs to verify completeness of the source code (we have a script for [rebuild](https://github.com/shsym/mind/blob/main/ctrl_scripts/readme.md#re-build-mind-kernel)).
- Since some of the experiments use the memory access traces (over 6 TB), it would take more than a week to reproduce all data points for MIND and the compared systems (FastSwap and GAM). To mitigate this overhead:
  - For the performance evaluation with memory traces, which is the main results of the paper (Fig. 6), the memory trace replayer exposes an input parameter to limit the length of the experiment. For example, by using 1/10 of the traces, the experiments for 4 applications and 3 systems would be done within 15 hours (roughly).
    - Even though the results from shorter memory traces would not be the exactly same values in the paper, we believe overall performance trend would be consistent (since we reported normalized values in the paper).
  - For the cache coherence protocol emulation, we prepared pre-computed output results.
  - For PSO/PSO+ estimation, the pre-computed output from the cache coherence protocol and other pre-computed input data can be used to feed the estimator.
- We have scripts for automating evaluations (preliminary setup is needed for the cluster such as VMs and physical Ethernet links between the NICs and the switch; the old guide we used for artifact evaluation in pre-configured Yale's cluster [located in here](https://github.com/shsym/mind/blob/main/artifacts/how_to_yale.md)).
---

### NDA Requirements
- To compile and run the programmable switch side programs by your own, NDA with Intel is required.
- To build *MIND*'s RoCE (RDMA over Converged Ethernet) module, you must agree with End-User Agreement for Mellanox OFED Software [(link to agreement and driver download)](https://www.mellanox.com/page/mlnx_ofed_eula?mtag=linux_sw_drivers&mrequest=downloads&mtype=ofed&mver=MLNX_OFED-5.0-1.0.0.0&mname=MLNX_OFED_LINUX-5.0-1.0.0.0-ubuntu18.04-x86_64.tgz)

### HW Requirements
- To run simulations, one may need up to 120 threads running in parallel.
- Default cluster environment consists of 8 compute blade VMs, 1 memory blade VM, and 1 programmable switch. We used 100 Gbps link for all connections between VM and the switch; VMs are distributed over 4 compute servers and 1 memory server.
- We enabled IO-MMU and used PCIe passthrough to attach NIC to VMs.

## Artifact evaulation instructions
### How to build MIND
- Please find the link to each system component below.
1. [MIND kernel](https://github.com/shsym/mind/tree/main/mind_linux)
2. Switch program
    - [Dependencies](https://github.com/shsym/mind/blob/main/mind_switch_ctrl/dependencies.md)
    - [Control plane C/C++ program](https://github.com/shsym/mind/tree/main/mind_switch_ctrl)
    - [Data plane P4 program](https://github.com/shsym/mind/tree/main/mind_p4)

### How to run experiments
We list up the evaluations included in this repo and *a link to more specific instructions* as follows.
- We use this marker ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+) to notify script for the evaluations
1. [Performance with Tensorflow, GracphChi, and Memcached memory access traces](https://github.com/shsym/mind/tree/main/ctrl_scripts)
    - [Modified Intel PIN tool for collecting memory traces](https://github.com/shsym/mind/tree/main/tools/prepare_traces)
      - *You can skip this preparation step by directly using the memory traces provided via our cluster at Yale*
    - In the compute blade, end-to-end running time will be presented (Fig. 6)
    - In the programmable switch, number of directory entries over time will be presented (Fig. 8, left)
    - [PSO/PSO+ simulator](https://github.com/shsym/mind/tree/main/tools/pso_estimator) (part of Fig. 6)
2. [Latency measurements for state transision cases (Fig. 7, left)](https://github.com/shsym/mind/tree/main/ctrl_scripts)
3. [Benchmark with various sharing and read/write ratios (Fig. 7, right)](https://github.com/shsym/mind/tree/main/ctrl_scripts)
4. [Memory allocation test with memory allocation traces (Fig. 8, center and right)](https://github.com/shsym/mind/tree/main/tools/memory_allocation)
5. [Cache coherence protocol emulations (for the measurement under infeasible configurations in real switch / x86 architecture)](https://github.com/shsym/mind/tree/main/tools/cache_coherence_sim)
    - Sensitivity analysis for bounded splitting algorithm (Fig. 9)
    - PSO and PSO+ simulation (Part of Fig. 7, right)
