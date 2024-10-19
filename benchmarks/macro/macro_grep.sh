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
    # copy over some directory to grep through
    rm -rf ${_mount_dir}/linux-sgx-driver-c$3 &>/dev/null || true
    cp -r $BFS_HOME/benchmarks/macro/linux-sgx-driver $_mount_dir/linux-sgx-driver-c$3
else
    # recursively grep through it for the string "sgx"
    num_copies=50
    for copy_idx in $(seq 1 $num_copies); do
        grep -R "sgx" ${_mount_dir}/linux-sgx-driver-c$3 &>/dev/null
    done
fi
