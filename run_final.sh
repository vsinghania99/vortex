#!/bin/bash

# Define arrays for threads, warps, and matrix sizes
matrix_sizes=(64 128 256 512)
tcsizes=(8 16 32)
tcnums=(4 8 16 32)
lsulanes=(4 16)
cores=(32)


# Loop through each combination of threads and warps
for lane in "${lsulanes[@]}"; do
    for size in "${matrix_sizes[@]}"; do
        sed -i "s/OPTS ?= -n[0-9]\+/OPTS ?= -n${size}/" tests/regression/matmul/Makefile
        echo "Matrix size changed to ${size} in Makefile"
        for tcsize in "${tcsizes[@]}"; do
            for tcnum in "${tcnums[@]}"; do
                for core in "${cores[@]}"; do
                    
                    log_name="sim_final/lsulanes${lane}/mat${size}/tcsize${tcsize}_tcnum${tcnum}_cores${core}_32w32t"
                    command="./ci/blackbox.sh --cores=${core} --app=matmul --driver=simx --threads=32 --warps=32 --tc_size=${tcsize} --tc_num=${tcnum} --lsu_lanes=${lane} --rebuild=1 --perf=1  > ${log_name} 2>&1"
                    echo "$command"
                    eval "$command"

                    #log_name="sim_final/lsulanes${lane}/mat${size}/tcsize${tcsize}_tcnum${tcnum}_cores${core}_16w16t"
                    #command="./ci/blackbox.sh --cores=${core} --app=matmul --driver=simx --threads=16 --warps=16 --tc_size=${tcsize} --tc_num=${tcnum} --lsu_lanes=${lane} --rebuild=1 --perf=1  > ${log_name} 2>&1"
                    #echo "$command"
                    #eval "$command"

                done
            done
        done    
    done
done