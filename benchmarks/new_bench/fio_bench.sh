#!/bin/bash

# This script runs our full benchmark suite.

set -e -x

function flush_caches {
    sync
    echo 3 | sudo tee /proc/sys/vm/drop_caches
    sleep 5
}

function check_env {
    if [[ -z "$BFS_HOME" ]]; then
        echo "BFS_HOME is not set, aborting."
        exit 1
    fi

    mkdir -p $BFS_HOME/benchmarks/new_bench/o/fio_test || true
    rm *_server_current.log || true
}

check_env

# Specify the devices
fs_type=nfs_ne
# use_static_block=0
# use_chunked_io=1
# arity=2
ramp_time=30

# Specify experiment params
mode=randread
runtime=60
direct=0
e=sync
fsync=0
fdatasync=0
wrbarrier=0
# use_fs=1

_js="1"
depths="1"
caps="$((2 ** 30))"
cache_sizes="0.0"
exp_types="0"
mt_types="-1"
workloads="random"
_io_sizes="131072"
_rrs="100"

for dep in $depths; do
for cap in $caps; do
for cache_sz in $cache_sizes; do
for exp_type in $exp_types; do
for workload in $workloads; do
for mt in $mt_types; do

# Check if we are replaying an iolog or using an fio-generated workload. Dont
# iterate over multiple js when replaying iologs. And if the iolog is not
# fio-generated (but converted from Tencent/Alibaba traces), then we also
# should not iterate over multiple rrs/io_sizes.
js=$_js # reset these because we might mix replayed/non-replayed workloads
rrs=$_rrs
io_sizes=$_io_sizes
# replay_log_base=""
# if [[ $workload == replay* ]]; then
#     replay_log_base=$(echo $workload | cut -d'-' -f2)
#     # js="1" # only replaying 1 iolog, which will be assigned to 1 job

#     if [[ $replay_log_base == scaled* ]]; then
#         rrs="0" # determined by the iolog
#         io_sizes="4k" # determined by the iolog but output dir defaults to 4k
#     fi
# fi

for j in $js; do
for io_sz in $io_sizes; do
for rr in $rrs; do

# # Set the exact replay log name if we are replaying an iolog.
# replay_log=""
# if [ ! -z "$replay_log_base" ]; then
#     replay_log="workloads/traces"
#     if [[ $replay_log_base == scaled* ]]; then
#         replay_log="${replay_log}/${replay_log_base}_$cap"
#     else
#         replay_log="${replay_log}/iolog_${cap}_${replay_log_base}_${mode}${rr}_${io_sz}"
#     fi
# fi

# Set the correct workload stem for output data.
wl_stem=""
if [ ! -z "$replay_log_base" ]; then
    wl_stem=$replay_log_base
else
    wl_stem=$workload
fi

exp_type_str="raw"

# # Since we must have a trace profile for OPT, just skip this experiment if
# # the mt type is OPT but there is no replay log (i.e., when we are running
# # any fio-generated workloads). Note that we may do both kinds of experiments
# # for our analysis.
# if [ $mt -eq 3 ] && [ -z "$replay_log" ]; then
#     echo "Skipping experiment [$exp_type_str/mt$mt/$wl_stem/c$cache_sz/$cap]"
#     continue
# fi

# # If we are doing dm-verity, then mt_type must be -1 and rr must be 100,
# # otherwise skip this experiment.
# if [ $exp_type -eq 4 ]; then
#     if [ $mt -ne -1 ] || [ $rr -ne 100 ]; then
#         echo "Skipping experiment [$exp_type_str/mt$mt/$wl_stem/c$cache_sz/$cap]"
#         continue
#     fi
# fi

# Flush first (need to fix issue with device perms not being saved after flush)
flush_caches

outdir=$BFS_HOME/benchmarks/new_bench/o/fio_test/$exp_type_str/mt$mt/$wl_stem/c$cache_sz/$cap

# # Initialize the bdus device, unless exp_type is dm-verity.
# top_dev="/dev/mapper/data_disk"
# if [ $exp_type -ne 4 ]; then
#     init_bdus_dev $cache_sz $mt $use_static_block $use_chunked_io $arity $cap $replay_log
# fi

