#!/bin/bash
#
# This script invokes all of the microbenchmarks and macrobenchmarks.
#

cip="192.168.1.224"

# Benchmarks currently support:
#   fs_types:
#       [nfs, nfs_ci, bfs, bfs_ci]
#   micro ops:
#       single-client: [seqread, rread, seqwrite, rwrite]
#       multi-client: [seqread_multi, rread_multi]
#   macro ops:
#       single/multi-client: [cp, git-clone, grep, tar-c, tar-x, make]

time (
    # 1 - remote storage system
    # storage="remote"

    ## single-client microbenchmarks
    # CFGS="bfs,bfs_ci," # comma delimited (with postpended comma)
    # NUMSAMPLES=1
    # RUNLEN=30
    # NUM_CLIENTS=1
    # OPS="seqread,rread,seqwrite,rwrite" # comma delimited (with no postpended comma)
    # $BFS_HOME/benchmarks/micro/run_micro.sh $NUMSAMPLES $RUNLEN $NUM_CLIENTS $OPS $CFGS $cip $storage $USER
    # $BFS_HOME/benchmarks/micro/parse_micro_results.py $NUM_CLIENTS $OPS $CFGS

    # ## multi-client microbenchmarks
    # CFGS="bfs,bfs_ci,"
    # NUMSAMPLES=1
    # RUNLEN=30
    # NUM_CLIENTS=5
    # OPS="seqread_multi,rread_multi"
    # $BFS_HOME/benchmarks/micro/run_micro.sh $NUMSAMPLES $RUNLEN $NUM_CLIENTS $OPS $CFGS $cip $storage $USER
    # $BFS_HOME/benchmarks/micro/parse_micro_results.py $NUM_CLIENTS $OPS $CFGS

    ## single-client macrobenchmarks
    # CFGS="bfs,bfs_ci,"
    # NUMSAMPLES=10
    # NUM_CLIENTS=1
    # OPS="cp,git-clone,grep,tar-c,tar-x"
    # $BFS_HOME/benchmarks/macro/run_macro.sh $NUMSAMPLES $NUM_CLIENTS $OPS $CFGS $cip $storage $USER
    # $BFS_HOME/benchmarks/macro/parse_macro_results.py $NUM_CLIENTS $CFGS

    ## multi-client macrobenchmarks
    # CFGS="bfs,bfs_ci,"
    # NUMSAMPLES=10
    # NUM_CLIENTS=5
    # OPS="cp,git-clone,grep,tar-c,tar-x"
    # $BFS_HOME/benchmarks/macro/run_macro.sh $NUMSAMPLES $NUM_CLIENTS $OPS $CFGS $cip $storage $USER
    # $BFS_HOME/benchmarks/macro/parse_macro_results.py $NUM_CLIENTS $CFGS

    # # save remote storage results
    # mv $BFS_HOME/benchmarks/micro/output $BFS_HOME/benchmarks/output--$storage

    # 2 - local storage system
    storage="local"
    BKEND="lwext4" # [bfs,lwext4]

    # single-client microbenchmarks
    CFGS="bfs,bfs_ci," # comma delimited (with postpended comma)
    NUMSAMPLES=10
    RUNLEN=30
    NUM_CLIENTS=1
    OPS="seqread,rread,seqwrite,rwrite" # comma delimited (with no postpended comma)
    $BFS_HOME/benchmarks/micro/run_micro.sh $NUMSAMPLES $RUNLEN $NUM_CLIENTS $OPS $CFGS $cip $storage $USER $BKEND
    $BFS_HOME/benchmarks/micro/parse_micro_results.py $NUM_CLIENTS $OPS $CFGS

    # multi-client microbenchmarks
    CFGS="bfs,bfs_ci,"
    NUMSAMPLES=10
    RUNLEN=30
    NUM_CLIENTS=5
    OPS="seqread_multi,rread_multi"
    $BFS_HOME/benchmarks/micro/run_micro.sh $NUMSAMPLES $RUNLEN $NUM_CLIENTS $OPS $CFGS $cip $storage $USER $BKEND
    $BFS_HOME/benchmarks/micro/parse_micro_results.py $NUM_CLIENTS $OPS $CFGS

    # single-client macrobenchmarks
    # CFGS="bfs,bfs_ci,"
    # NUMSAMPLES=10
    # NUM_CLIENTS=1
    # OPS="dd,git-clone,grep,tar-c,tar-x,make"
    # $BFS_HOME/benchmarks/macro/run_macro.sh $NUMSAMPLES $NUM_CLIENTS $OPS $CFGS $cip $storage $USER $BKEND
    # $BFS_HOME/benchmarks/macro/parse_macro_results.py $NUM_CLIENTS $CFGS

    # multi-client macrobenchmarks
    # CFGS="bfs,bfs_ci,"
    # NUMSAMPLES=10
    # NUM_CLIENTS=5
    # OPS="dd,git-clone,grep,tar-c,tar-x,make"
    # $BFS_HOME/benchmarks/macro/run_macro.sh $NUMSAMPLES $NUM_CLIENTS $OPS $CFGS $cip $storage $USER $BKEND
    # $BFS_HOME/benchmarks/macro/parse_macro_results.py $NUM_CLIENTS $CFGS

    # # save local storage results
    # mv $BFS_HOME/benchmarks/micro/output $BFS_HOME/benchmarks/output--$storage
)
