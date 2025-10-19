#!/bin/bash

CONFIG_FILE="config.txt"
MAIN_CLASS="./launcher"
PROJECT_DIRECTORY="."

num_nodes=$(awk 'NR==1 {print $1}' "$CONFIG_FILE")

for ((i=0; i<num_nodes; i++)); do
  echo "Launching Node $i"

  cmd.exe /c start cmd.exe /k "cd /d $PROJECT_DIRECTORY && $MAIN_CLASS $CONFIG_FILE"
done