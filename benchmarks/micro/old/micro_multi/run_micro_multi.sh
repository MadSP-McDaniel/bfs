#!/bin/bash
#
# This script invokes the filebench workloads for microbenchmarks,
# generates the results, then plots the data.
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

RUNLEN=
if [ -z "$2" ]; then
    echo "No run length specified, defaulting to 1"
    RUNLEN="1"
else
    RUNLEN="$2"
fi

NUM_CLIENTS=
if [ -z "$3" ]; then
    echo "No num clients specified, defaulting to 1"
    NUM_CLIENTS="1"
else
    NUM_CLIENTS="$3"
fi

OUTDIR=
if [ -z "$4" ]; then
    echo "No output directory specified, defaulting to [$BFS_HOME/benchmarks/micro_multi/output]"
    OUTDIR="$BFS_HOME/benchmarks/micro_multi/output"
else
    OUTDIR="$4"
fi

BFS_USER=
if [ -z "$5" ]; then
    echo "No bfs user id specified, defaulting to [$USER]"
    BFS_USER="$USER"
else
    BFS_USER="$5"
fi

NUMTHR=(
    "1"
)

OPS="seqread_multi_client,rread_multi_client"

# Note: /tmp is not in-mem fs
declare -A CFGS
CFGS[CFG_BFS]=" \
    bfs \
    1039872 \
    4062,8124,16248,32496,64992,129984"
CFGS[CFG_NFS]=" \
    nfs \
    1048576 \
    4096,8192,16384,32768,65536,131072"

echo "\n\tExperiment parameters: \n\
    \t\tNUMSAMPLES=$NUMSAMPLES \n\
    \t\tOUTDIR=$OUTDIR \n\
    \t\tRUNLEN=$RUNLEN \n\
    \t\tNUMTHR=$NUMTHR \n\
    \t\tOPS=$OPS \n\
    \t\tCFGS= \n\
    $(printf "\t\t%s\n" "${CFGS[@]}")"

# Note: Use ' --show-output' and 'set -x' to get more debugging info
# Note: The hyperfine csv is unused for microbenchmarks (filebench logs parsed instead)
# Client workload start time will be offset slightly so the workload duration should be sufficiently long enough
set -x
rm $OUTDIR/*.csv $OUTDIR/*.log $OUTDIR/*.pdf $OUTDIR/*.f &>/dev/null
mkdir -p $OUTDIR &>/dev/null # try to make if it didnt already exist

for cfg in "${CFGS[@]}"; do
    read -a _cfg <<<$(xargs <<<$cfg)
    read -a _ops <<<$(xargs -d "," <<<$OPS)
    read -a _iosizes <<<$(xargs -d "," <<<${_cfg[2]})

    for client_idx in $(seq 1 $NUM_CLIENTS); do
        for _op in "${_ops[@]}"; do
            printf "sample," >$OUTDIR/${_cfg[0]}-$_op-c$client_idx.csv
            printf "%s," "${_iosizes[@]}" >>$OUTDIR/${_cfg[0]}-$_op-c$client_idx.csv
        done
    done

    echo "Starting [$NUMSAMPLES] microbenchmark (multi) experiments for [${_cfg[0]}]"

    for sample in $(seq 0 $(($NUMSAMPLES - 1))); do
        for client_idx in $(seq 1 $NUM_CLIENTS); do
            mkdir -p /tmp/${_cfg[0]}/mp-c$client_idx &>/dev/null # make sure mount point exists for client

            for _op in "${_ops[@]}"; do
                printf "\n$sample," >>$OUTDIR/${_cfg[0]}-$_op-c$client_idx.csv
            done

            hyperfine --shell bash --show-output --runs 1 --export-csv $OUTDIR/micro_multi_overall_${_cfg[0]}-c$client_idx-$sample.csv \
                -L rl ${RUNLEN[0]} \
                -L nt ${NUMTHR[0]} \
                -L op $OPS \
                -L iosz ${_cfg[2]} \
                --command-name "${_cfg[0]}-{op}-{iosz}-$sample" \
                --prepare " \
                set -x ; \
                NUMIOS=\$((${_cfg[1]} / {iosz})) ; \
                MOUNTDIR=\$(sed 's/\//\\\\\//g' <<</tmp/${_cfg[0]}/mp-c$client_idx) ; \
                sed_pref=\" \
                    sed -e 's/RUNLEN/{rl}/' \
                    -e 's/MOUNTDIR/\$MOUNTDIR/' \
                    -e 's/FILESZ/${_cfg[1]}/' \
                    -e 's/IOSZ/{iosz}/' \
                    -e 's/NUMTHR/{nt}/' \
                    -e 's/NUMIOS/\$NUMIOS/' \" ; \
                eval \"\${sed_pref} $BFS_HOME/benchmarks/micro_multi/micro_{op}.f >$OUTDIR/c$client_idx-current.f\" ; \
                $BFS_HOME/benchmarks/client_refresh.sh ${_cfg[0]} micro_multi $BFS_USER $client_idx" \
                "filebench -f $OUTDIR/c$client_idx-current.f &>$OUTDIR/c$client_idx-current.log ; wait ; \
                echo -n \$(cat $OUTDIR/c$client_idx-current.log | grep -o \".*{op}OP.*mb/s\" \
                    | grep -o \"[0-9]*.[0-9]*mb/s\" | cut -d m -f 1 | tail -1 | xargs), >>$OUTDIR/${_cfg[0]}-{op}-c$client_idx.csv ; \
                echo -e \"\\n[Run params: ${_cfg[0]}-{op}-{iosz}]\" >>$OUTDIR/${_cfg[0]}-{op}-c$client_idx.log ; \
                cat $OUTDIR/c$client_idx-current.log >>$OUTDIR/${_cfg[0]}-{op}-c$client_idx.log "
        done

        wait # wait for recently started experiments to finish first
    done
done

$BFS_HOME/benchmarks/micro_multi/parse_micro_multi_results.py
echo "Done"
