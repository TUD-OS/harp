#!/bin/bash

# Check if a configuration file was provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <config_file>"
    exit 1
fi

config_file="$1"

run_cpu_load() {
    local cpus=($1)  # Get CPU cores configuration as an array
    local depth=50   # Adjust the Fibonacci number depth as needed
    echo "Starting CPU load on CPUs: ${cpus[*]}"

    for cpu in "${cpus[@]}"; do
        taskset -c $cpu ./fibonacci $depth 2>/dev/null &
        pid_list+=($!)
    done
}

# Read configurations from file and run experiments
experiment_number=1

while IFS= read -r line; do
    echo "Measuring power usage for configuration $experiment_number: $line"
    
    # Start cpu-energy-meter
    cpu-energy-meter -r &
    meter_pid=$!

    # Start CPU load
    run_cpu_load "$line"

    # Wait 5 seconds
    sleep 5

    # Stop all CPU load processes
    kill -9 ${pid_list[@]} 1>/dev/null 2>&1
    unset pid_list

    # Stop cpu-energy-meter and log the energy data
    kill -INT $meter_pid 2>/dev/null
    wait $meter_pid

    echo "Measurement for configuration $experiment_number completed."
    echo "================"
    ((experiment_number++))
done < "$config_file"

