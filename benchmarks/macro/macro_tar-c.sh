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
num_copies=50

if [ "$1" = "p" ]; then
    echo "Running setup for benchmark"
    # copy over some directory to archive and clean any old archive
    rm -rf ${_mount_dir}/linux-sgx-driver-c$3 &>/dev/null || true
    cp -r $BFS_HOME/benchmarks/macro/linux-sgx-driver $_mount_dir/linux-sgx-driver-c$3

    for copy_idx in $(seq 1 $num_copies); do
        rm ${_mount_dir}/macro_tar-c_c$3-copy$copy_idx.tgz || true
    done
else
    # archive the directory
    for copy_idx in $(seq 1 $num_copies); do
        tar czf ${_mount_dir}/macro_tar-c_c$3-copy$copy_idx.tgz $_mount_dir/linux-sgx-driver-c$3
    done
fi
