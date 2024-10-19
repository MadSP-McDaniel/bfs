#!/bin/bash

nc=$1
fsz=$2
iosz=$3
fs_type=$4
rlen=$5
nt=$6
op=$7

set -x
for client_idx in $(seq 0 $nc); do
    NUMIOS=$(($fsz / $iosz))
    MOUNTDIR=$(sed 's/\//\\\//g' <<</tmp/$fs_type/mp-c$client_idx)
    sed_pref=" 
                        sed -e 's/RUNLEN/$rl/' 
                        -e 's/MOUNTDIR/\$MOUNTDIR/' 
                        -e 's/FILESZ/$fsz/' 
                        -e 's/IOSZ/$iosz/' 
                        -e 's/NUMTHR/$nt/' 
                        -e 's/NUMIOS/$NUMIOS/' "
    eval "${sed_pref} $BFS_HOME/benchmarks/micro/micro_$op.f >$OUTDIR/current-exp$nc-c$client_idx.f"
    $BFS_HOME/benchmarks/client_refresh.sh $fs_type micro $BFS_USER $client_idx
done
