# How to run test program
- To make all the shared memory space as 'shared' state (read only)
  - `make run_shared NODE_ID=[id of the current compute blade]`
- To make all the shared memory space as 'modified' state (writable)
  - `make run_modified NODE_ID=[id of the current compute blade]`
- We used the first blade as an evaluating blade of the test, which sends memory access requests and measure time to grant permission via MIND's cache coherence protocol.
  - To see the result, inside the first blade, we rely on in-kernel profiler located in [here](https://github.com/shsym/mind/tree/main/mind_linux/util_modules)
- This test program does not use any synchronization method between blades (unlike other usual tests in MIND)

## Examples
- To measure state transition latency from 7 sharers (shared state) to single writer (modified state)
  - Run `make run_shared NODE_ID=1` to `make run_shared NODE_ID=7` on the 2nd to 8th compute blades, respectively.
  - (wait for a few seconds to ensure state transition)
  - Run `make run_modified NODE_ID=0`
