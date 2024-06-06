#!/bin/bash
for file in "$@"
do
    while read line || [ -n "$line" ]
    do 
        ./jobCommander issueJob $line
    done < test_cases/"$file"
done
