import os
import glob

p = os.getenv("BFS_HOME") + "/benchmarks/new_bench/o/filebench-directio/tmp/"
# We want to loop through all files in p and split them into multiple files. Each file in p is of the form:
# [Run params: bfs_ci-rread-4096-nc1-ncseq1]
# WARNING: Could not open /proc/sys/kernel/shmmax file!
# It means that you probably ran Filebench not as a root. Filebench will not increase shared
# region limits in this case, which can lead to the failures on certain workloads.
# Filebench Version 1.5-alpha3
# 0.000: Allocated 177MB of shared memory
# 0.001: Populating and pre-allocating filesets
# 0.003: test-fileset populated: 1 files, avg. dir. width = 1, avg. dir. depth = -nan, 0 leafdirs, 1.000MB total size
# 0.005: Removing test-fileset tree (if exists)
# 0.010: Pre-allocating directories in test-fileset tree
# 0.024: Pre-allocating files in test-fileset tree
# 0.098: Waiting for pre-allocation to finish (in case of a parallel pre-allocation)
# 0.098: Population and pre-allocation of filesets completed
# 0.098: Starting 1 filereader instances
# 1.104: Running...
# 31.108: Run took 30 seconds...
# 31.108: Per-Operation Breakdown
# closeOP              385ops       13ops/s   0.0mb/s    0.001ms/op [0.001ms - 0.004ms]
# rreadOP              98663ops     3288ops/s  12.8mb/s    0.299ms/op [0.265ms - 10.739ms]
# openOP               386ops       13ops/s   0.0mb/s    1.013ms/op [0.763ms - 13.753ms]
# 31.108: IO Summary: 99434 ops 3314.150 ops/s 3288/0 rd/wr  12.8mb/s 0.301ms/op
# 31.108: Shutting down processes

# We want to split them into multiple files named after the run params as follows: rread_1_4096_bfs_ci_1.log

files = glob.glob(p + "*")
for f in files:
    with open(f, "r") as file:
        data = file.readlines()
        for i in range(len(data)):
            first_line = 0
            if "Run params" not in data[i]:
                continue
            
            first_line = i
            
            # now find the last line, which is basically the next line that starts with "Run params"
            last_line = 0
            for j in range(i + 1, len(data)):
                if "Run params" in data[j]:
                    last_line = j - 1
                    break

            run_params = data[first_line].split(":")[1].strip()
            run_params = run_params.split("-")
            new_fs_type = run_params[0]
            
            if new_fs_type == "bfs":
                new_fs_type = "bfs_ne"
            if new_fs_type == "nfs_ci":
                new_fs_type = "nfs_we"
            if new_fs_type == "nfs":
                new_fs_type = "nfs_ne"
            if new_fs_type == "nfsg":
                new_fs_type = "nfsg_ne"
                
            newf = (
                run_params[1] + "_1_" + run_params[2] + "_" + new_fs_type + "_1.log"
            )
            
            if not os.path.exists(p[:-4] + newf):    
                with open(p[:-4] + newf, "w") as newfile:
                    newfile.writelines(data[first_line:last_line])
            
            i = last_line
