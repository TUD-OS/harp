cat /proc/cpuinfo | grep -E 'processor|physical id|core id' | tr '\n' ' ' | sed 's/processor/\nprocessor/g'

