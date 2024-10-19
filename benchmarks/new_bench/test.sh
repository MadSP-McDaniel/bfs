#!/bin/bash

# This script assumes that the mountpoint is already set up and mounted.
# This just runs the filebench profile with different parameters.
# Make sure to warmup the caches by running a simple workload first.

set -e -x

mkdir -p o/filebench || true
workload="rread"
mp="/mnt/bfs_ci"
fsz="$((2 ** 20))"
iosizes="131072 65536 32768 16384 8192 4096"
num_threads="1"
num_ios=""

for iosize in $iosizes; do
    echo "Running experiment: [mp=$mp, workload=$workload, num_threads=$num_threads, iosize=$iosize]"

    num_ios=$((fsz / iosize * 1))

    for tid in $(seq 1 $num_threads); do
        # Make sure each thread (client) read/write different files
        # by appending the thread number to the mp.
        thrmp="$mp/$tid"

        sudo umount $thrmp || true
        $BFS_HOME/build/bin/bfs_client -s -o allow_other $thrmp &>o/filebench/bench_${workload}_${num_threads}_${iosize}_$(basename $mp)_$(basename $thrmp).log &

        cp micro.f micro.f.$tid.tmp
        sed -i "s/MOUNTDIR/$(echo $thrmp | sed 's/\//\\\//g')/g" micro.f.$tid.tmp
        sed -i "s/FILESZ/$fsz/g" micro.f.$tid.tmp
        sed -i "s/IOSZ/$iosize/g" micro.f.$tid.tmp
        sed -i "s/NUMTHR/$num_threads/g" micro.f.$tid.tmp
        sed -i "s/NUMIOS/$num_ios/g" micro.f.$tid.tmp

        filebench -f micro.f.$tid.tmp &>o/filebench/${workload}_${num_threads}_${iosize}_$(basename $mp)_$(basename $thrmp).log &

        if [ $tid -eq 1 ]; then
            # sleep until o/filebench/bench_${workload}_${num_threads}_${iosize}_$(basename $mp)_$(basename $thrmp).log contains the string "Running..."
            while ! grep -q "Running..." o/filebench/bench_${workload}_${num_threads}_${iosize}_$(basename $mp)_$(basename $thrmp).log; do
                sleep 1
            done
        fi
    done
    wait
done

echo "Done"
