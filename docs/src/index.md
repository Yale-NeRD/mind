# MIND: In-Network Memory Management for Disaggregated Data Centers

## Overview
MIND is an in-network memory management system for compute-memory disaggregation.

![type:video](https://www.youtube.com/embed/Gm43N_0UMQM)

## What is resource disaggregation and its benefits?
Resource disaggregation physically separates compute and memory into network-attached resource blades, which can provide the following benefits compared to traditional data center architectures:

* Higher resource utilization
* Better support for hardware heterogeneity
* Resource elasticity
* Failure handling

<!-- ## Challenges in compute-memory disaggregation -->
## In-network memory management
MIND leverages programmability of network to achieve high performance, elastic scalability, and transparent virtual address abstraction. Unlike prior disaggregated memory designs, MIND places all logic and metadata for memory management in the network which include memory allocation, address translation, memory protection, and cache coherence. In fact, such many memory management functionalities have similar counterparts in networking allowing MIND to leverage decades of innovation in net- work hardware and protocol design for disaggregated memory management.

**Assumption**: MIND assumes partial memory disaggregation where compute blades possess a small amount (few GBs) of local DRAM as cache. All memory LOAD/STORE operations from the user processes are handled by this DRAM cache.

### Control plane operations (slow path)
MIND's kernel (modified Linux) captures memory management-related system calls (`mmap`, `munmap`, etc.) and forwards them to the programmable switch. Similar to how Linux manages handles memory allocations, MIND's control plane program running on the programmable switch maintains simplified version of `vm_area_struct`, `struct mm_struct`, and `task_struct`. Instead of updating page table entries (in Linux), MIND updates address translation and memory protection tables in the programmable switch ASIC.

### Data plane operations (fast path)
When DRAM cache miss happens, MIND's page fault handler generates and sends a data request to the programmable switch. The request is served by the programmable switch ASIC following the address translation and memory protection rules. In addition, MIND realizes in-network cache coherence protocol by maintaining cache directory entries inside the switching ASIC. Finally, the request (or generated cache invalidation requests) is forwarded to its destination—one of the memory blades (or the compute blade having dirty data)—to fetch data.
<!-- % To overcome the limited in-network compute and memory resources, we designed MIND -->

## Looking ahead
MIND is a first step toward our vision, an OS for resource disaggregation which can abstract the entire edge data center as a single computer for applications.
