trace_dir=../traces/
app=""
for i in "${@:1}"; do
    app="$app $i"
done

#compile
cd ../pin/source/tools/ManualExamples
make all TARGET=intel64

#collect
echo "start collecting syscall trace for $app"
../../../pin -t obj-intel64/malloctrace.so -- $app

