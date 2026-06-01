#!/bin/bash

echo "=== Assignment 3: Online Leaderboard Grading ==="
echo ""

make -s
if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

shopt -s nullglob
DATASETS=(data/*.data)
TOTAL_OVERALL=0
ALL_CORRECT=true

for data in "${DATASETS[@]}"; do
    echo "-------------------------------------------"
    echo "Running on $data..."
    
    OUTPUT=$(./leaderboard "$data")
    if [ $? -ne 0 ]; then
        echo "FAILED on $data"
        ALL_CORRECT=false
        break
    fi
    
    COST=$(echo "$OUTPUT" | grep -o "Total Access Count: [0-9]*" | awk '{print $4}')
    
    if [ -z "$COST" ]; then
        echo "Success at $data (Cost not found)"
    else
        echo "Success at $(basename $data) | Access Count: $COST"
        TOTAL_OVERALL=$((TOTAL_OVERALL + COST))
    fi
    echo ""
done

if [ "$ALL_CORRECT" = true ]; then
    echo "==========================================="
    echo "All test cases passed!"
    echo "Overall Access Count (Sum of all datasets): $TOTAL_OVERALL"
    echo "==========================================="
fi
