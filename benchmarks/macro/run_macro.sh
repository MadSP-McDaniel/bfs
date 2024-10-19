#!/bin/bash
#
# This script invokes the linux utility workloads for
# macrobenchmarks, generates the results, then plots the data.
#

shopt -s expand_aliases
alias echo="echo -e [$(date)]"

NUMSAMPLES=
if [ -z "$1" ]; then
    echo "No number of samples given, defaulting to 1"
    NUMSAMPLES="1"
else
    NUMSAMPLES="$1"
fi

NUM_CLIENTS=
if [ -z "$2" ]; then
    echo "No num clients specified, defaulting to 1"
    NUM_CLIENTS="1"
else
    NUM_CLIENTS="$2"
fi

OPS=
if [ -z "$3" ]; then
    echo "No experiment ops specified, aborting"
    exit -1
else
    OPS="$3"
fi

_CFGS=
if [ -z "$4" ]; then
    echo "No cfgs specified, aborting"
    exit -1
else
    _CFGS="$4"
fi

CLIENT_IP=
if [ -z "$5" ]; then
    echo "No client ip address specified, aborting"
    exit -1
else
    CLIENT_IP="$5"
fi

# OUTDIR=
# if [ -z "$2" ]; then
#     echo "No output directory specified, defaulting to [$BFS_HOME/benchmarks/macro/output]"
#     OUTDIR="$BFS_HOME/benchmarks/macro/output"
# else
#     OUTDIR="$2"
# fi
OUTDIR="$BFS_HOME/benchmarks/macro/output"

STORAGE_TYPE=
if [ -z "$6" ]; then
    echo "No storage device type specified, defaulting to [local]"
    STORAGE_TYPE="local"
else
    STORAGE_TYPE="$6"
fi

BFS_USER=
if [ -z "$7" ]; then
    echo "No bfs user id specified, defaulting to [$USER]"
    BFS_USER="$USER"
else
    BFS_USER="$7"
fi

BKEND=
if [ -z "$8" ]; then
    echo "No backend specified, defaulting to [bfs]"
    BKEND="bfs"
else
    BKEND="$8"
fi

# Note: /tmp is not in-mem fs
declare -A CFGS
if [[ "$_CFGS" == *"nfs,"* ]]; then
    CFGS[CFG_NFS]="nfs"
fi
if [[ "$_CFGS" == *"nfs_ci,"* ]]; then
    CFGS[CFG_NFS_CI]="nfs_ci"
fi
if [[ "$_CFGS" == *"nfsg,"* ]]; then
    CFGS[CFG_NFSG]="nfsg"
fi
if [[ "$_CFGS" == *"nfsg_ci,"* ]]; then
    CFGS[CFG_NFSG_CI]="nfsg_ci"
fi
if [[ "$_CFGS" == *"bfs,"* ]]; then
    CFGS[CFG_BFS]="bfs"
fi
if [[ "$_CFGS" == *"bfs_ci,"* ]]; then
    CFGS[CFG_BFS_CI]="bfs_ci"
fi

echo "\n\tExperiment parameters: \n\
    \t\tNUMSAMPLES=$NUMSAMPLES \n\
    \t\tNUM_CLIENTS=$NUM_CLIENTS \n\
    \t\tOUTDIR=$OUTDIR \n\
    \t\tOPS= $OPS \n\
    \t\tCFGS= \n\
    $(printf "\t\t    %s\n" "${CFGS[@]}")"

echo "Starting [$NUMSAMPLES] macrobenchmark experiments:\n"

# Need to refresh client/server before we can run prepare scripts (they stage files on the the mount point)
set -x
set -e
DEBUG_OUTPUT=--show-output

# Starts all client threads and calculates the total latecny for all clients to finish at the latency to wait for all threads (equivalent realization if clients on diff machines)
# rm $OUTDIR/*.csv $OUTDIR/*.log $OUTDIR/*.pdf $OUTDIR/*.f &>/dev/null
mkdir -p $OUTDIR # try to make if it didnt already exist
# CLIENT_SEQ=$(xargs <<<$(seq 1 $NUM_CLIENTS) | sed -e 's/ /,/g') # just run NUM_CLIENTS

for cfg in "${CFGS[@]}"; do
    for client_idx in $(seq 1 $NUM_CLIENTS); do
        eval "printf 'cmd,' >$OUTDIR/$cfg-nc$NUM_CLIENTS-c$client_idx.csv"
        eval "printf 'sample%.0d,' {1..$NUMSAMPLES} >>$OUTDIR/$cfg-nc$NUM_CLIENTS-c$client_idx.csv"
        # eval "printf '\n' >>$OUTDIR/$cfg-nc$NUM_CLIENTS-c$client_idx.csv"
    done

    hyperfine --shell bash $DEBUG_OUTPUT --runs $NUMSAMPLES --export-csv $OUTDIR/$cfg.csv \
        -L cmd $OPS \
        --command-name "$cfg-{cmd}-nc$NUM_CLIENTS" \
        --setup " set -x ; \
            for client_idx in \`seq 1 $NUM_CLIENTS\`; do \
                eval \"printf '\n{cmd}', >>$OUTDIR/$cfg-nc$NUM_CLIENTS-c\$client_idx.csv\" ; \
            done " \
        --prepare " set -x ; \
            for client_idx in \`seq 1 $NUM_CLIENTS\`; do \
                $BFS_HOME/benchmarks/client_refresh.sh $cfg macro $BFS_USER 32768 \$client_idx $CLIENT_IP $STORAGE_TYPE $BKEND ; \
                $BFS_HOME/benchmarks/macro/macro_{cmd}.sh p $cfg \$client_idx ; \
            done " \
        "   echo \"Starting client linux utility workloads ...\" ; \
            for client_idx in \`seq 1 $NUM_CLIENTS\`; do \
                /usr/bin/time -f \"%e\" $BFS_HOME/benchmarks/macro/macro_{cmd}.sh c $cfg \$client_idx &>$OUTDIR/current-nc$NUM_CLIENTS-c\$client_idx.log & \
            done ; \
            echo \"Waiting for client linux utility workloads ...\" ; \
            wait; \
            
            echo \"Collecting macrobenchmark results ...\" ; \
            for client_idx in \`seq 1 $NUM_CLIENTS\`; do \
                echo -n \$(cat $OUTDIR/current-nc$NUM_CLIENTS-c\$client_idx.log | tail -1 | xargs), >>$OUTDIR/$cfg-nc$NUM_CLIENTS-c\$client_idx.csv ; \
                echo -e \"\\n[Run params: $cfg-{cmd}-nc$NUM_CLIENTS]\" >>$OUTDIR/$cfg-nc$NUM_CLIENTS-c\$client_idx.log ; \
                cat $OUTDIR/current-nc$NUM_CLIENTS-c\$client_idx.log >>$OUTDIR/$cfg-nc$NUM_CLIENTS-c\$client_idx.log ; \
            done"
    # "$BFS_HOME/benchmarks/macro/macro_{cmd}.sh c /tmp/$cfg/mp-c0"
done

echo "Done"
