#!/bin/bash

# Define arrays for threads, warps, and matrix sizes
matrix_sizes=(16 32 64 128 256 512)
datasizes="4"

#num_tcs=(1 2 4 8)

#./ci/blackbox.sh --cores=1 --app=matmul --driver=simx --threads=32 --warps=16  --tc_size=2 --tc_num=2 --rebuild=1 --log=log3 --debug=3

# Loop through each combination of threads and warps
for size in "${matrix_sizes[@]}"; do
    for datasize in "${datasizes[@]}"; do
        sed -i "s/OPTS ?= -n[0-9]\+/OPTS ?= -n${size}/" tests/regression/matmul/Makefile
        # Generate command with specified threads, warps, and log name
        #tensor_core_size=2
        #while [ $tensor_core_size -le $size ]; do
        # Define log name based on threads and warps
        log_name="sim_results_datatypes/mat${size}/run32t32w_${datasize}bytes"
        command="./ci/blackbox.sh --cores=1 --app=matmul --driver=simx --threads=32 --warps=32 --tc_size=8 --tc_num=1 --rebuild=1 --perf=1  > ${log_name} 2>&1"
        echo "$command"
        eval "$command"

        log_name="sim_results_datatypes/mat${size}/run16t16w_${datasize}bytes"
        command="./ci/blackbox.sh --cores=1 --app=matmul --driver=simx --threads=16 --warps=16 --tc_size=8 --tc_num=1 --rebuild=1 --perf=1  > ${log_name} 2>&1"
        echo "$command"
        eval "$command"
        
        echo "Matrix size changed to ${size} in Makefile"
    done
done