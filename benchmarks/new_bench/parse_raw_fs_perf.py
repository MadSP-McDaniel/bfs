#!/usr/bin/env python3
# Parse results for raw FS performance of BFS and Gramine.

import matplotlib.pyplot as plt
import pandas as pd
import os

def set_patches(df):
    # return
    for i, bar in enumerate(df.patches):
        groups = 16
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
            

# plt.style.use("fivethirtyeight")
# plt.style.use("grayscale")
plt.style.use("bmh")
p = os.getenv("BFS_HOME") + "/benchmarks/new_bench/o/gramine/"
# sub = False  # separate plots or single
# fig, axes = plt.subplots(nrows=2, ncols=2)
bfs_res = pd.read_csv(p + "raw_bfs.csv").set_index("config")
# bfs_res.insert(0, "fs_type", "bfs_seq")
gramine_res = pd.read_csv(p + "raw_gramine.csv").set_index("config")
# gramine_res.insert(0, "fs_type", "gramine_seq")
res = bfs_res / gramine_res  # compute speedup
# res = pd.concat([bfs_res, gramine_res], axis=0).set_index("fs_type")

# for i in range(1, len(res.columns)):
#     res.iloc[:, i] = res.iloc[:, i].apply(lambda x: int(x.split()[0]))

ax = res.plot(
    kind="bar", subplots=False, rot=35, width=0.75, figsize=(6, 2.5), ylim=(0, 4), edgecolor="black"
)
set_patches(ax)

# ax = (
#     res.div(res.gramine_seq, axis=0)
#     .sub(1, axis=0)
#     .clip(lower=0)
#     .drop("gramine_seq", axis=1)
#     .T.plot(kind="bar", subplots=False, rot=0, width=1, ylim=(0, 1.2), figsize=(5, 2.5))
# )
# a = ax[0] if sub else ax
# ax.spines["top"].set_visible(False)
# ax.spines["right"].set_visible(False)
ax.axvline(3.5, color="black", linestyle="-", linewidth=1.0)
ax.axvline(7.5, color="black", linestyle="-", linewidth=1.0)
ax.axvline(11.5, color="black", linestyle="-", linewidth=1.0)
ax.legend(framealpha=1.0, edgecolor="none", facecolor="white", fontsize=7, ncol=2)
ax.grid(color="black", axis="y", linestyle="--", linewidth=0.5, alpha=0.5)
ax.set_xlabel("System configuration")
ax.set_ylabel("Speedup (X)")
ax.set_facecolor("white")
ax.get_figure().tight_layout()
ax.get_figure().savefig(p + "raw.pdf")
