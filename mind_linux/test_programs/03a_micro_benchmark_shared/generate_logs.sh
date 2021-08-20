#!/bin/bash


for i in {0..7}
do

make run_generate_log_0_0     NODE_ID=$i  
make run_generate_log_0_25    NODE_ID=$i  
make run_generate_log_0_50    NODE_ID=$i 
make run_generate_log_0_75    NODE_ID=$i 
make run_generate_log_0_100   NODE_ID=$i 
make run_generate_log_25_0    NODE_ID=$i 
make run_generate_log_25_25   NODE_ID=$i  
make run_generate_log_25_50   NODE_ID=$i  
make run_generate_log_25_75   NODE_ID=$i  
make run_generate_log_25_100  NODE_ID=$i   
make run_generate_log_50_0    NODE_ID=$i  
make run_generate_log_50_25   NODE_ID=$i   
make run_generate_log_50_50   NODE_ID=$i   
make run_generate_log_50_75   NODE_ID=$i   
make run_generate_log_50_100  NODE_ID=$i   
make run_generate_log_75_0    NODE_ID=$i  
make run_generate_log_75_25   NODE_ID=$i  
make run_generate_log_75_50   NODE_ID=$i   
make run_generate_log_75_75   NODE_ID=$i  
make run_generate_log_75_100  NODE_ID=$i   
make run_generate_log_100_0   NODE_ID=$i    
make run_generate_log_100_25  NODE_ID=$i   
make run_generate_log_100_50  NODE_ID=$i  
make run_generate_log_100_75  NODE_ID=$i  
make run_generate_log_100_100 NODE_ID=$i   
done
