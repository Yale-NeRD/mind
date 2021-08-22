# @1 workload
# @2 num_nodes
# @3 num_threads
# @4 cache_line_size in B
# @5 dir_block_size in B
# @6 cache_size in B
# @7 tot_mem in B
# @8 interval of cache resizing in passes
# @9 performance target coefficient (integer > 0)
# @10 degree of split (1 for split 1 big block into 2 smaller blocks, 2 for 4, so on)
# @11 maximum number of directory

workload1="tensorflow"
workload2="voltdb"
workload3="random"
workload_gc="graphchi"
workload_ma="memcached_a"
workload_mc="memcached_c"
workload_sh="shared"

PRE=""
SUF=""

echo "workload to simulate: $1"
if [ $1 = $workload1 ]
then
    PRE="/home/yanpeng/2020_11_25_tensorflow/partitioned/"
elif [ $1 = $workload3 ]
then
    PRE="./test_logs/random_"
elif [ $1 = $workload_gc ]
then
    PRE="/home/yanpeng/2021_03_graphchi/partitioned/"
elif [ $1 = $workload_ma ]
then
    PRE="/media/data_ssds/memcached_a/partitioned/memcached_a_"
    SUF="_0"
elif [ $1 = $workload_mc ]
then
    PRE="/media/data_ssds/memcached_c/partitioned/"
else
    echo "unexpected workload"
fi

ARG="$2 $3 $4 $5 $6 $7 $8 $9 $10 $11"
for i in $(seq 0 $(($3 - 1)))
do
    ARG="$ARG $PRE$i$SUF"
done
echo $ARG

#compile
make

#run
mkdir -p logs
mkdir -p logs/rwcnt
mkdir -p logs/pso
mkdir -p logs/cdf
./SIMULATOR $ARG
