# How to run test program
- `make run_shared_[sharing ratio]_[read ratio]`
- We assumed 8 compute blades. When all 8 blades start this program, the program synchonizes starting point by read and write to the shared memory space provided by MIND (since the synchronization process does not check whether the programs running over blades have the same experiment configuration or not, user needs to make sure the configuration).
