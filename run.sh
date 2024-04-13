#!/bin/bash

# Define arrays for threads, warps, and matrix sizes
threads=(1 2 4 8 16 32)
warps=(1 2 4 8 16 32)
matrix_sizes=(64 128)

# Loop through each combination of threads and warps
for thread in "${threads[@]}"; do
    for warp in "${warps[@]}"; do
        for size in "${matrix_sizes[@]}"; do
            
            sed -i "s/OPTS ?= -n[0-9]\+/OPTS ?= -n${size}/" tests/regression/matmul/Makefile
            # Generate command with specified threads, warps, and log name
            tensor_core_size=2
            while [ $tensor_core_size -le $size ]; do
                # Define log name based on threads and warps
                log_name="sim_results_tc_size/mat${size}/run${thread}t${warp}w_mat${size}_tc${tensor_core_size}"
                command="./ci/blackbox.sh --cores=1 --app=matmul --driver=simx --threads=${thread} --warps=${warp} --debug=1 --log=${log_name} --tc_size=${tensor_core_size}"

                echo "Matrix size changed to ${size} in Makefile"
                # Run the command with the updated log name
                echo "$command"
                eval "$command"

                tensor_core_size=$((tensor_core_size * 2))
            done
        done
    done
done