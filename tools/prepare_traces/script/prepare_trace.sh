trace_dir=../traces/
n_threads=$1
app=""
for i in "${@:2}"; do
    app="$app $i"
done

#collect
mkdir -p ${trace_dir}original
./collect_trace.sh $n_threads $app

#remap
mkdir -p ${trace_dir}remapped
./remap_addr.sh $n_threads $trace_dir
