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
    # copy over some archive to extract
    rm -rf ${_mount_dir}/c$3/macro_tar-c_c$3.tgz &>/dev/null || true
    mkdir -p $_mount_dir/c$3 # make separate directory for each client to extract independently
    cp $BFS_HOME/benchmarks/macro/macro_tar-c.tgz $_mount_dir/c$3/macro_tar-c_c$3.tgz &>/dev/null
else
    # extract the directory (in subshell)
    num_copies=100
    for copy_idx in $(seq 1 $num_copies); do
        cd $_mount_dir/c$3
        # by default (without --overwrite flag), tar will delete+create new directory when extracting
        tar xzf macro_tar-c_c$3.tgz &>/dev/null || true
    done
fi
