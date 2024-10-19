#!/bin/bash

set -x
if [ -z "$1" ]; then
    echo "No benchmark param given"
    exit -1
fi

if [ -z "$2" ]; then
    echo "No cfg given"
    exit -1
fi

if [ -z "$3" ]; then
    echo "No client index given"
    exit -1
fi

_mount_dir=/tmp/$2/mp-c$3

if [ "$1" = "p" ]; then
    echo "Running setup for benchmark"
    # copy over some code directory to compile (and clean any build artifacts)
    rm -rf ${_mount_dir}/bzip2-c$3 &>/dev/null || true
    cp -r $BFS_HOME/benchmarks/macro/bzip2 $_mount_dir/bzip2-c$3 &>/dev/null
    make -C ${_mount_dir}/bzip2-c$3 clean &>/dev/null
else
    # compile the program
    make -C ${_mount_dir}/bzip2-c$3 all &>/dev/null
fi
