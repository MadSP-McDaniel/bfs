#!/bin/bash

set -e -x

mkdir -p o/utilities || true
mp="/mnt/bfs_ne"
sync
echo 3 | sudo tee /proc/sys/vm/drop_caches

echo "Running experiments: [mp=$mp]"
rm -rf $mp/* &>/dev/null || true
sleep 1

echo "Running git-clone"
num_samples=10
# clone_time=`/usr/bin/time -f "%e" git clone https://github.com/intel/linux-sgx-driver $mp/linux-sgx-driver 2>&1 | tail -1 | xargs`
start_time=$(date +%s%6N)
for s in $(seq 1 $num_samples); do
    git clone https://github.com/intel/linux-sgx-driver $mp/linux-sgx-driver-$s &>/dev/null
done
end_time=$(date +%s%6N)
clone_time=$((end_time - start_time))
clone_time=$((clone_time / num_samples))
sleep 1

echo "Running grep"
num_samples=30
start_time=$(date +%s%6N)
for s in $(seq 1 $num_samples); do
    grep -R "sgx$s" $mp/linux-sgx-driver-1 &>/dev/null || true
done
end_time=$(date +%s%6N)
grep_time=$((end_time - start_time))
grep_time=$((grep_time / num_samples))
sleep 1

echo "Running tar-c"
num_samples=30
start_time=$(date +%s%6N)
for s in $(seq 1 $num_samples); do
    sudo chmod -R 777 $mp/linux-sgx-driver-1
    tar czf $mp/macro_tar-$s.tgz $mp/linux-sgx-driver-1
done
end_time=$(date +%s%6N)
tar_time=$((end_time - start_time))
tar_time=$((tar_time / num_samples))
sleep 1

echo "Running tar-x"
num_samples=30
start_time=$(date +%s%6N)
for s in $(seq 1 $num_samples); do
    sudo chmod -R 777 $mp/linux-sgx-driver-1
    tar xzf $mp/macro_tar-$s.tgz -C $mp/linux-sgx-driver-1
done
end_time=$(date +%s%6N)
untar_time=$((end_time - start_time))
untar_time=$((untar_time / num_samples))
sleep 1

echo "Running make"
num_samples=30
start_time=$(date +%s%6N)
for s in $(seq 1 $num_samples); do
    cp -r $BFS_HOME/benchmarks/macro/bzip2 $mp/bzip2-$s &>/dev/null
    # make -C $mp/bzip2-$s clean &>/dev/null
    make -C $mp/bzip2-$s all &>/dev/null
done
end_time=$(date +%s%6N)
make_time=$((end_time - start_time))
make_time=$((make_time / num_samples))
sleep 1

# dump the results to a csv of the form:
# <benchmark>,<clone_time>,<grep_time>,<tar_time>,<untar_time>,<make_time>
# dump header
echo "clone,grep,tar-c,tar-x,make" >$BFS_HOME/benchmarks/new_bench/o/utilities/$(basename $mp).csv
echo "$clone_time,$grep_time,$tar_time,$untar_time,$make_time" >>$BFS_HOME/benchmarks/new_bench/o/utilities/$(basename $mp).csv

echo "Done"
