# How to run test program
- Simple test in a single node:
  - `make run NODE_ID=1`
- Simple test in a multiple node:
  - Inside first node `make run_uni_node1` for unipage test, `make run_multi_node1` for multipage test
  - Inside second node `make run_uni_node2` for unipage test, `make run_multi_node2` for multipage test

# Yanpeng's tutorial for the test program (may be old version)
## Cache Coherence Protocol

1. **how to run**

   we can test the cache coherence protocol with the following steps~

   1. **preparation**

      The kernel and modules we need to use are all from branch `DEV_ccp_hw_NIC`.

      And we need at least 4 VMs. Here are the VMs I'm using:

      | computing node                     | controller node | memory node   |
      | ---------------------------------- | --------------- | :------------ |
      | compute_node_test & compute_node_2 | ubuntu18.04     | mem_node_test |

      Note that there are some IP addresses that you may need to modify if you do not use my VMs, they are in the first few lines in`kernel/network_disagg.c` and `ctrl_module/controller/controller.c`. You also need to change the `disagg_computing_node_id`in file `kernel/network_disagg.c`to be 2 and 3 respectively on 2 computing nodes(it should be 1 in the original repo).

      The only difference in the kernel code for general test and high contention test is which evicting thread we launch in `kerne/cnthread_disagg.c`. Please launch the regular one(`cnthread_handler`) to do general test and the other one(`cnthread_test_evict_handler`) to do high contention test.

   2. **boot the VMs**

      The VMs should be booted in the order of `controller node`, `memory node` and `computing nodes`. 

      **Note that** for convenience reasons, the computing node whose `disagg_computing_node_id`in file `kernel/network_disagg.c` is 2 have to be booted before the one whose is 3. For example, in my settings,`compute_node_test`(whose ip address is `0xca150a0a` in hexadecimal and network endian ) has to be booted prior to `compute_node_2`(`0xc9150a0a`).

      If everything works well with regard to booting, you should see the follwing log in the kernel logs of `controller node` for RDMA:

      ```
      ...
      All nodes are joined - total [1]
      ```
   
      and 2 logs for TCP that looks like this:

      ```
      ...
      Disagg-FIT: New conn: connection from 10.10.21.202:40930 - thread id 1
      Disagg-FIT: TCP: Received opcode: 15
      Disagg-FIT: IS_SEND_CONN - received: 9, id is 1
      Response has been sent (Ret code: 9)
      Disagg-FIT: New conn: connection from 10.10.21.201:47918 - thread id 2
      Disagg-FIT: TCP: Received opcode: 15
      Disagg-FIT: IS_SEND_CONN - received: 9, id is 2
      Disagg-FIT: Response has been sent (Ret code: 9)
      ```
   
      **Note that** after switching to hardware NIC, those 2 logs for TCP may show up after a few minutes after the computing node is booted(I think this is anomalous but after they show up everything works well). So you may need to be very patient and wait if you use the hardware NIC...If you do not see the 2 logs after too long a time, then please reset the test environment. After seeing these logs, just log in both `computing nodes` as `user_test`.

   3. **run the test program**

      The same test program will be run on different computing nodes to test the cache coherence protocol. The test program and it's periferal files are packed into folder `test_programs`, which locates at the root directory of the project. For convenience reason, I have also copied it to the root directory of `user_test`.

      We can simply compile it by `make`. The binary test program `test_protocol`will be in directory `test_programs/bin`after compilation, we **must not change the name** of the binary in order that our system could detect it.

      The test program receives 3 parameters, which are the 2 trace files and 1 integer parameter, which can be either 1 or 0. 1 indicates that the current test program is the first to be launched and 0 otherwise. Specifically, you only **need to and must give a 1 to the very first test program** you run and 0 to other test programs running on other machines. 

      It should not matter on which computing node you run the first test program. However, I have only tested the case that we run the first test program on the first computing node(`compute_node_test` for me).

      After the first test program starts running, it will print out some inforamtion, 2 of which deserve some attention: 

      1. something like `sleeping 0, please run the same executable on other machines now...`This message show up for 10 seconds and **within this 10 seconds all test programs on other computing nodes have to be launched**. After seeing this message, **the faster you launch other test programs, the larger portion of memory accesses will be overlapped among different test programs**. Launching other test programs too early or too late can both cause potential problems since I currently don't have any explict synchronization mechanism.

      2. the address of the shared buffer. Because other test programs on other computing nodes will also print out this information. This address has to be consistent acorss all test programs. Or you might have launched the second test program too early or too late.

   4. **the test program**

      The test program reads the trace files in `test_programs/trace/` and launch 2 threads to do concurrent random load/store according to these files in the shared VMA. So we can change the trace files to adjuct the behavior of the test programs. 

      After the memory accesses, it will generate 2 result files in `test_programs/res/`, which contain several lines of string in the format of ``access_type addr value`, indicating the value(`value`) it read/write to/from address(`addr`) on memory access(`access_type`).

   5. **trace files**

      The trace file starts with an integer indicating the number of total traces in the file. Each trace is denoted by a triple in the format of `access_type address value`, in which `access_type` can be either "r" or "w" of char type. `address` is an integer offset to the start address of shared VMA. `value` is 0 if `access_type` is "r" or the value we wirte to the shared VMA if `access_type` is "w".

      There are lots of different trace files, whose features are indicated by their name, which is in the format of `[num_pages]Page_align[align]_tid[tid]_r2w[r2w_ratio]`. The meaning of the variables in the name of trace files are listed below:

      - `num_pages`: how many pages(cache lines) this trace covers. It currently can only be either "uni" or "multi", which are used for high contention test and general test respectively.
   - `align`: determine what addresses are valid to access together with `tid`.
      - `tid`: the thread id of the thread that use this trace, typically ranging from 0 to 3. A trace file may only cover the addresses that remain `tid` when divided by `align` , so that no different threads can access the same address to create races.
   - `r2w_ratio`: the read to write ratio in the trace file, which can be 0.01, 0.5 or 0.9.
   
   For example, a trace file that is used in high contention test by thread 0 and has a read to write ratio of 0.5 has the name:`uniPage_align4_tid0_r2w0.5`
   
6. **verify the test result**
   
      After test program finishes, we can check the result files in `test_programs/re`s(`res1.txt` generated by thread 1 and `res2. txt` generated by thread 2) by `verify_res.py` in `test_programs/res.` 
   
      The script receives 2 parameters, which are the name of the trace file and res file respectively. For example, when we try to check the result of thread 1 that use trace`uniPage_align4_tid0_r2w0.5`in the test , then we can switch to `test_programs/res` and use the following command:

      `python3 verify_res.py ../trace/uniPage_align4_tid0_r2w0.5 res1.txt`

      If the cache coherence protocol pass the test, the script will print `test passed`. Otherwise it will print out the expected value and the wrong value we get for those failed read.

   7. **an example**

      Let's go through the whole process of conducting a general test after we boot the VMs and log in both computing nodes as user_test ~

      Note that in order to do general test, we launch the regular evicting thread(`cnthread_handler`) in computing node's kernel. Also, if you fail to log in(rarely happen), please reset the test environment.

      Firstly, run the following 2 commands on computing node 1 to launch the first test program:

   ```
   cd test_programs/bin
      ./test_protocol ../trace/multiPage_align1024_tid0_r2w0.5 ../trace/multiPage_align1024_tid1_r2w0.5 1
   ```
   
   You may then see:
   
   ```
      loading trace...
   trace len is: 100000
      trace len is: 100000
   sleeping 0, please run the same executable on other machines now...
      ...
      ```
   
      After you see that, launch another test_program on another computing node as soon as possible:

      ```
   cd test_programs/bin
      ./test_protocol ../trace/multiPage_align1024_tid2_r2w0.5 ../trace/multiPage_align1024_tid3_r2w0.5 0
      ```
   
      Now both test programs are running, they may print some information about their process like:
   
      ```
      0
   1000
      ...
   ...
      98000
      99000
      printing result...
      done
   ```
   
   After the test programs terminate, run the following 2 commands on computing node 1 to verify the results of the 2 threads on computing node 1:
   
      ```
      cd ../res
      python3 verify_res.py ../trace/multiPage_align1024_tid0_r2w0.5 res1.txt
      python3 verify_res.py ../trace/multiPage_align1024_tid1_r2w0.5 res2.txt
      ```
   
      Also, do similar things on computing node 2:
   
      ```
   cd ../res
      python3 verify_res.py ../trace/multiPage_align1024_tid2_r2w0.5 res1.txt
   python3 verify_res.py ../trace/multiPage_align1024_tid3_r2w0.5 res2.txt
      ```
   
      If everything works well, you are supposed to see 2 `test passed` on each computing node. If you want to conduct another test with different traces, you may need to reset the whole environment.
   
   8.  

      

2. **implementation details**

   Let's go through the details of my inplementation now. They include 2 parts of code:

   - code for cache coherence protocol, which you may be able to adapt to other branches with little or no modification(and which I think is the most important).
   - code for testing purpose, which are irrelevant to the cache coherence protocol, but just for the test. e.g. code for memory address sharing, code for detecting and specially handling protocol testing program and so on.

   1. **code for cache coherence protocol**

      1. **logical design**

         Logically, the directory based MSI cache coherence protocol is implemented based on the following state transition matrixs respectively in controller and computing nodes.

         ###### directory based MSI protocol state transition matrix for computing node

         |        | **read**                     | **write**                     | evict                   | **req**                | inv                   | **req** **&** **inv**      | **ack**(+data) |
         | ------ | ---------------------------- | ----------------------------- | ----------------------- | ---------------------- | --------------------- | -------------------------- | -------------- |
         | **I**  | send read miss  <br />**IS** | send write miss  <br />**IM** | X                       | X                      | X                     | X                          | X              |
         | **IS** | stall                        | stall                         | stall                   | X                      | stall                 | X                          | **S**          |
         | **IM** | stall                        | stall                         | stall                   | stall                  | X                     | stall                      | **M**          |
         | **S**  | hit                          | send write miss <br />**SM**  | send evict <br />**SI** | X                      | send ack <br />**I**  | X                          | X              |
         | **SM** | hit                          | stall                         | stall                   | stall                  | send ack <br />**IM** | stall                      | **M**          |
         | **M**  | hit                          | hit                           | send evict <br />**MI** | send data <br />**S**  | X                     | send ack+data<br />**I**   | X              |
         | **SI** | stall                        | stall                         | X                       | X                      | send ack <br />**II** | X                          | **I**          |
         | **MI** | stall                        | stall                         | X                       | send data <br />**SI** | X                     | send ack+data <br />**II** | **I**          |
         | **II** | stall                        | stall                         | X                       | X                      | X                     | X                          | **I**          |

         ###### directory based MSI protocol state transition matrix for controller node

         |       | read miss                                                    | write miss                                                   | evict                                                        |
         | ----- | ------------------------------------------------------------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
         | **I** | 1.add sender to sharers<br />2.read data from memory<br />3.send data to sender<br />**S** | 1.set sender as owner<br />2.read data from memory<br />3.send data to sender<br />**M** | X                                                            |
         | **S** | 1.add sender to sharers<br />2.read data from memory<br />3.send data to sender<br /> | 1.send inv to sharers<br />2.clear sharers<br />3.set sender as owner<br />4.read data from memory<br />5.send data to sender<br />**M** | 1.remove sender from shares<br />**I**                       |
         | **M** | 1.set sender and old owner as sharers<br />2.request data from owner<br />3.push data to memory<br />4.send data to sender<br />**S** | 1.clear owner<br />2.set sender as owner<br />3.send inv and req to owner<br />4.send data to sender | 1.remove sender's ownership<br />2.push data to memory<br />**I** |

      2. **core functions**

         The functions that implement the behaviors denoted by state transition matrix are as follow.

         ###### functions to handle cache events in computing node

         | cache event | function                                                     | location of implementation |
         | ----------- | ------------------------------------------------------------ | -------------------------- |
         | read/write  | do_disagg_page_fault                                         | arch/x86/mm/fault_disagg.c |
         | evict       | cnthread_evict_one(general)<br />cnthread_evict_test_page(high contention) | kernel/cnthread_disagg.c   |
         | req         | handle_request                                               | kernel/cnthread_disagg.c   |
         | inv         | handle_invalidate                                            | kernel/cnthread_disagg.c   |
         | req & inv   | handle_request_and_invalidate                                | kernel/cnthread_disagg.c   |
         | ack(+data)  | same with the event that ack is to                           |                            |

         ###### functions to handle cache events in controller node

         | cache event | function                 | location of implementation               |
         | ----------- | ------------------------ | ---------------------------------------- |
         | read miss   | read_directory           | ctrl_module/controller/cache_directory.c |
         | write miss  | write_directory          | ctrl_module/controller/cache_directory.c |
         | evict       | push_page_data_directory | ctrl_module/controller/cache_directory.c |

         Among these functions, are all new APIs except for `do_disagg_page_fault` and `__cnthread_evict_one`. However, their behavior should not be very difficult to understand, for they purely follow the state transition matrix and they have break their jobs into small helper functions, whose sub-jobs are easy to understand by their name. What might be confusing is that some times you can see large chunk of `if-else` clause. Those are codes to handle transient states.

      3. **data structures**

         In controller node, directories are represented by struct `dir_struct` and directory entries are represented by struct `entry_struct `as shown below:

         ```
         struct dir_struct {
         	struct entry_struct *entry_list;
         	struct task_struct *tsk;
         	u16 tgids[DIR_MAX_NODE_NUM];
         	spinlock_t dir_lock;
         };
         
         struct entry_struct {
         	struct dir_struct *dir;
         	u64 addr;
         	u64 ownership;
         	int modified;
         	UT_hash_handle hh;
         };
         ```

         As reported previously, each task, though may run on multiple machines, will use one directory since all memory sharing are within task. So, each `dir_struct` will be held by a `utgid_node  `   and inside struct `dir_struct`, there is a member `tsk` pointing to the task that currently use the directory. Member `entry_list` can be regarded as the hash table used by `uthash`, an open source hash table implementation to store directory entries. Member `tgids` stores the tgid of the task when it runs on different machines. For example, if the same task's tgid is 1500 on computing node 1 and 1700 on computing node 2, then `tgids[compute_node_1_id] == 1500` and `tgids[compute_node_2_id] == 1700`.

         Each `entry_struct` has a member `modified` as dirty bit and `ownership`to indicate the ownership of the page. Member`hh` is just a handle used by `uthash`.

      4. **directory updates on syscall**

         As mentioned earlier, besides the nromal cache events, the directory will also need to be initialized or updated on memory related syscalls. I verify these implemenst only following the rule that no inconsistency between controller and computing node is detected(they are in accordance regarding which page is cached and which are not). Since I do not fully understand how our system distributes a proccess's pages on each syscall, I think you may need to rethink about this part of the behavior when you want to use it.

         | syscall                               | directory behavior                                           | location of implementation                                   |
         | ------------------------------------- | ------------------------------------------------------------ | ------------------------------------------------------------ |
         | fork                                  | if parent directory exists, initialize the child directory by copying father directory. Otherwise, initialize a empty directory for child. | function `mn_common_new_task_mm` in file `ctrl_module/controller/memory_management.c` |
         | exec                                  | clear the directory                                          | function `mn_update_mm` in file `ctrl_module/controller/memory_management.c` |
         | exit                                  | clear and free the directory                                 | function `mn_delete_task` in file `ctrl_module/controller/request_handler.c` |
         | mmap<br />munmap<br />mremap<br />brk | free entries corresponding to the unmmapped area             | function `mn_munmap` in file `ctrl_module/controller/mmap_ftns.c` |

         

   2. **code for testing purpose**

      This part of code some times can be messy and dummy...Let me go through them in the order of time that they come in to picture in the test.

      1. **booting time**

         On booting time, the computing node will initialize another TCP connection dedicated to contol message sending and receiving. e.g. invalidate or invalidate-ack messages. This extra TCP connection should ne necessary to avoid deadlock. After the initialization the computing node will send a `DISSAGG_IS_SEND_CONN` to controller through function `kernel/network_disagg.c/send_msg_to_controller`  to explicitly tell the controller that this socket is used to send protocol related controll messages. After that, the computing node will just wait for control messages from controller node and call handlers(e.g. `handle_invalidate`) to handle them. After successfully establishing this connection, you are supposed to see the following in computing nodes's log.

         ```
         Disagg-FIT: coherence protocol - start handling request from controller
         ```

         and the following in controller node's log as I mentioned in section 1.

         ```
         Disagg-FIT: New conn: connection from 10.10.21.202:40930 - thread id 1
         Disagg-FIT: TCP: Received opcode: 15
         Disagg-FIT: IS_SEND_CONN - received: 9, id is 1
         Response has been sent (Ret code: 9)
         ```

      2. **test program launching time**

         Most of the messy code are about to show up now. Since we do not have an actual cross-machine memory sharing mechanism now, In losts of places the test program and it's syscalls or memory accesses need to be specially detected and handled by the `ctrl_module` in order to conduct the test.

         After the first computing node launch the test program, the ctrl module will detect it in function `ctrl_module/request_handler.c/__handle_exec` by its name "test_protocol"(that's why we can't change the binary name now) and record its tgid in several variables. Also, the shared testing address space denoted by a new `utgid_node` named `protocol_tester_tgnode`will be initialized by calling function `init_protocol_tester_tgnode`. 

         After the second computing node launch the test program, the ctrl module will also detect it in function `ctrl_module/request_handler.c/__handle_exec` by its name. However, the ctrl module only record its tgid but does not initilize the shared address space afterward.

      3. **test program mmapping test area time**

         After the test programs are launched and before they mmap an area in shared address space, they will just run as they do in your old branch `DEV_cache_opt`. However, when the first test program trys to mmap an area of a special magic size(currently 9999 * 4096), the ctrl module will detect that and allocates this area in the shared address space as the test area. Afterwards when the second test program also trys to mmap an area of this magic size, the ctrl module will simply return the starting address of the test area. 

         These behaviors are implemented in func `ctrl_module/request_handler.c/__handle_mmap`.

      4. **concurrent memory accessing time**

         The test programs will start memory accessing as soon as the mmap syscall returns. So, since I currently do not have any explicit synchronizations, the faster you launch the second test program, the larger portion of the memory accesses will happen concurrently. So I strongly recommend you to prepare the command in terminal for the second test program first and press "enter" to run it as long as you run the first one(This sounds really dummy......And for me about 7000 memory access can be wasted).

         While they access the shared test area and page faults are forwarded to the controller, the ctrl module will detect those accesess to the test area by their fault address and forward them to the shared address space, in which they will be handled by the cache coherence protocol.

         These behaviors are implemented in func `ctrl_module/request_handler.c/__handle_pfault`.

      5. **evicting time**

         As victims are pushed to controllers, the ctrl module will also detect those pages that belongs to the test area by their address and forward them to the shared address space.

         These behaviors are implemented in func`ctrl_module/request_handler.c/__handle_data`.

      6. **exit time**

         Actually this part of code is never important to me because I reset the test environment whenever the test program finish...So I'm not confident in its correctness. But anyway, I free the  shared address space `protocol_tester_tgnode`and reset a few variables after both test programs terminate.

         These behaviors are implemented in func`ctrl_module/request_handler.c/mn_delete_task`.
