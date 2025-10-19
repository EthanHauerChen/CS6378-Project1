#!/bin/bash

CONFIG_FILE=$1 # argv[1] equivalent
MAIN_EXECUTABLE="main"
PROJECT_DIRECTORY="~/6378/Project1"

# --- SAFETY CHECKS ---
if [[ -z "$CONFIG_FILE" || ! -f "$CONFIG_FILE" ]]; then
    echo "Usage: $0 <config_file>"
    exit 1
fi

# --- PARSE CONFIGURATION FILE ---

# Read the first valid line (starts with an unsigned integer, ignores # comments)
first_valid_line=$(grep -E '^[0-9]+' "$CONFIG_FILE" | sed 's/#.*//' | head -n 1)

# Extract the first integer (number of nodes)
n=$(echo "$first_valid_line" | awk '{print $1}')

# Extract next n valid lines after the first one
#    (still ignoring comments and blank lines)
mapfile -t lines < <(grep -E '^[0-9]+' "$CONFIG_FILE" | sed 's/#.*//' | tail -n +2 | head -n "$n")

# --- SSH INTO MACHINES ---
for line in "${lines[@]}"; do
    hostname=$(echo "$line" | awk '{print $2}')
    if [[ -n "$hostname" ]]; then
        echo "Connecting to $hostname..."
        ssh "ehc180001@${hostname}.utdallas.edu" "${PROJECT_DIRECTORY}/${MAIN_EXECUTABLE} ${PROJECT_DIRECTORY}/${CONFIG_FILE}" &
    fi
done

# Wait for all SSH sessions to finish
wait
echo "All remote commands completed."

