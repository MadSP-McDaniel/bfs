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
    for client_idx in $(seq 0 $_NUM_CLIENTS); do
        filebench -f $OUTDIR/current-exp$NUM_CLIENTS-c$client_idx.f &>$OUTDIR/current-exp$NUM_CLIENTS-c$client_idx.log &
    done
    wait
    for client_idx in $(seq 0 $_NUM_CLIENTS); do
        echo -n $(cat $OUTDIR/current-exp$NUM_CLIENTS-c$client_idx.log | grep -o ".*{op}OP.*mb/s" | grep -o "[0-9]*.[0-9]*mb/s" | cut -d m -f 1 | tail -1 | xargs), >>$OUTDIR/${_cfg[0]}-{op}-exp$NUM_CLIENTS-c$client_idx.csv
        echo -e "\n[Run params: ${_cfg[0]}-{op}-{iosz}-exp$NUM_CLIENTS]" >>$OUTDIR/${_cfg[0]}-{op}
        -exp$NUM_CLIENTS-c$client_idx.log
        cat $OUTDIR/current-exp$NUM_CLIENTS-c$client_idx.log >>$OUTDIR/${_cfg[0]}-{op}-exp$NUM_CLIENTS-c$client_idx.log
    done
done
