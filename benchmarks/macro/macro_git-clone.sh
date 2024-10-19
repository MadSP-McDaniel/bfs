#!/bin/bash

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
    # delete any remaining contents (should call refresh here to get completely new mount)
    rm -rf ${_mount_dir}/linux-sgx-driver-c$3 &>/dev/null || true
else
    # clone a repo
    git clone https://github.com/intel/linux-sgx-driver ${_mount_dir}/linux-sgx-driver-c$3
fi
