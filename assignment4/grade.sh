#!/bin/bash

# Ensure the executable exists
if [ ! -x "./graph" ]; then
    echo "Error: ./graph executable not found! Please run 'make' first."
    exit 1
fi

DATA_DIR="./data"

# Check if data directory exists
if [ ! -d "$DATA_DIR" ]; then
    echo "Error: Directory $DATA_DIR does not exist."
    exit 1
fi

for file in "$DATA_DIR"/*.data; do
    # Check if files actually exist (in case glob fails)
    if [ -f "$file" ]; then
        echo -e "\n\033[1;36m▶ Testing: $file\033[0m"
        # Extract N (number of nodes) from the first line of the file, taking only the first token
        N=$(head -n 1 "$file" | awk '{print $1}')
        
        # Calculate memory limit: 7000 + 6 * N^2 (in KB)
        LIMIT_KB=$(( 7000 + (6 * N * N) / 1024 ))
        
        # Limit virtual memory dynamically based on N
        ( 
            ulimit -v $LIMIT_KB
            echo -e "\033[1;33mMemory Budget: ${LIMIT_KB} KB\033[0m"
            timeout 10 ./graph "$file"
            EXIT_CODE=$?
            if [ $EXIT_CODE -eq 124 ]; then
                echo -e "\033[1;31m[Error] Time Limit Exceeded (10s)!\033[0m"
            elif [ $EXIT_CODE -ne 0 ]; then
                echo -e "\033[1;31m[Error] Runtime Error or Memory Limit Exceeded! (Exit Code: $EXIT_CODE, Limit: ${LIMIT_KB} KB)\033[0m"
            fi
        )
    fi
done