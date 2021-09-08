# Memory allocation simulation tool
- To test and compare *MIND*'s and traditional memory allocation. schemes, we designed this tool which simulates of memory allcation and calculates page table aggregation based on prefix-match

## Usage
For the memory allocation results of TensorFlow, Graphchi, and Memcached A/C traces (10 to 80 threads), please run:
- `./run_tf.sh`
- `./run_gc.sh`
- `./run_md.sh`

*Note*) Due to the computation time (especially for the GraphChi trace with 80 threads), simulation for 2 MB page tables are disabled by default. If you want to run it, please enable [3 lines in this part](https://github.com/shsym/mind/blob/83a8ae4e2bf7a2d9299ef02f80ed4486f21a4b64/tools/memory_allocation/main.py#L103).

- Even without actual simulation, the result for 2 MB page table can be estimated as follows:
  - Number of pages = [size of allocated memory] / 2 MB
  - Fairness = 1.0 (since the size of allocated memory > 1 GB, 2 MB pages can be evenly distributed over memory blades, almost perfectly)

## Result representation
The script will print out *Jain's fairness* and *number of address translation / memory protection* entries.
- **We inserted a mark `[RESULT]` with ![#c5f015](https://via.placeholder.com/15/c5f015/000000?text=+)green color, so that you can easily find the result.**


### Representation details
For *MIND*, the script first prints address translation and memory protection entries seprately, then prints the result; this result considers the optimization we implemented in the data plane of *MIND*'s programmalbe switch, which merges memory protection and exceptional entries.

Each allocation algorithm can be printed at the top of each log:
- 2 MB page table
```
==Simulation results==
SizeBalancingAllocator & FirstFitAllocator — granularity: 2097152 , second-layer-pgtable: False
```
- 1 GB page table (1 GB as a page size of the coarse-grained first level table)
```
==Simulation results==
 SizeBalancingAllocator & FirstFitAllocator — granularity: 1073741824 , second-layer-pgtable: True
```
- *MIND*
```
==Simulation results==
 SizeBalancingAllocator & UVAAllocator — granularity: 0 , second-layer-pgtable: False
```
