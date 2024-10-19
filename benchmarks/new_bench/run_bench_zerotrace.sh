#!/bin/bash

set -e -x

iosizes="131072 65536 16384 4096"
outdir=$BFS_HOME/benchmarks/new_bench
# Run ZeroTrace experiments
# rrs="0 100"
# for iosize in $iosizes; do
#     for rr in $rrs; do
#         ./exec_zt.sh 1048576 $iosize 1000 $rr &>$outdir/o/zerotrace/zt_${iosize}_${rr}.log
#     done
# done

# Run BFS experiments
for iosize in $iosizes; do
    # collects both reads and writes
    $BFS_HOME/build/bin/bfs_core_test_ne -c -r -f 1048576 -o $iosize -n 1000 &>$outdir/o/zerotrace/bfs_ci_${iosize}.log
done
