#!/usr/bin/env bash
# Get raw FS perf for BFS and Gramine.

# set -x
set -e

h="Usage: ./run_bench_raw.sh <mt flag> <cache size> <clean flag> <bfs flag> <gramine flag>"

export outd=$BFS_HOME/benchmarks/micro/output
if [[ -z "$1" ]]; then
    echo "No merkle tree type specified, aborting"
    echo $h
    exit 1
fi
MT=$1

if [[ -z "$2" ]]; then
    echo "No cache size specified, aborting"
    echo $h
    exit 1
fi
CACHE=$2

if [[ -z "$3" ]]; then
    echo "No clean flag specified, aborting"
    echo $h
    exit 1
fi
C=$3
if [[ "$C" -eq 1 ]]; then
    echo -e "Cleaning old experiment csv ...\n"
    rm $outd/raw_bfs.csv
    echo config,swrite,sread,rwrite,rread >>$outd/raw_bfs.csv
    rm $outd/raw_gramine.csv
    echo config,swrite,sread,rwrite,rread >>$outd/raw_gramine.csv
fi

B=0
if [[ "$4" ]]; then
    B=$4
fi

G=0
if [[ "$5" ]]; then
    G=$5
fi

# bench params
num_samples=1000
fsz=1048576
op_szs=(4096 16384 65536 131072)
echo -e "#\n# Config:\n#\tmt: $MT, c: $CACHE, num_samples: $num_samples, fsz: $fsz, op_szs: ${op_szs[@]}\n#\n"

# dump BFS R/W results to csv (need to make sure bfsFsLayer log_enabled is true so we can grep output)
if [[ "$B" -eq 1 ]]; then
    echo "Running BFS ..."
    for i in "${op_szs[@]}"; do
        _mt=$MT _c=$CACHE bash -c "\
            echo -n mt\$_mt-c\$_c-o$i, >>$outd/raw_bfs.csv && \
            echo SEQ, >$outd/raw_bfs.log && \
            $BFS_HOME/build/bin/bfs_core_test_ne -c -n $num_samples -f $fsz -o $i >>$outd/raw_bfs.log && \
            cat $outd/raw_bfs.log | grep \"Write throughput\" | cut -d \")\" -f2 | xargs | cut -d \" \" -f1 | xargs | tr -d '\\n' >>$outd/raw_bfs.csv && \
            echo -n , >>$outd/raw_bfs.csv && \
            cat $outd/raw_bfs.log | grep \"Read throughput\" | cut -d \")\" -f2 | xargs | cut -d \" \" -f1 | xargs | tr -d '\\n' >>$outd/raw_bfs.csv && \
            echo -n , >>$outd/raw_bfs.csv && \
            echo RAND, >$outd/raw_bfs.log && \
            $BFS_HOME/build/bin/bfs_core_test_ne -c -r -n $num_samples -f $fsz -o $i >>$outd/raw_bfs.log && \
            cat $outd/raw_bfs.log | grep \"Write throughput\" | cut -d \")\" -f2 | xargs | cut -d \" \" -f1 | xargs | tr -d '\\n' >>$outd/raw_bfs.csv && \
            echo -n , >>$outd/raw_bfs.csv && \
            cat $outd/raw_bfs.log | grep \"Read throughput\" | cut -d \")\" -f2 | xargs | cut -d \" \" -f1 | xargs >>$outd/raw_bfs.csv"
    done
fi

# dump Gramine R/W results to csv
####################################
# Note: make sure to copy the new rw-latency.cpp in this repo over to the Gramine repo scripts dir, rebuild it, then copy it over to the Gramine repo test dir (encrypted mount point), so that when we run it, it will run from the context of the TEE:
#   cd scripts ; cp ~/repos/bfs/benchmarks/micro/rw-latency.cpp . ;
#   g++ -Wall -g rw-latency.cpp -o rwlat ;
#   .. ;
#   ll scripts/rw* ;
#   gramine-sgx ./bash -c "cp scripts/rwlat test" ;
#   ll test/rw* ;
#   gramine-sgx ./bash -c "test/rwlat" ;
####################################
if [[ "$G" -eq 1 ]]; then
    echo "Running Gramine ..."
    for i in "${op_szs[@]}"; do
        _mt=$MT _c=$CACHE bash -c "\
            echo -n mt\$_mt-c\$_c-o$i, >>$outd/raw_gramine.csv && \
            echo SEQ, >$outd/raw_gramine.log && \
            pushd /home/$USER/repos/gramine/CI-Examples/bash && \
            gramine-sgx ./bash -c \"test/rwlat s $num_samples $fsz $i\" >>$outd/raw_gramine.log && \
            popd && \
            cat $outd/raw_gramine.log | grep \"Write throughput\" | cut -d \")\" -f2 | xargs | cut -d \" \" -f1 | xargs | tr -d '\\n' >>$outd/raw_gramine.csv && \
            echo -n , >>$outd/raw_gramine.csv && \
            cat $outd/raw_gramine.log | grep \"Read throughput\" | cut -d \")\" -f2 | xargs | cut -d \" \" -f1 | xargs | tr -d '\\n' >>$outd/raw_gramine.csv && \
            echo -n , >>$outd/raw_gramine.csv && \
            echo RAND, >$outd/raw_gramine.log && \
            pushd /home/$USER/repos/gramine/CI-Examples/bash && \
            gramine-sgx ./bash -c \"test/rwlat r $num_samples $fsz $i\" >>$outd/raw_gramine.log && \
            popd && \
            cat $outd/raw_gramine.log | grep \"Write throughput\" | cut -d \")\" -f2 | xargs | cut -d \" \" -f1 | xargs | tr -d '\\n' >>$outd/raw_gramine.csv && \
            echo -n , >>$outd/raw_gramine.csv && \
            cat $outd/raw_gramine.log | grep \"Read throughput\" | cut -d \")\" -f2 | xargs | cut -d \" \" -f1 | xargs >>$outd/raw_gramine.csv"
    done
fi

####################################
# For testing without gramine:
# _mt=$MT _c=$CACHE bash -c "\
#     echo -e \#\\n\# Config:\\n\#\\tmt: $_mt, c: $_c\\n\#\\n && \
#     echo -n mt$_mt-c$_c, >>$outd/raw_gramine.csv && \
#     echo SEQ, >$outd/raw_gramine.log && \
#     micro/rwlat >>$outd/raw_gramine.log && \
#     cat $outd/raw_gramine.log | grep \"Write throughput\" | cut -d \")\" -f2 | xargs | cut -d \" \" -f1 | xargs | tr -d \\n >>$outd/raw_gramine.csv && \
#     echo -n , >>$outd/raw_gramine.csv && \
#     cat $outd/raw_gramine.log | grep \"Read throughput\" | cut -d \")\" -f2 | xargs | cut -d \" \" -f1 | xargs | tr -d \\n >>$outd/raw_gramine.csv && \
#     echo -n , >>$outd/raw_gramine.csv && \
#     echo RAND, >$outd/raw_gramine.log && \
#     micro/rwlat r >>$outd/raw_gramine.log && \
#     cat $outd/raw_gramine.log | grep \"Write throughput\" | cut -d \")\" -f2 | xargs | cut -d \" \" -f1 | xargs | tr -d \\n >>$outd/raw_gramine.csv && \
#     echo -n , >>$outd/raw_gramine.csv && \
#     cat $outd/raw_gramine.log | grep \"Read throughput\" | cut -d \")\" -f2 | xargs | cut -d \" \" -f1 | xargs >>$outd/raw_gramine.csv"
####################################

# plot
$BFS_HOME/benchmarks/micro/parse_raw_fs_perf.py
