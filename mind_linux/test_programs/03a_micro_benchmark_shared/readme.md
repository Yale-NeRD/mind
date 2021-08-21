# How to run test program
- `make run_shared_[sharing ratio]_[read ratio] NUM_NODE=[number of blades] NODE_ID=[id of the current blade]`
  - Id of the current blade starts from 0 and must be less than `number of blades`
  - Sharing ratio: one of [0, 25, 50, 75, 100]
  - Read ratio: one of [0, 25, 50, 75, 100]
- When all `number of blades` compute blades start this program, the program synchonizes starting point by read and write to the shared memory space provided by MIND (since the synchronization process does not check whether the programs running over blades have the same experiment configuration or not, user needs to make sure they are using the same configuration).
