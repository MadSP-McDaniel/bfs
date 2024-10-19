#!/bin/bash

if [ -z "$1" ]; then
    echo "No benchmark param given"
    exit -1
fi

if [ -z "$2" ]; then
    echo "No benchmark mount point given"
fi

if [ "$1" = "p" ]; then
    echo "Running setup for benchmark"
    # none
    exit 0
else
    # none
    exit 0
fi
