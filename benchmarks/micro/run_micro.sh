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

OPS=
if [ -z "$4" ]; then
    echo "No experiment ops specified, aborting"
    exit -1
else
    OPS="$4"
fi

_CFGS=
if [ -z "$5" ]; then
    echo "No cfgs specified, aborting"
    exit -1
else
    _CFGS="$5"
fi

CLIENT_IP=
if [ -z "$6" ]; then
    echo "No client ip address specified, aborting"
    exit -1
else
    CLIENT_IP="$6"
fi

# OUTDIR=
# if [ -z "$5" ]; then
#     echo "No output directory specified, defaulting to [$BFS_HOME/benchmarks/micro/output]"
#     OUTDIR="$BFS_HOME/benchmarks/micro/output"
# else
#     OUTDIR="$5"
# fi
OUTDIR="$BFS_HOME/benchmarks/micro/output"

STORAGE_TYPE=
if [ -z "$7" ]; then
    echo "No storage device type specified, defaulting to [local]"
    STORAGE_TYPE="local"
else
    STORAGE_TYPE="$7"
fi

BFS_USER=
if [ -z "$8" ]; then
    echo "No bfs user id specified, defaulting to [$USER]"
    BFS_USER="$USER"
else
    BFS_USER="$8"
fi

BKEND=
if [ -z "$9" ]; then
    echo "No backend specified, defaulting to [bfs]"
    BKEND="bfs"
else
    BKEND="$9"
fi

NUMTHR=(
    "1"
)

# Note: /tmp is not in-mem fs
# _io_sizes=("4096" "8192")
_io_sizes=("4096" "8192" "16384" "32768" "65536" "131072")
io_sizes=$(xargs <<<${_io_sizes[@]} | sed -e 's/ /,/g')
declare -A CFGS
if [[ "$_CFGS" == *"nfs,"* ]]; then
    CFGS[CFG_NFS]=" \
        nfs \
        1048576 \
        ${io_sizes}"
fi
if [[ "$_CFGS" == *"nfs_ci,"* ]]; then
    CFGS[CFG_NFS_CI]=" \
        nfs_ci \
        1048576 \
        ${io_sizes}"
fi
if [[ "$_CFGS" == *"nfsg,"* ]]; then
    CFGS[CFG_NFSG]=" \
        nfsg \
        1048576 \
        ${io_sizes}"
fi
if [[ "$_CFGS" == *"nfsg_ci,"* ]]; then
    CFGS[CFG_NFSG_CI]=" \
        nfsg_ci \
        1048576 \
        ${io_sizes}"
fi
if [[ "$_CFGS" == *"bfs,"* ]]; then
    CFGS[CFG_BFS]=" \
        bfs \
        1048576 \
        ${io_sizes}"
fi
if [[ "$_CFGS" == *"bfs_ci,"* ]]; then
    CFGS[CFG_BFS_CI]=" \
        bfs_ci \
        1048576 \
        ${io_sizes}"
fi

echo "\n\tExperiment parameters: \n\
    \t\tNUMSAMPLES=$NUMSAMPLES \n\
    \t\tRUNLEN=$RUNLEN \n\
    \t\tNUM_CLIENTS=$NUM_CLIENTS \n\
    \t\tNUMTHR=$NUMTHR \n\
    \t\tOPS=$OPS \n\
    \t\tOUTDIR=$OUTDIR \n\
    \t\tCFGS= \n\
    $(printf "\t\t%s\n" "${CFGS[@]}")"

# Note: Use ' --show-output' and 'set -x' to get more debugging info
# Note: The hyperfine csv is unused for microbenchmarks (filebench logs parsed instead)
set -x
set -e
DEBUG_OUTPUT=--show-output

