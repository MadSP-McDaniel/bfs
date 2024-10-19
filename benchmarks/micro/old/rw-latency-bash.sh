#!/bin/bash

NUM_FILES=1
ITERS=1000

# Preallocate the files (so they get allocated to different blocks)
for i in `seq 1 $NUM_FILES`; do
    touch /tmp/bfs_test_mp/$i.txt 2> /dev/null;
done

# Execute a 4K write for ITERS iterations on each file
for i in `seq 1 $NUM_FILES`; do
    for j in `seq 1 $ITERS`; do
        printf 'w%.0s' 1..4000 > /tmp/bfs_test_mp/$i.txt;
    done
done

# Execute a 4K read for ITERS iterations on each file
for i in `seq 1 $NUM_FILES`; do
    for j in `seq 1 $ITERS`; do
        cat /tmp/bfs_test_mp/$i.txt > /dev/null;
    done
done

# Unmount the fs
fusermount -u /tmp/bfs_test_mp;

# Plot the data
./parse_results.py;
