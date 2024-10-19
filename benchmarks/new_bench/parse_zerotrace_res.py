#!/usr/bin/env python3
# Parse results for raw FS performance of BFS and ZeroTrace.

import matplotlib.pyplot as plt
import pandas as pd
import os


def set_patches(df):
    # return
    for i, bar in enumerate(df.patches):
        groups = 4
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
p = os.getenv("BFS_HOME") + "/benchmarks/new_bench/o/zerotrace/"

bfs_res = {
    "rwrite": {
        "128K": 230.579,
        "64K": 220.850,
        "16K": 160.906,
        "4K": 82.089,
    },
    "rread": {
        "128K": 312.489,
        "64K": 309.118,
        "16K": 293.335,
        "4K": 208.559,
    },
}

zt_res = {
    "rwrite": {
        "128K": 67.014610,
        "64K": 32.890418,
        "16K": 0.684073,
        "4K": 0.489695,
    },
    "rread": {
        "128K": 65.795729,
        "64K": 33.057201,
        "16K": 0.684682,
        "4K": 0.487707,
    },
}

# plot a grouped bar chart
bfs_res_df = pd.DataFrame.from_dict(bfs_res)
zt_res_df = pd.DataFrame.from_dict(zt_res)
res = bfs_res_df / zt_res_df
# sort the index
res = res.reindex(
    ["4K", "16K", "64K", "128K"], axis=0
)
ax = res.plot(
    kind="bar", subplots=False, rot=0, width=0.75, figsize=(6, 2.5), edgecolor="black"
)
set_patches(ax)
# ax.axvline(3.5, color="black", linestyle="-", linewidth=1.0)
# ax.axvline(7.5, color="black", linestyle="-", linewidth=1.0)
# ax.axvline(11.5, color="black", linestyle="-", linewidth=1.0)
ax.legend(framealpha=1.0, edgecolor="none", facecolor="white", fontsize=7, ncol=2)
ax.grid(color="black", axis="y", linestyle="--", linewidth=0.5, alpha=0.5)
# draw text on top of bars
for i, v in enumerate(res[-2:].values.flatten()):
    ax.text(
        i / 2 + 1.75,
        v,
        f"{v:.2f}",
        ha="center",
        va="bottom",
        color="black",
        fontsize=10,
    )
ax.set_xlabel("I/O Size")
ax.set_ylabel("Speedup (X)")
ax.set_facecolor("white")
ax.get_figure().tight_layout()
ax.get_figure().savefig(p + "raw.pdf")



# ax = res.plot(
#     kind="bar", subplots=False, rot=35, width=0.75, figsize=(6, 2.5), ylim=(0, 4)
# )
# set_patches(ax)
# ax.axvline(3.5, color="black", linestyle="-", linewidth=1.0)
# ax.axvline(7.5, color="black", linestyle="-", linewidth=1.0)
# ax.axvline(11.5, color="black", linestyle="-", linewidth=1.0)
# ax.legend(framealpha=1.0, edgecolor="none", facecolor="white", fontsize=7, ncol=2)
# ax.grid(color="black", axis="y", linestyle="--", linewidth=0.5, alpha=0.5)
# ax.set_xlabel("System configuration")
# ax.set_ylabel("Speedup (X)")
# ax.set_facecolor("white")
# ax.get_figure().tight_layout()
# ax.get_figure().savefig(p + "raw.pdf")
