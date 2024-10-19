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

num_copies=10

if [ "$1" = "p" ]; then
    echo "Running setup for benchmark"
    # create new file
    # dd if=/dev/zero of=${_mount_dir}/1.txt bs=1024 count=1000 &>/dev/null

    # for copy_idx in $(seq 1 $num_copies); do
    #     rm ${_mount_dir}/$copy_idx-c$3.txt &>/dev/null || true
    # done
    # printf 'x%.0s' {1..1039872} >>${_mount_dir}/1-c$3.txt # 1M r/w requires about 2 blocks for either fs

    # flush stuff
    sync
    echo 3 | sudo tee /proc/sys/vm/drop_caches
    sleep 5
else
    # make a copies of the file
    # cd $2
    # for copy_idx in $(seq 2 $num_copies); do
    #     cp ${_mount_dir}/1-c$3.txt ${_mount_dir}/$copy_idx-c$3.txt &>/dev/null
    # done

    # match default bs for nfs in macrobenchmarks so bfs and nfs use it
    # copy a 128M file
    for copy_idx in $(seq 2 $num_copies); do
        dd if=/dev/zero of=${_mount_dir}/0.txt bs=32768 count=4096 &>/dev/null
    done
fi
