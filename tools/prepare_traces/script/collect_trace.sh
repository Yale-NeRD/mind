#this script collects memory access trace
n_threads=$1
app=""
for i in "${@:2}"; do
    app="$app $i"
done

cd ../pin/source/tools/ManualExamples

#compile
make all TARGET=intel64

#collect
echo "start collecting trace for $app"
taskset --cpu-list 0-$(($n_threads - 1)) ../../../pin -t obj-intel64/pinatrace.so -- $app
