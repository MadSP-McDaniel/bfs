#!/usr/bin/env python3
#
# Description: Script to parse macrobenchmark results.
#

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import matplotlib.gridspec as gridspec
import os
import csv
import sys
from enum import Enum
from matplotlib.patches import Patch
from matplotlib.lines import Line2D


def get_util_lats(_num_clients, _cfgs, dev_type):
    cfgs = _cfgs.split(",")[:-1]  # get rid of last comma
    num_clients = int(_num_clients)

    # default the path prefix to use local dev results
    _dev_type = dev_type if dev_type is not None else "local"

    new_cfgs = [None] * len(cfgs)
    for i in range(len(cfgs)):
        if cfgs[i] == "nfs":
            new_cfgs[i] = "nfs_ne"
        elif cfgs[i] == "nfs_ci":
            new_cfgs[i] = "nfs_we"
        elif cfgs[i] == "nfsg":
            new_cfgs[i] = "nfsg_ne"
        elif cfgs[i] == "nfsg_ci":
            new_cfgs[i] = "nfsg_we"
        elif cfgs[i] == "bfs":
            new_cfgs[i] = "bfs_ne"
        elif cfgs[i] == "bfs_ci":
            new_cfgs[i] = "bfs-c0"
        elif cfgs[i] == "bfs_ci_c256":
            new_cfgs[i] = "bfs-c256"

    # res = [None for _ in range(len(cfgs))]
    res = list()
    # res = None
    for cfg_idx in range(len(cfgs)):
        _res_avg = None
        for c_idx in range(1, num_clients + 1):
            _res = pd.read_csv(
                os.getenv("BFS_HOME")
                + "/benchmarks/macro/output--%s/%s-nc%d-c%d.csv"
                % (_dev_type, cfgs[cfg_idx], num_clients, c_idx)
            )

            # take mean for this client (then mean of all clients together, below after joining)
            _res = (
                _res.dropna(axis=1)
                .set_index("cmd")
                .mean(1)
                .to_frame()
                .set_axis(["avg"], axis=1)
            )

            if c_idx == 1:
                _res_avg = _res
            else:
                _res_avg = _res_avg.join(
                    _res,
                    rsuffix="_c%d" % c_idx,
                    # rsuffix="_ncseq%d_c%d" % (ncseq, c_idx),
                )

            # for macro we use bar plots, so to keep things readable just plot the results for the actual number of clients specified
            # _res = _res.set_index([_res.command])
            # _res = _res.dropna(axis=1).drop(
            #     [cmd for cmd in _res.index if (cmd.find("-nc%d" % num_clients) == -1)],
            #     axis=0,
            # )

            # insert columns to pivot on
            # _res.insert(
            #     0,
            #     "utility",
            #     [cmd.split(cfgs[cfg_idx] + "-")[1] for cmd in list(_res.command)],
            # )
            # _res.insert(
            #     0,
            #     "nc",
            #     [cmd.split("-nc")[-1] for cmd in list(_res.command)],
            # )
            # _res.insert(
            #     0,
            #     "fs_type",
            #     [cfgs[cfg_idx] for _ in _res.command],
            # )
            # _res.insert(
            #     0,
            #     "avg_lat",
            #     [
            #         float(_res.loc[cmd]["mean"]) / float(_res.loc[cmd]["nc"])
            #         for cmd in _res.command
            #     ],
            # )
        _res_avg = _res_avg.mean(1).to_frame().T
        _res_avg.insert(
            0,
            "fs_type",
            [new_cfgs[cfg_idx] for _ in list(_res_avg.index)],
        )
        # _res_avg = _res_avg.T
        res.append(_res_avg)
        # if cfg_idx == 0:
        #     res = _res_avg
        # else:
        #     res = res.join(_res_avg)

    res = pd.concat(res, axis=0).set_index("fs_type").T
    # res.insert(
    #     0,
    #     "utility",
    #     [c for c in list(res.index)],
    # )
    # for cfg_idx in range(len(cfgs)):
    #     # res[cfg_idx] = res[cfg_idx].drop(
    #     #     ["cmd_c%d" % c for c in range(2, num_clients + 1)],
    #     #     axis=1,
    #     # )

    #     # res[cfg_idx] = (
    #     #     res[cfg_idx].set_index("cmd").mean(1).to_frame().set_axis(["avg"], axis=1)
    #     # )
    #     res[cfg_idx] = res[cfg_idx].mean(1).to_frame().set_axis(["avg"], axis=1)

    #     res[cfg_idx].insert(
    #         0,
    #         "fs_type",
    #         [cfgs[cfg_idx] for _ in list(res[cfg_idx].index)],
    #     )

    #     res[cfg_idx].insert(
    #         0,
    #         "_cmd",
    #         [c for c in list(res[cfg_idx].index)],
    #     )

    # plt.style.use("seaborn-deep")
    plt.style.use("bmh")
    patterns = ["", "////", "xxx", "\\\\\\", "-----", "+", "|", ".", "O", "*", "-"]
    fs_colors = ["white", "grey", "black"]
    plt.figure()
    fig, _ax = plt.subplots(figsize=(4, 1.5))
    fig.tight_layout()
    # fig, (_ax, ax2) = plt.subplots(2, 1, sharex=True, figsize=(4, 1.5))
    # fig.subplots_adjust(hspace=0.25)
    # fig.tight_layout()
    fig.text(0.5, -0.05, "Utility", ha="center")
    fig.text(
        -0.04,
        0.5,
        "Overhead (X)\n  %s(%d client%s)"
        % ("" if num_clients > 1 else " ", num_clients, "s" if num_clients > 1 else ""),
        va="center",
        rotation="vertical",
    )

    # res_errs = res[["fs_type", "utility", "stddev"]].to_numpy().T
    # res_errs = res.pivot("utility", "fs_type", "stddev")
    # res_errs = res.pivot("cmd", "fs_type", "stddev")

    # res_plot = res.pivot("utility", "fs_type", "avg").plot(
    # res_plot = (
    #     res.div(res.nfs_ne, axis=0)
    #     .sub(1, axis=0)
    #     .clip(lower=0)
    #     .drop("nfs_ne", axis=1)
    #     .plot(
    #         kind="bar",
    #         ax=_ax,
    #         # x="command",
    #         # y="mean",
    #         # yerr=res_errs,
    #         rot=0,
    #         capsize=2,
    #         # fontsize=15,
    #         color="white",
    #         edgecolor="black",
    #         ylim=(7, 30) if num_clients == 1 else (10, 30),
    #         # xlim=(0, max(_res_sums[op_idx][cfg_idx].iosz)),
    #     )
    # )
    res_plot2 = (
        res.div(res.nfs_ne, axis=0)
        .sub(1, axis=0)
        .clip(lower=0)
        .drop("nfs_ne", axis=1)
        .plot(
            kind="bar",
            ax=_ax,
            # x="command",
            # y="mean",
            # yerr=res_errs,
            rot=0,
            capsize=2,
            # fontsize=15,
            legend=False,
            # color="white",
            edgecolor="black",
            ylim=(0, 7.5) if num_clients == 1 else (0, 10),
            # xlim=(0, max(_res_sums[op_idx][cfg_idx].iosz)),
        )
    )

    # add break in y-axis at 20
    # _ax.axhline(20, color="black", linewidth=1)
    # _ax.axhline(1, color="black", linewidth=1)

    # for patch_idx in range(len(res_plot.patches)):
    #     res_plot.patches[patch_idx].set_hatch(
    #         # patterns[patch_idx // len(res[cfg_idx].columns)]
    #         patterns[patch_idx // len(res.index)]
    #     )
    #     if patch_idx // len(res.index) == 1:
    #         res_plot.patches[patch_idx].set_fc("black")

    for patch_idx in range(len(res_plot2.patches)):
        res_plot2.patches[patch_idx].set_hatch(
            # patterns[patch_idx // len(res[cfg_idx].columns)]
            patterns[patch_idx // len(res.index)]
        )
        if patch_idx // len(res.index) == 1:
            res_plot2.patches[patch_idx].set_fc("black")

    # _ax.legend(framealpha=0.0)
    _ax.legend(
        framealpha=0.0,
        edgecolor=None,
        facecolor="black",
        bbox_to_anchor=(-0.25, 1.25, 0, 0),
        loc=(0.0, 0.0),
        ncol=4,
        markerscale=1.0,
    )
    # _ax.set_yscale("log")

    # Note: these limits really depend on the settings in the macro workload files (ie 10 or 1000 file copies)
    # hline_y = 1
    # if _dev_type == "remote":
    #     if num_clients == 1:
    #         _ax.set_ylim(1, 100)
    #         hline_y = 10
    #     else:
    #         _ax.set_ylim(1, 1000)
    #         hline_y = 100
    # else:
    #     if num_clients == 1:
    #         _ax.set_ylim(0, 30)
    #         hline_y = 4
    #     else:
    #         _ax.set_ylim(0, 40)
    #         hline_y = 10
    # _ax.axhline(y=hline_y, color="r", linewidth=1)
    # _ax.axhline(y=1, color="r", linewidth=0.5)
    _ax.axhline(y=1, color="r", linewidth=0.5)
    # _ax.grid(visible=True, axis='y', which='both', color='r', alpha=0.8)
    _ax.grid(color="black", axis="y", linestyle="--", linewidth=0.5, alpha=0.5)

    # _ax.set_title(ops[op_idx], fontsize=10)
    # _ax.spines["top"].set_visible(False)
    # _ax.spines["right"].set_visible(False)
    # _ax.spines["bottom"].set_visible(False)
    # _ax.tick_params(bottom=False)
    # _ax.set_xlabel("")
    # _ax.axes.get_xaxis().set_label_text("")
    # _ax.spines["top"].set_visible(False)
    # _ax.spines["right"].set_visible(False)
    _ax.set_xlabel("")
    _ax.set_facecolor("white")

    # d = 0.5  # proportion of vertical to horizontal extent of the slanted line
    # kwargs = dict(
    #     marker=[(-1, -d), (1, d)],
    #     markersize=10,
    #     linestyle="none",
    #     color="k",
    #     mec="k",
    #     mew=1,
    #     clip_on=False,
    # )
    # _ax.plot([0, 1], [0, 0], transform=_ax.transAxes, **kwargs)
    # ax2.plot([1, 0], [1, 1], transform=ax2.transAxes, **kwargs)
    # if num_clients==1:
    #     _ax.plot(
    #         [0, 0.115, 0.45, 0.615, 0.78], [0, 0, 0, 0, 0], transform=_ax.transAxes, **kwargs
    #     )
    #     ax2.plot(
    #         [0, 0.115, 0.45, 0.615, 0.78],
    #         [1, 1, 1, 1, 1],
    #         transform=ax2.transAxes,
    #         **kwargs
    #     )
    # else:
    #     _ax.plot(
    #         [0, 0.28, 0.45, 0.615, 0.78], [0, 0, 0, 0, 0], transform=_ax.transAxes, **kwargs
    #     )
    #     ax2.plot(
    #         [0, 0.28, 0.45, 0.615, 0.78],
    #         [1, 1, 1, 1, 1],
    #         transform=ax2.transAxes,
    #         **kwargs
    #     )

    # plt.gca().spines["right"].set_visible(False)
    # plt.gca().spines["top"].set_visible(False)
    # plt.gca().legend(framealpha=0.0, fontsize=15)

    # plt.ylabel("latency (s)",fontsize=15)
    # plt.xlabel("utility",fontsize=15)
    plt.savefig(
        os.getenv("BFS_HOME")
        + (
            "/benchmarks/macro/output--%s/macro_lat_res_multi.pdf" % _dev_type
            if num_clients > 1
            else "/benchmarks/macro/output--%s/macro_lat_res.pdf" % _dev_type
        ),
        bbox_inches="tight",
    )


if __name__ == "__main__":
    print("Plotting macrobenchmark results:")
    if len(sys.argv) < 3:
        print("Invalid number of args")
        exit(-1)
    get_util_lats(
        sys.argv[1],
        sys.argv[2],
        sys.argv[3] if len(sys.argv) > 3 else None,
    )
    print("Done")
