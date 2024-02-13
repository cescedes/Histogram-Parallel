#!/bin/bash

for i in 10 20 40 80 160 320 640 1280 2560
do 
    # echo "[configuration]: threads: 32, N: $i"
    srun --nodes=1 $1 --N $i --num-threads 32 --print-level 0
done

