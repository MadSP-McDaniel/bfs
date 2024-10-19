#!/usr/bin/env python3
# Parse results for raw FS performance of BFS and ZeroTrace.

import matplotlib.pyplot as plt
import pandas as pd
import os


def set_patches(df):
    # return
    for i, bar in enumerate(df.patches):
        groups = 5
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
p = os.getenv("BFS_HOME") + "/benchmarks/new_bench/o/utilities-async/"

fs_types = ["bfs_ne", "bfs_ci", "nfs_ne", "nfs_we"]

bfs_ci_res = pd.read_csv(p + "bfs_ci.csv")
bfs_ci_res.index = ["bfs_ci"]
bfs_ne_res = pd.read_csv(p + "bfs_ne.csv")
bfs_ne_res.index = ["bfs_ne"]
nfs_ne_res = pd.read_csv(p + "nfs_ne.csv")
nfs_ne_res.index = ["nfs_ne"]
nfs_we_res = pd.read_csv(p + "nfs_we.csv")
nfs_we_res.index = ["nfs_we"]

# plot a grouped bar chart
res = pd.concat([bfs_ne_res, bfs_ci_res, nfs_ne_res, nfs_we_res], axis=0)
res = res / 1000
ax = res.T.plot(
    kind="bar",
    subplots=False,
    rot=0,
    width=0.75,
    figsize=(6, 2.5),
    xlabel="Utilities",
    ylabel="Latency (ms)",
    edgecolor="black",
)
set_patches(ax)
handles, labels = ax.get_legend_handles_labels()
labels = [l.replace("bfs_ci", "bfs") for l in labels]
ax.legend(
    handles,
    labels,
    framealpha=1.0,
    edgecolor="none",
    facecolor="white",
    fontsize=7,
    ncol=2,
)
ax.grid(color="black", axis="y", linestyle="--", linewidth=0.5, alpha=0.5)
ax.set_facecolor("white")
ax.get_figure().tight_layout()
ax.get_figure().savefig(p + "utilities.pdf")
