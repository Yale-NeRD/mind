# MIND: In-Network Memory Management for Disaggregated Data Centers

## Overview
MIND is an in-network memory management system for compute-memory disaggregation.

![type:video](https://www.youtube.com/embed/Gm43N_0UMQM)

## What is compute-memory disaggregation?
Compute-memory disaggregation physically separates compute and memory that typically resides in a single physical server into network-attached resource blades. Such an architecture can provide the following benefits compared to traditional (server-based) data center architectures:

* Higher resource utilization
* Better support for hardware heterogeneity
* Resource elasticity
* Better failure handling

<!-- ## Challenges in compute-memory disaggregation -->
## In-network memory management
MIND leverages programmability of network to achieve high performance, compute and memory elasticity, and transparent virtual memory abstraction. Unlike prior disaggregated memory designs, MIND places the logic and metadata for memory management (memory allocation, address translation, memory protection, and cache coherence) _in the network fabric_. Interestingly, we found many memory management functionalities to already have similar counterparts in the networking domain, allowing MIND to leverage decades of innovation in network hardware and protocol design to enable efficient disaggregated memory management.

MIND assumes partial memory disaggregation where compute blades possess a small amount (few GBs) of local DRAM as cache. All memory LOAD/STORE operations from the user processes are handled by this DRAM cache. MIND distinguishes between its control plane components (which are realized in a centralized processor on the switch) and its data plane components (which are realized in specialized hardware, specifically the programmable switch ASIC).

### Control plane operations
MIND's kernel (modified Linux) captures memory management-related system calls (`mmap`, `munmap`, etc.) and forwards them to the programmable switch. Similar to how Linux manages handles memory allocations, MIND's control plane program running on the programmable switch maintains a global memory allocation metadata, e.g., simplified realizations of `vm_area_struct`, `mm_struct`, and `task_struct`. Based on the global metadata, MIND updates address translation and memory protection tables in the programmable switch ASIC.

### Data plane operations
When a memory access incurs a miss in the compute blades local DRAM cache, MIND's page fault handler generates and sends a data request to the programmable switch. The request is served by the programmable switch ASIC following the address translation and memory protection rules. In addition, MIND realizes in-network cache coherence protocol by maintaining cache directory entries inside the switching ASIC. Finally, the request (or generated cache invalidation requests) is forwarded to its destination --- to one of the memory blades (in case the data is not cached at any compute blade) or the compute blade that has the dirty data (in case a cache invalidation is required).
<!-- % To overcome the limited in-network compute and memory resources, we designed MIND -->

## Looking ahead
MIND is a first step toward our vision, an OS for resource disaggregation which can abstract the entire edge data center as a single computer for applications. In MIND, we focused on memory subsystem, particularly the shared memory abstraction between compute blades. We are working on other OS components such as process management and virtualization.

Please see our [SOSP'21](https://dl.acm.org/doi/10.1145/3477132.3483561) paper ([:material/projector-screen-outline.svg:](https://github.com/shsym/mind/raw/docs/docs/src/mind_sosp_21_fin.pdf)) and [GitHub repo](https://github.com/shsym/mind) for more details!

