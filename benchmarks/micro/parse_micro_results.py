#!/usr/bin/env python3
#
# Description: Script to parse microbenchmark results.
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

# import seaborn as sns


def get_tp(_num_clients, _ops, _cfgs, dev_type):
    # parse all results and set up dataframes
    ops = _ops.split(",")
    cfgs = _cfgs.split(",")[:-1]  # get rid of last comma
    num_clients = int(_num_clients)

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
            new_cfgs[i] = "bfs"

    # default the path prefix to use local dev results
    _dev_type = dev_type if dev_type is not None else "local"
    # _res = [
    #     {op_idx: [None] * len(cfgs) for op_idx in range(len(ops))}
    #     for _ in range(1, num_clients + 1)
    # ]
    _res_sums = {op_idx: [None] * len(new_cfgs) for op_idx in range(len(ops))}

    for ncseq in range(1, num_clients + 1):
        for c_idx in range(1, ncseq + 1):
            for op_idx in range(len(ops)):
                for cfg_idx in range(len(new_cfgs)):
                    _res = pd.read_csv(
                        os.getenv("BFS_HOME")
                        + "/benchmarks/micro/output--%s/%s-%s-nc%d-ncseq%d-c%d.csv"
                        % (
                            _dev_type,
                            cfgs[cfg_idx],
                            ops[op_idx],
                            num_clients,
                            ncseq,
                            c_idx,
                        )
                    )
                    _res = _res.dropna(axis=1).drop("sample", axis=1)
                    _res = _res.mean(0).to_frame()
                    _res = _res.set_axis(["avg"], axis=1)

                    if (ncseq == 1) and (c_idx == 1):
                        _res_sums[op_idx][cfg_idx] = _res.copy()
                    # elif c_idx > 1:
                    else:
                        _res_sums[op_idx][cfg_idx] = _res_sums[op_idx][cfg_idx].join(
                            _res,
                            rsuffix="_ncseq%d_c%d" % (ncseq, c_idx),
                            # rsuffix="_ncseq%d_c%d" % (ncseq, c_idx),
                        )
                        # _res_sums[op_idx][cfg_idx] = _res_sums[op_idx][cfg_idx][""]
                    # _res.insert(
                    #     0,
                    #     "_avg",
                    #     [avg / 1e3 for avg in list(_res.avg)],
                    # )
                    # _res.insert(
                    #     0,
                    #     "tp",
                    #     [
                    #         (
                    #             int(_res.index[iosz_idx])
                    #             * _res.avg[iosz_idx]
                    #         )
                    #         / 1e6
                    #         for iosz_idx in range(len(list(_res.index)))
                    #     ],
                    # )

    _res_sums_per_ncseq = {op_idx: [None] * len(new_cfgs) for op_idx in range(len(ops))}
    for op_idx in range(len(ops)):
        for cfg_idx in range(len(new_cfgs)):
            _res_sums_per_ncseq[op_idx][cfg_idx] = pd.DataFrame()
            # get aggregate throughput for each ncseq experiment (also for each io size)
            for ncseq in range(1, num_clients + 1):
                col = [
                    float(total)
                    for total in _res_sums[op_idx][cfg_idx][
                        list(
                            ["avg"]
                            if ncseq == 1
                            else [
                                "avg_ncseq%d_c%d" % (ncseq, c)
                                for c in range(1, ncseq + 1)
                            ]
                        )
                    ]
                    .sum(axis=1)
                    .values
                ]
                _res_sums_per_ncseq[op_idx][cfg_idx].insert(
                    0,
                    "total_ncseq%d" % ncseq,
                    col,
                )

            _res_sums_per_ncseq[op_idx][cfg_idx] = _res_sums_per_ncseq[op_idx][
                cfg_idx
            ].set_index(_res_sums[op_idx][cfg_idx].index)

            if num_clients == 1:
                # pull io sizes from _res_sums
                _res_sums_per_ncseq[op_idx][cfg_idx].insert(
                    0,
                    "iosz",
                    [
                        int(int(iosz) * 1.0 / 1e3)
                        for iosz in list(_res_sums[op_idx][cfg_idx].index)
                    ],
                )
            else:
                # pull io sizes from _res_sums
                _res_sums_per_ncseq[op_idx][cfg_idx].insert(
                    0,
                    "iosz",
                    [
                        str(int(int(iosz) * 1.0 / 1e3)) + "KB"
                        for iosz in list(_res_sums[op_idx][cfg_idx].index)
                    ],
                )

                _res_sums_per_ncseq[op_idx][cfg_idx] = _res_sums_per_ncseq[op_idx][
                    cfg_idx
                ].set_index("iosz")

                # for multi-client, transpose and sort, then insert our cols to pivot on
                _res_sums_per_ncseq[op_idx][cfg_idx] = _res_sums_per_ncseq[op_idx][
                    cfg_idx
                ].T.sort_index()

                _res_sums_per_ncseq[op_idx][cfg_idx].insert(
                    0,
                    "ncseq",
                    [ncseq for ncseq in range(1, num_clients + 1)],
                )

            _res_sums_per_ncseq[op_idx][cfg_idx].insert(
                0,
                "fs_type",
                [new_cfgs[cfg_idx] for _ in _res_sums_per_ncseq[op_idx][cfg_idx].index],
            )

    # make a frame (from client0) for plotting sums with
    # _res_sums = {
    #     op_idx: [_res[0][op_idx][cfg_idx].copy() for cfg_idx in range(len(cfgs))]
    #     for op_idx in range(len(ops))
    # }
    # _res_sums = {op_idx: [None] * len(cfgs) for op_idx in range(len(ops))}
    # for op_idx in range(len(ops)):
    #     for cfg_idx in range(len(cfgs)):
    #         _res_sums = sum(
    #             [_res for c_idx in range(num_clients)]
    #         )

    # plot tp
    _fontsize = 12
    # plt.style.use("seaborn-deep")
    plt.style.use("bmh")
    fs_type_markers = ["s", "^", "d", "o", "P", "*"]
    # fs_type_colors = ["green", "black"]
    # fs_type_styles = ["--", "-"]
    plt.figure()
    _nrows = 1 if (num_clients > 1) else 2
    _ncols = 2
    _figsize = (4.5, 3) if (num_clients == 1) else (5, 2)
    fig, axes = plt.subplots(
        nrows=_nrows, ncols=_ncols, sharex=True, sharey=True, figsize=_figsize
    )
    fig.tight_layout()
    fig.subplots_adjust(wspace=0.25, hspace=0.5)

    if num_clients == 1:
        fig.text(0.55, -0.02, "I/O size (KB)", ha="center", fontsize=_fontsize)
    else:
        fig.text(
            0.5, -0.00, "Number of concurrent clients", ha="center", fontsize=_fontsize
        )
    fig.text(-0.03, 0.55, "MB/s", va="center", rotation="vertical", fontsize=_fontsize)
    ax_set = False
    _ax = None
    _xlim = 0
    _ylim = 0
    p1 = None
    p2 = None
    p3 = None
    for op_idx in range(len(ops)):
        _ax = axes[op_idx // 2, op_idx % 2] if (num_clients == 1) else axes[op_idx % 2]
        for cfg_idx in range(len(new_cfgs)):
            if num_clients == 1:
                # plot single-client TP for various IO sizes
                p1 = ["iosz"]
                p2 = ["fs_type"]
                p3 = ["total_ncseq1"]
                _xlim = (
                    0,
                    50 * (int(max(_res_sums_per_ncseq[op_idx][cfg_idx].iosz) / 50) + 1),
                )
            else:
                # plot multi-client TP for various # clients (at various IO sizes)
                p1 = ["ncseq"]
                p2 = ["fs_type"]
                # p3 = (
                #     ["4062", "32496", "129984"]
                #     if (cfgs[cfg_idx].find("bfs") != -1)
                #     else ["4096", "32768", "131072"]
                # )
                # p3 = (
                #     ["4KB", "129KB"]
                #     if (new_cfgs[cfg_idx].find("bfs") != -1)
                #     else ["4KB", "131KB"]
                # )
                p3 = ["4KB", "131KB"]

                # p3 = (
                #     ["4062"]
                #     if (cfgs[cfg_idx].find("bfs") != -1)
                #     else ["4096"]
                # )
                _xlim = (0.5, num_clients + 0.5)

            # set ylim as max across all fs_types
            t = 50 * (
                int(max(_res_sums_per_ncseq[op_idx][cfg_idx][p3].values.flatten()) / 50)
            )
            _ylim = max(_ylim, t)

            # _res.pivot("iosz", "fs_type", "total").plot(
            for isz_idx in range(len(p3)):
                if num_clients == 1:
                    _res_sums_per_ncseq[op_idx][cfg_idx].pivot(
                        p1, p2, p3[isz_idx]
                    ).plot(
                        ax=_ax,
                        # color=fs_type_colors[cfg_idx // 2],
                        # color="black",
                        # linestyle=fs_type_styles[cfg_idx % 2],
                        linewidth=0.5,
                        # markerfacecolor="black"
                        # if (new_cfgs[cfg_idx] == "bfs")
                        # else None,
                        markeredgecolor="black",
                        markersize=6,
                        markeredgewidth=0.5,
                        marker=fs_type_markers[cfg_idx // 2],
                        ylim=(0, 100),
                        # xlim=(0, max(_res_sums_per_ncseq[op_idx][cfg_idx].iosz)),
                        xlim=_xlim,
                        legend=False,
                    )
                else:
                    _res_sums_per_ncseq[op_idx][cfg_idx].pivot(
                        p1, p2, p3[isz_idx]
                    ).set_axis(
                        ["%s, %s" % (new_cfgs[cfg_idx], p3[isz_idx])], axis=1
                    ).plot(
                        ax=_ax,
                        # color=fs_type_colors[cfg_idx // 2],
                        # color="black",
                        # linestyle=fs_type_styles[cfg_idx % 2],
                        linewidth=0.5,
                        # markerfacecolor="black"
                        # if (new_cfgs[cfg_idx] == "bfs")
                        # else None,
                        markeredgecolor="black",
                        markersize=6,
                        markeredgewidth=0.5,
                        marker=fs_type_markers[cfg_idx],
                        ylim=(0, 150),
                        # xlim=(0, max(_res_sums_per_ncseq[op_idx][cfg_idx].iosz)),
                        xlim=_xlim,
                        legend=False,
                    )
        _ax.set_title(ops[op_idx], fontsize=_fontsize, y=1.05)
        _ax.tick_params(labelsize=_fontsize)
        # _ax.spines["top"].set_visible(False)
        # _ax.spines["right"].set_visible(False)
        _ax.axes.get_xaxis().set_label_text("")
        # _ax.grid(color='black', linestyle='--', linewidth=0.1, alpha=0.25)
        _ax.grid(color="black", axis="y", linestyle="--", linewidth=0.5, alpha=0.5)
        _ax.set_facecolor("white")

    if num_clients == 1:
        _ax.legend(
            framealpha=0.0,
            # edgecolor=None,
            # facecolor="black",
            bbox_to_anchor=(-1.5, 2.8, 0, 0),
            loc=(0.0, 0.0),
            ncol=3,
            markerscale=1.0,
            fontsize=_fontsize,
        )
    else:
        _ax.legend(
            framealpha=0.0,
            # edgecolor=None,
            # facecolor="black",
            bbox_to_anchor=(-1.7, 1.25, 0, 0),
            loc=(0.0, 0.0),
            ncol=3,
            markerscale=1.0,
            fontsize=_fontsize,
        )
    plt.savefig(
        os.getenv("BFS_HOME")
        + (
            "/benchmarks/micro/output--%s/micro_tp_res_multi.pdf" % _dev_type
            if num_clients > 1
            else "/benchmarks/micro/output--%s/micro_tp_res.pdf" % _dev_type
        ),
        bbox_inches="tight",
    )


if __name__ == "__main__":
    print("Plotting microbenchmark results:")
    if len(sys.argv) < 4:
        print("Invalid number of args")
        exit(-1)
    get_tp(
        sys.argv[1],
        sys.argv[2],
        sys.argv[3],
        sys.argv[4] if len(sys.argv) > 4 else None,
    )
    print("Done")
