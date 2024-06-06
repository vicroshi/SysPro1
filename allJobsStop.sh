#!/bin/bash
if [ ! -f ./jobExecutorServer.txt ]; then 
    ./jobCommander setConcurrency 1 # this is needed to start the server if its not already running because the stdout pipe will get stuck
fi

while read job 
do if [[ $job == *"<"* ]]; then
        job=${job#"<"}  # remove leading '<'
        job=${job%%,*}  # remove everything after and including ','
        ./jobCommander stop "$job"
    fi
done < <(./jobCommander poll queued; ./jobCommander poll running)