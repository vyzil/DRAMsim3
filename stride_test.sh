#!/bin/bash

# Constants
ELEMENT_SIZE=32         # 32 bytes (256-bit)
COUNT=512               # number of elements
MAX_BYTES=$((4 * 1024 * 1024 * 1024))  # 4GB
IS_WRITE=0              # 0 for read, 1 for write

# Compute max stride_exp
max_exp=0
while true; do
    stride=$((1 << max_exp))
    last_index=$((COUNT - 1))
    total_bytes=$(((last_index + 1) * stride * ELEMENT_SIZE))
    if [ "$total_bytes" -gt "$MAX_BYTES" ]; then
        break
    fi
    ((max_exp++))
done
((max_exp--))  # step back to last valid

for ((exp = 0; exp <= max_exp; exp++)); do
    ./generate 0 "$exp" "$COUNT" "$IS_WRITE" > /dev/null
    sync
    cycles=$(./test | grep -oP '\d+(?= cycles)')
    printf "%-2d stride -> %s cycles\n" "$exp" "$cycles"
done