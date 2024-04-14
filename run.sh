#!/bin/bash

# Define arrays for threads, warps, and matrix sizes
threads=(1 2 4 8 16 32)
warps=(1 2 4 8 16 32)
matrix_sizes=(16 32 64 128 256 512)

#./ci/blackbox.sh --cores=1 --app=matmul --driver=simx --threads=32 --warps=16  --tc_size=2 --tc_num=2 --rebuild=1 --log=log3 --debug=3

# Loop through each combination of threads and warps
for size in "${matrix_sizes[@]}"; do
    for warp in "${warps[@]}"; do
        for thread in "${threads[@]}"; do
            sed -i "s/OPTS ?= -n[0-9]\+/OPTS ?= -n${size}/" tests/regression/matmul/Makefile
            # Generate command with specified threads, warps, and log name
            #tensor_core_size=2
            #while [ $tensor_core_size -le $size ]; do
                # Define log name based on threads and warps
                log_name="sim_results_thread_opt/mat${size}/run${thread}t${warp}w_mat${size}_tc${tensor_core_size}"
                command="./ci/blackbox.sh --cores=1 --app=matmul --driver=simx --threads=${thread} --warps=${warp} --tc_size=4 --tc_num=1 --rebuild=1 --perf=1  > ${log_name} 2>&1"

                echo "Matrix size changed to ${size} in Makefile"
                # Run the command with the updated log name
                echo "$command"
                eval "$command"

                #tensor_core_size=$((tensor_core_size * 2))
            #done
        done
    done
done