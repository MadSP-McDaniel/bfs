#!/bin/bash -x
set -e

# Kill any zombie processes
pkill bfs || true;
fusermount -u /tmp/bfs_test_mp || true;
sleep 1;

SAVE=$PWD;
cd $BFS_HOME/build/bin;

# Start the devices (only need to start/stop these for remote devices)
# pkill bfs_device || true;
# $BFS_HOME/build/bin/bfs_device -d 10 &
# $BFS_HOME/build/bin/bfs_device -d 20 &

# Start the server
$BFS_HOME/build/bin/bfs_server_ne &
sleep 5;

# Mount the client fs
$BFS_HOME/build/bin/bfs_client -f -s /tmp/bfs_test_mp &
sleep 5;

# Build the workload (make sure it builds OK) and run it (block until done)
g++ -Wall -g $SAVE/rw-latency.cpp -o $SAVE/rw-latency;
$SAVE/rw-latency;
sleep 2;

# Unmount the client fs
fusermount -u /tmp/bfs_test_mp;
sleep 2;

# Stop the server
pkill --signal 2 bfs_server_ne;
sleep 2;

# Stop the devices (only for remote devices)
# pkill --signal 2 bfs_device;
# sleep 2;

# Plot the data
$SAVE/parse_results.py;
sleep 2;

# Kill any remaining processes
pkill bfs  || true;