# Edit: dont need; eventually need better way to clean only single/multi experiment results
# rm $OUTDIR/*.csv $OUTDIR/*.log $OUTDIR/*.pdf $OUTDIR/*.f &>/dev/null || true
mkdir -p $OUTDIR &>/dev/null # try to make if it didnt already exist
CLIENT_SEQ=$(xargs <<<$(seq 1 $NUM_CLIENTS) | sed -e 's/ /,/g')
for cfg in "${CFGS[@]}"; do
    read -a _cfg <<<$(xargs <<<$cfg)

    read -a _ops <<<$(xargs -d "," <<<$OPS)
    read -a _iosizes <<<$(xargs -d "," <<<${_cfg[2]})

    for _client_seq in $(seq 1 $NUM_CLIENTS); do
        for client_idx in $(seq 1 $_client_seq); do
            for _op in "${_ops[@]}"; do
                printf "sample," >$OUTDIR/${_cfg[0]}-$_op-nc$NUM_CLIENTS-ncseq$_client_seq-c$client_idx.csv
                printf "%s," "${_iosizes[@]}" >>$OUTDIR/${_cfg[0]}-$_op-nc$NUM_CLIENTS-ncseq$_client_seq-c$client_idx.csv
            done
        done
    done

    echo "Starting microbenchmark experiments for [${_cfg[0]}]"

    for sample in $(seq 1 $NUMSAMPLES); do
        for _client_seq in $(seq 1 $NUM_CLIENTS); do
            for client_idx in $(seq 1 $_client_seq); do
                for _op in "${_ops[@]}"; do
                    printf "\n$sample," >>$OUTDIR/${_cfg[0]}-$_op-nc$NUM_CLIENTS-ncseq$_client_seq-c$client_idx.csv
                done
            done
        done

        hyperfine --shell bash $DEBUG_OUTPUT --runs 1 --export-csv $OUTDIR/micro_overall_${_cfg[0]}-nc$NUM_CLIENTS-sample$sample.csv \
            -L rl ${RUNLEN[0]} \
            -L nt ${NUMTHR[0]} \
            -L op $OPS \
            -L ncseq $CLIENT_SEQ \
            -L iosz ${_cfg[2]} \
            --command-name "${_cfg[0]}-{op}-{iosz}-nc$NUM_CLIENTS-ncseq{ncseq}-sample$sample" \
            --prepare " set -x ; \
                for client_idx in \`seq 1 {ncseq}\`; do \
                    NUMIOS=\$((${_cfg[1]} / {iosz})) ; \
                    MOUNTDIR=\$(sed 's/\//\\\\\//g' <<</tmp/${_cfg[0]}/mp-c\$client_idx) ; \
                    sed_pref=\" \
                        sed -e 's/RUNLEN/{rl}/' \
                        -e 's/MOUNTDIR/\$MOUNTDIR/' \
                        -e 's/FILESZ/${_cfg[1]}/' \
                        -e 's/IOSZ/{iosz}/' \
                        -e 's/NUMTHR/{nt}/' \
                        -e 's/NUMIOS/\$NUMIOS/' \" ; \
                    eval \"\${sed_pref} $BFS_HOME/benchmarks/micro/micro_{op}.f >$OUTDIR/current-nc$NUM_CLIENTS-ncseq{ncseq}-c\$client_idx.f\" ; \
                    $BFS_HOME/benchmarks/client_refresh.sh ${_cfg[0]} micro $BFS_USER {iosz} \$client_idx $CLIENT_IP $STORAGE_TYPE $BKEND ; \
                done " \
            "   set -x ; \
                echo \"Starting client filebench workloads ...\" ; \
                for client_idx in \`seq 1 {ncseq}\`; do \
                    filebench -f $OUTDIR/current-nc$NUM_CLIENTS-ncseq{ncseq}-c\$client_idx.f &>$OUTDIR/current-nc$NUM_CLIENTS-ncseq{ncseq}-c\$client_idx.log & \
                    if [ \"\$client_idx\" -eq 1 ]; then \
                        echo \"Waiting for first client to finish prealloc ...\" ; \
                        sleep 5; \
                    fi ; \
                done ; \
                echo \"Waiting for client filebench workloads ...\" ; \
                wait; \
                echo \"Collecting filebench workload results ...\" ; \
                for client_idx in \`seq 1 {ncseq}\`; do \
                    echo -n \$(cat $OUTDIR/current-nc$NUM_CLIENTS-ncseq{ncseq}-c\$client_idx.log | grep -o \".*{op}OP.*mb/s\" \
                        | grep -o \"[0-9]*.[0-9]*mb/s\" | cut -d m -f 1 | tail -1 | xargs), >>$OUTDIR/${_cfg[0]}-{op}-nc$NUM_CLIENTS-ncseq{ncseq}-c\$client_idx.csv ; \
                    echo -e \"\\n[Run params: ${_cfg[0]}-{op}-{iosz}-nc$NUM_CLIENTS-ncseq{ncseq}]\" >>$OUTDIR/${_cfg[0]}-{op}-nc$NUM_CLIENTS-ncseq{ncseq}-c\$client_idx.log ; \
                    cat $OUTDIR/current-nc$NUM_CLIENTS-ncseq{ncseq}-c\$client_idx.log >>$OUTDIR/${_cfg[0]}-{op}-nc$NUM_CLIENTS-ncseq{ncseq}-c\$client_idx.log ; \
                done"

        # --command-name "${_cfg[0]}-{op}-{iosz}-nc$NUM_CLIENTS-ncseq{ncseq}-s$sample" \
        # --prepare "$BFS_HOME/benchmarks/micro/prep.sh $NUM_CLIENTS ${_cfg[1]} {iosz} ${_cfg[0]} {rl} {nt} {op}" \
        # "$BFS_HOME/benchmarks/micro/cmd.sh "
    done
done

echo "Done"
