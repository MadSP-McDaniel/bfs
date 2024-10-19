#!/bin/bash

# This script assumes that the mountpoint is already set up and mounted.
# This just runs the filebench profile with different parameters.
# Make sure to warmup the caches by running a simple workload first.

set -e -x

mkdir -p o/filebench || true
workload="seqwrite"
mp="/mnt/bfs_ne"
fsz="$((2 ** 20))"
iosizes="131072"
num_threads="1"
num_ios=""

for iosize in $iosizes; do
    echo "Running experiment: [mp=$mp, workload=$workload, num_threads=$num_threads, iosize=$iosize]"

    num_ios=$((fsz / iosize * 1))

    for tid in $(seq 1 $num_threads); do
        # Make sure each thread (client) read/write different files
        # by appending the thread number to the mp.
        thrmp="$mp/$tid"
        cp micro.f micro.f.tmp
        sed -i "s/MOUNTDIR/$(echo $thrmp | sed 's/\//\\\//g')/g" micro.f.tmp
        sed -i "s/FILESZ/$fsz/g" micro.f.tmp
        sed -i "s/IOSZ/$iosize/g" micro.f.tmp
        sed -i "s/NUMTHR/$num_threads/g" micro.f.tmp
        sed -i "s/NUMIOS/$num_ios/g" micro.f.tmp
        filebench -f micro.f.tmp &>o/filebench/${workload}_${num_threads}_${iosize}_$(basename $mp)_$(basename $thrmp).log &
    done
    wait
done

echo "Done"
