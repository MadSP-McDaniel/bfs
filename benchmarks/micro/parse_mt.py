#!/usr/bin/env python3
#
# Script to parse merkle tree benchmark results.
#

# import matplotlib.pyplot as plt
import pandas as pd
import os

# configure which plots to generate
r = False
w = False
a = True

p = os.getenv("BFS_HOME") + "/benchmarks/micro/output/"
sub = True  # separate plots or single
# fig, axes = plt.subplots(nrows=2, ncols=2)

if r:
    res = pd.read_csv(p + "read_lats.csv", squeeze=True, header=None).T
    ax = res.plot(
        # ax=axes[0, 0],
        kind="hist",
        # kind="density",
        subplots=sub,
        title="block read",
        # xlabel="iteration",
        # ylabel="latency (µs)",
    )
    a = ax[0] if sub else ax
    # ax[0].spines["top"].set_visible(False)
    # ax[0].spines["right"].set_visible(False)
    # ax[0].legend(framealpha=0.5)
    # ax[0].grid(color="black", linestyle="--", linewidth=0.1, alpha=0.25)
    a.set_xlabel("Latency (µs)")
    a.get_figure().savefig(p + "read_lats.pdf")

if w:
    res = pd.read_csv(p + "write_lats.csv", squeeze=True, header=None).T
    ax = res.plot(
        # ax=axes[0, 1],
        kind="hist",
        subplots=sub,
        title="block write",
        # xlabel="iteration",
        # ylabel="latency (µs)",
    )
    a = ax[0] if sub else ax
    a.set_xlabel("Latency (µs)")
    a.get_figure().savefig(p + "write_lats.pdf")

if a:
    t = "blk_accesses_w"
    res = pd.read_csv(p + t + ".csv", squeeze=True, header=None).T
    ax = res.plot(
        # ax=axes[0, 1],
        kind="line",
        # kind="hist",
        subplots=sub,
        title=t,
        xlabel="time",
        ylabel="block address",
        style="x",
        ylim=(0, 10000),  # for easier visualizaing
        # xlim=(47100, 47350),  # for looking closer at certain trends
        xlim=(32900, 33050),
    )
    a = ax[0] if sub else ax
    # a.set_xlabel("Iteration")
    # mkfs is deterministic so these are OK (and if doing all blk accesses, we use 80141)
    if t == "blk_accesses_w":
        a.axvline(x=32946, color="r", linewidth=1.0, label="mkfs_done")
    else:
        a.axvline(x=47195, color="r", linewidth=1.0, label="mkfs_done")
    # a.get_figure().text(1.1,1,'blah',rotation=90)
    a.get_figure().savefig(p + t + ".png")
