#!/usr/bin/env bash
#
# This is the benchmark script for the resilient block layer analysis.
#

### Intel VTune Amplifier

### Custom profiler

##### New benchmarks
$BFS_HOME/build/bin/bfs_core_test_ne -b
$BFS_HOME/benchmarks/micro/parse_mt.py
