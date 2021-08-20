n_threads=$1
trace_dir=$2

cd ../remap

#compile
g++ addr_remapper.cpp -o addr_remapper -lpthread

#make args
ARG=$n_threads
for i in $(seq 0 $(($n_threads - 1)))
do
    ARG="$ARG ${trace_dir}original/$i ${trace_dir}remapped/$i"
done

#run remap
./addr_remapper $ARG