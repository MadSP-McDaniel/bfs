#!/usr/bin/env python3

import matplotlib.pyplot as plt
import pandas as pd
import os


def set_patches(df):
    # return
    for i, bar in enumerate(df.patches):
        groups = 3
        if i >= groups * 5:
            bar.set_hatch("|||")
        elif i >= groups * 4:
            bar.set_hatch("\\\\")
        elif i >= groups * 3:
            bar.set_hatch("**")
        elif i >= groups * 2:
            bar.set_hatch("///")
        elif i >= groups:
            # bar.set_hatch("|||")
            pass
        elif i >= 0:
            bar.set_hatch("\\\\\\")


plt.style.use("bmh")
p = os.getenv("BFS_HOME") + "/benchmarks/new_bench/o/filebench-macro/"

fs_types = ["bfs_ne", "bfs_ci", "nfs_ne", "nfs_we"]
workloads = ["oltp", "videoserver", "webserver"]

rres = {workload: {fs_type: 0.0 for fs_type in fs_types} for workload in workloads}
wres = {workload: {fs_type: 0.0 for fs_type in fs_types} for workload in workloads}
for fs_type in fs_types:
    for workload in workloads:
        r = p + "bench_" + workload + "_" + fs_type + ".log"
        with open(r) as f:
            lines = f.readlines()

            tokens = None
            done = False
            for l in lines:
                tokens = l.split(" ")
                if workload == "oltp":
                    if "shadowread" in tokens:
                        break
                elif workload == "videoserver":
                    if "vidreader" in tokens:
                        break
                else:
                    if "readfile6" in tokens:
                        break

            tokens = [t for t in tokens if t]
            throughput = float(tokens[3][:-4])
            rres[workload][fs_type] = throughput

            tokens = None
            for l in lines:
                tokens = l.split(" ")
                if workload == "oltp":
                    if "dbwrite-a" in tokens:
                        break
                elif workload == "videoserver":
                    if "newvid" in tokens:
                        break
                else:
                    done = True

            if done:
                break

            tokens = [t for t in tokens if t]
            throughput = float(tokens[3][:-4])
            wres[workload][fs_type] = throughput

# plot write perf
df = pd.DataFrame(wres).T
fig, ax = plt.subplots()
# drop webserver from the plot
df = df.drop("webserver")
df.plot(
    kind="bar",
    subplots=False,
    rot=0,
    width=0.75,
    figsize=(6, 2.5),
    xlabel="Workloads",
    ylabel="Throughput (MB/s)",
    edgecolor="black",
    ax=ax,
)
set_patches(ax)
handles, labels = ax.get_legend_handles_labels()
labels = [l.replace("bfs_ci", "bfs") for l in labels]
# ax.set_yscale("log")
ax.legend(
    handles,
    labels,
    fontsize=12,
    ncol=6,
    bbox_to_anchor=(1.0, 1.4),
    edgecolor="white",
)
ax.grid(visible=True, alpha=0.5)
plt.savefig(p + "filebench-macro-write.pdf", bbox_inches="tight")

# plot read perf
df = pd.DataFrame(rres).T
fig, ax = plt.subplots()
df.plot(
    kind="bar",
    subplots=False,
    rot=0,
    width=0.75,
    figsize=(6, 2.5),
    xlabel="Workloads",
    ylabel="Throughput (MB/s)",
    edgecolor="black",
    ax=ax,
)
set_patches(ax)
handles, labels = ax.get_legend_handles_labels()
labels = [l.replace("bfs_ci", "bfs") for l in labels]
ax.set_yscale("log")
ax.legend(
    handles,
    labels,
    fontsize=12,
    ncol=6,
    bbox_to_anchor=(1.0, 1.4),
    edgecolor="white",
)
ax.grid(visible=True, alpha=0.5)
plt.savefig(p + "filebench-macro-read.pdf", bbox_inches="tight")
