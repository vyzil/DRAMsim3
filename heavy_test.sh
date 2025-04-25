#!/bin/bash

# Constants
ELEMENT_SIZE=32
MAX_BYTES=$((4 * 1024 * 1024 * 1024))  # 4GB
IS_WRITE=1

# Element counts: 1, 2, 4, ..., 16384
counts=()
for ((c = 0; c <= 14; c++)); do
    counts+=($((1 << c)))
done

# Maximum stride exponent
max_exp=18

# Header
printf "%12s" "stride\\cnt"
for c in "${counts[@]}"; do
    printf "%10d" "$c"
done
echo ""

# Measure
for ((exp = 0; exp <= max_exp; exp++)); do
    stride=$((1 << exp))
    printf "%12d" "$stride"

    for count in "${counts[@]}"; do
        # 메모리 초과 방지
        total_bytes=$((count * stride * ELEMENT_SIZE))
        if [ "$total_bytes" -gt "$MAX_BYTES" ]; then
            printf "%10s" "-"
            continue
        fi

        ./generate 0 "$exp" "$count" "$IS_WRITE" > /dev/null
        sync
        cycles=$(./test | grep -oP '\d+(?= cycles)' || echo "ERR")
        printf "%10s" "$cycles"
    done
    echo ""
done
