#!/bin/bash
#
# Refreshes bfs device processes.
#

set -x
fs_type=$1
bench_type=$2
did=$3

if [ "${fs_type}" = "bfs" ]; then
    echo -e "\nSetting up new ${fs_type} device ..."

    # kill running device processes under this did
    pkill -SIGKILL -f "bfs_device -d $did"
    sleep 5

    mkdir -p $BFS_HOME/benchmarks/$bench_type/output &>/dev/null # for device logs
    $BFS_HOME/build/bin/bfs_device -d $did &>$BFS_HOME/benchmarks/$bench_type/output/${fs_type}_d${did}_current.log &
    sleep 2
elif [ "${fs_type}" = "bfs_ci" ]; then
    echo -e "\nSetting up new ${fs_type} device ..."

    # kill running device processes under this did
    pkill -SIGKILL -f "bfs_device -d $did"
    sleep 5

    mkdir -p $BFS_HOME/benchmarks/$bench_type/output &>/dev/null # for device logs
    $BFS_HOME/build/bin/bfs_device -d $did &>$BFS_HOME/benchmarks/$bench_type/output/${fs_type}_d${did}_current.log &
    sleep 2
elif [ "${fs_type}" = "nfs" ]; then
    : # do nothing for now in nfs mode
elif [ "${fs_type}" = "nfsg" ]; then
    : # do nothing for now in nfsg mode
elif [ "${fs_type}" = "nfs_ci" ]; then
    : # do nothing for now in nfs_ci mode
elif [ "${fs_type}" = "nfsg_ci" ]; then
    : # do nothing for now in nfsg_ci mode
else
    echo "Unknown fs_type: ${fs_type}"
    exit -1
fi