# Setup extra workload params based on if we are replaying an iolog or not
# iostat_opts="exec_prerun=\"$BFS_HOME/benchmarks/start_iostat.sh\" exec_postrun=\"$BFS_HOME/benchmarks/stop_iostat.sh\""
ex=""
# if [ -z "$replay_log" ]; then
ex="--extra-opts ramp_time=$ramp_time group_reporting=1 \
    write_barrier=$wrbarrier fdatasync=$fdatasync fsync=$fsync norandommap=1 \
    random_distribution=$workload" #$iostat_opts 
# Only write the iolog for mt_type==-1
# if [ $mt -eq -1 ]; then
#     ex="$ex write_iolog=iolog_${cap}_${workload}_${mode}${rr}_${io_sz}"
# fi
# else
#     # if j>1 then we need to repeat the iolog for each job.
#     # use eval to expand the variable
#     replay_log_escaped=$(echo $replay_log | sed 's/:/\\:/g')
#     replay_log_escaped=$(eval $(echo printf '"$replay_log_escaped:%0.s"' {1..$j}))
#     ex="--extra-opts ramp_time=$ramp_time group_reporting=1 write_barrier=$wrbarrier fdatasync=$fdatasync fsync=$fsync \
#         read_iolog=$replay_log_escaped replay_redirect=$top_dev replay_align=4k $iostat_opts"
# fi

# if [ $use_fs -eq 0 ]; then
#     # Run the workload against the bdus device
#     sudo BFS_HOME=$BFS_HOME bench-fio --target $top_dev --type device --iodepth $dep \
#         --runtime $runtime -o $outdir --direct $direct -e $e --time-based --numjobs $j \
#         -m $mode -b $io_sz --rwmixread $rr $ex
# else

# Run the workload against a mounted filesystem
# sudo umount /mnt/${fs_type} || true
# sudo mkfs.ext4 $top_dev
# sudo mount $top_dev /mnt/${fs_type}
# sudo chown -R $USER:$USER /mnt/${fs_type}
chmod -R 777 /mnt/${fs_type} || true
sleep 1
# set size (-s) at 90% of the cap
BFS_HOME=$BFS_HOME bench-fio --target /mnt/${fs_type} --type directory --iodepth 1 \
    -o $outdir --direct $direct -e $e --time-based --numjobs $j -m $mode -b $io_sz \
    --rwmixread $rr -s $((cap * 75 / 100)) --destructive --runtime $runtime $ex
# fi

# # Cleanup
# if [ $exp_type -ne 4 ]; then
#     cleanup_bdus_dev
# fi

chown -R $USER:$USER o
# sudo chown -R $USER:$USER FIOJOB.prerun.txt || true
# sudo chown -R $USER:$USER FIOJOB.postrun.txt || true
# if [ $use_fs -eq 0 ]; then
#     sudo mv bench_dev.tmp.log $outdir/$(basename $top_dev)/$mode$rr/$io_sz || true
#     sudo mv o/*.csv $outdir/$(basename $top_dev)/$mode$rr/$io_sz || true
#     sudo mv o/iostat.log $outdir/$(basename $top_dev)/$mode$rr/$io_sz || true
#     sudo mv FIOJOB.prerun.txt $outdir/$(basename $top_dev)/$mode$rr/$io_sz || true
#     sudo mv FIOJOB.postrun.txt $outdir/$(basename $top_dev)/$mode$rr/$io_sz || true
# else
# mv bfs*.log $outdir/${fs_type}/$mode$rr/$io_sz || true
mv o/*.csv $outdir/${fs_type}/$mode$rr/$io_sz || true
mv o/iostat.log $outdir/${fs_type}/$mode$rr/$io_sz || true
mv FIOJOB.prerun.txt $outdir/${fs_type}/$mode$rr/$io_sz || true
mv FIOJOB.postrun.txt $outdir/${fs_type}/$mode$rr/$io_sz || true
# fi

done
done
done
done
done
done
done
done
done

sudo chown -R $USER:$USER $BFS_HOME/benchmarks/new_bench/o
echo "Finished experiments"
