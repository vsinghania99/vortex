#!/bin/bash
 
nwarps="1 2 4 8 16 32"
nthreads="1 2 4 8 16 32"
matsize="8 16 32 64"
 
data_type="8"
# Loop over each item in the variable
for size in ${matsize[@]}; do
    sed -i "8 s/-n[0-9]\+/-n$size/" tests/regression/matmul/Makefile
    echo "Matrix Size = ${size}"
    for warps in ${nwarps[@]}; do
        for threads in ${nthreads[@]}; do      
            logfile="results_databytes/run_${size}MatSize/run_${warps}W_${threads}T_${data_type}Bytes.log"
            command="./ci/blackbox.sh --cores=1 --app=matmul --driver=simx --threads=${threads} --warps=${warps} --debug=1 --log=${logfile}"
            eval "$command"
        done
    done
done
 