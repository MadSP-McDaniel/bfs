import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import os
import seaborn as sns
import sys
import glob
import json
from scipy import optimize
from scipy.special import zeta
from scipy.stats import entropy

from plot_support import *

matplotlib.rcParams["pdf.fonttype"] = 42
matplotlib.rcParams["ps.fonttype"] = 42


# def plot_bwlog_ecdf(logs):
#     print("Plotting ecdf for", logs)

#     p = os.getenv("BFS_HOME") + "/benchmarks/o/"
#     plt.style.use("ggplot")
#     plt.figure(figsize=(10, 6))

#     for l in logs:
#         exp_type = l.split("/")[1]

#         # Parse the fio bandwidth log file and plot an ecdf of the bandwidths.
#         # The columns in the bandwidth log file are: [time (msec), bandwidth (KB/s), R/W, 0]
#         df = pd.read_csv(
#             p + l,
#             header=None,
#             names=["time", "bandwidth", "rw", "zero"],
#         ).drop(columns=["zero"])

#         df_reads = df[df["rw"] == 0] / 1024
#         df_writes = df[df["rw"] == 1] / 1024

#         plt.plot(
#             np.sort(df_reads["bandwidth"]),
#             np.linspace(0, 1, len(df_reads), endpoint=False),
#             label=exp_type + "-reads",
#         )
#         plt.plot(
#             np.sort(df_writes["bandwidth"]),
#             np.linspace(0, 1, len(df_writes), endpoint=False),
#             label=exp_type + "-writes",
#         )
#     plt.xlabel("Throughput (MB/s)", fontsize=15)
#     plt.ylabel("ECDF", fontsize=15)
#     plt.legend(loc="lower right", fontsize=13, ncol=2)
#     plt.grid(visible=True, alpha=0.5)
#     plt.yticks([0, 0.25, 0.5, 0.75, 1], fontsize=15)
#     for item in plt.gca().get_xticklabels():
#         item.set_fontsize(15)
#     for item in plt.gca().get_yticklabels():
#         item.set_fontsize(15)
#     plt.savefig(p + "tp_ecdf.pdf", bbox_inches="tight")


# def plot_recorded_trace_details(trace=None):
#     print("Plotting trace details for", trace)

#     p = os.getenv("BFS_HOME") + "/benchmarks/o/"
#     plt.style.use("ggplot")

#     sub = True  # separate plots or single
#     fig, axes = plt.subplots(nrows=2, ncols=2, figsize=(15, 6))
#     plt.subplots_adjust(
#         left=None, bottom=None, right=None, top=None, wspace=0.3, hspace=0.5
#     )

#     t = trace
#     res = pd.read_csv(p + t + ".csv", header=None, names=["time", "blk_id"])

#     # res = res.apply(lambda x: int(x.str[1:]), 1)
#     # for i in range(0, len(res.index)):
#     #     res.iloc[i] = res.iloc[i].str[1:]
#     # res = res.astype(int)

#     # plot time series
#     ax = res["blk_id"].plot(
#         ax=axes[0][0],
#         kind="line",
#         subplots=sub,
#         title=t,
#         # xlabel="time",
#         # ylabel="block addr",
#         style="x",
#     )
#     print("done plotting time series")
#     a = ax[0] if sub else ax
#     a.set_xlabel("time (x1e3)", fontsize=15)
#     a.set_xticklabels([str(float(x) / 1e3)[:5] for x in a.get_xticks()])
#     a.set_ylabel("block addr (x1e3)", fontsize=15)
#     a.set_yticklabels([str(float(y) / 1e3)[:5] for y in a.get_yticks()])
#     for item in a.get_xticklabels():
#         item.set_fontsize(15)
#     for item in a.get_yticklabels():
#         item.set_fontsize(15)

#     # plot histogram

#     resh = (
#         pd.Series(res["blk_id"][0])
#         .value_counts(normalize=True, sort=True, ascending=False, bins=100)
#         .reset_index()
#     )

#     ax = resh["proportion"].plot(
#         ax=axes[0][1],
#         # kind="bar",
#         subplots=sub,
#         title=t,
#         xlabel="rank",
#         ylabel="frequency",
#     )
#     print("done plotting histogram")
#     a = ax[0] if sub else ax
#     a.set_xlabel("rank", fontsize=15)
#     # a.set_xticklabels([str(float(x) / 1e3)[:5] for x in a.get_xticks()])
#     a.set_ylabel("frequency", fontsize=15)
#     # a.set_yticklabels([str(float(y) / 1e3)[:5] for y in a.get_yticks()])
#     for item in a.get_xticklabels():
#         item.set_fontsize(15)
#     for item in a.get_yticklabels():
#         item.set_fontsize(15)

#     # plot cdf
#     ax = sns.ecdfplot(
#         data=res["blk_id"],
#         ax=axes[1][0],
#         legend=True,
#     )
#     a = ax
#     a.legend(loc="lower right", fontsize=15, ncol=4)
#     a.legend(res["blk_id"].index, loc="lower right", fontsize=15, ncol=4)
#     a.axhline(y=0.9, color="black", linewidth=1.0, label="90%")
#     a.axvline(x=0.1 * res["blk_id"].max(), color="black", linewidth=1.0, label="90%")
#     a.set_xlabel("block addr (x1e3)", fontsize=15)
#     a.set_ylabel("ECDF", fontsize=15)
#     a.grid(visible=True, alpha=0.5)
#     for item in a.get_yticklabels():
#         item.set_fontsize(15)
#     a.set_xticklabels([str(float(x) / 1e3)[:5] for x in a.get_xticks()])
#     for item in a.get_xticklabels():
#         item.set_fontsize(15)
#     print("done plotting ecdf")

#     # calculate the inter-arrival time (in us) and plot it as a histogram
#     ax = (
#         res["time"]
#         .diff()
#         .abs()
#         .plot(
#             ax=axes[1][1],
#             kind="line",
#             # bins=100,
#             subplots=sub,
#             # title=t,
#             # linewidth=2,
#             # xlabel="time",
#             # ylabel="inter-arrival time (µs)",
#             style="x",
#         )
#     )
#     print("done plotting inter-arrival time")
#     a = ax[0] if sub else ax
#     a.set_xlabel("time (x1e3)", fontsize=15)
#     a.set_ylabel("inter-arrival time (µs)", fontsize=15)
#     a.set_xticklabels([str(float(x) / 1e3)[:5] for x in a.get_xticks()])
#     # a.set_yticklabels([str(float(y) / 1e3)[:5] for y in a.get_yticks()])
#     for item in a.get_xticklabels():
#         item.set_fontsize(15)
#     for item in a.get_yticklabels():
#         item.set_fontsize(15)

#     # # calculate the distance between accesses (in blocks) and plot it as a histogram
#     # ax = (
#     #     res.diff()
#     #     .abs()
#     #     .plot(
#     #         ax=axes[1][1],
#     #         kind="hist",
#     #         bins=100,
#     #         subplots=sub,
#     #         # title=t,
#     #         # linewidth=2,
#     #         xlabel="stride len",
#     #         ylabel="frequency",
#     #         style="x",
#     #     )
#     # )
#     # print("done plotting stride length")
#     # a = ax[0] if sub else ax
#     # a.set_xlabel("stride len (x1e3)", fontsize=15)
#     # a.set_ylabel("frequency (x1e3)", fontsize=15)
#     # a.set_xticklabels([str(float(x) / 1e3)[:5] for x in a.get_xticks()])
#     # a.set_yticklabels([str(float(y) / 1e3)[:5] for y in a.get_yticks()])
#     # for item in a.get_xticklabels():
#     #     item.set_fontsize(15)
#     # for item in a.get_yticklabels():
#     #     item.set_fontsize(15)

#     # a = ax[0] if sub else ax
#     # a.get_figure().savefig(p + t + ".pdf", bbox_inches="tight")

#     # take the block access sequence, and for each block, count the number of accesses until the next access to that block, and compute the average, and plot it as a histogram
#     # max_blk = res.max()
#     # print("max_blk: ", max_blk[0])
#     # res2 = pd.DataFrame(index=range(0, max_blk[0] + 1), columns=["avg_time_to_reuse"])
#     # for blk in range(0, max_blk[0] + 1):
#     #     res2["avg_time_to_reuse"].loc[blk] = (
#     #         res[res == blk].index.to_frame().diff().mean()
#     #     )
#     # res2 = res2.fillna(0)
#     # res2 = res2.astype(float)
#     # ax = res2.plot(
#     #     ax=axes[1][1],
#     #     kind="line",
#     #     subplots=sub,
#     #     # title=t,
#     #     linewidth=2,
#     #     xlabel="block addr",
#     #     ylabel="avg time to reuse",
#     #     style="x",
#     #     legend=False,
#     # )
#     # print("done plotting time to reuse")

#     a = ax[0] if sub else ax
#     a.get_figure().suptitle(trace, fontsize=15, y=1.0)
#     a.get_figure().savefig(p + t + ".png", bbox_inches="tight")


# def zipfian_likelihood(s, data):
#     """Calculate the negative log likelihood for the Zipfian distribution."""
#     ranks = data.index.to_frame()
#     frequencies = data["offset"]
#     # Calculate the likelihood of the Zipfian distribution for the given s
#     likelihood = -np.sum(np.log((ranks ** (-s)) / zeta(s))) * frequencies
#     return likelihood


# def plot_iolog_details():
#     cap = 2**30
#     stem = "workloads/traces/"
#     p = "scaled_iolog_alibaba_loop_0_" + str(cap)
#     # p = "iolog_" + str(cap) + "_zipf:2.5_randrw1_4k"

#     # Parse the fio iolog file and plot an ecdf of the offsets (in blocks).
#     # The columns in the iolog file are: [vol_id op offset size]
#     # Skip header lines containing "fio version 2 iolog", "add", "open", or "close"
#     df = pd.read_csv(
#         stem + p,
#         header=None,
#         names=["vol_id", "op", "offset", "size"],
#         skiprows=[0, 1, 2],
#         skipfooter=1,
#         delim_whitespace=True,
#     )
#     df = df.reset_index()
#     df.rename(columns={"index": "time"}, inplace=True)

#     df["time"] = df["time"].astype(float)
#     df["offset"] = df["offset"].astype(float)
#     df["size"] = df["size"].astype(float)

#     df["offset"] = df["offset"] // 4096
#     df["size"] = df["size"] // 4096

#     plt.style.use("ggplot")
#     plt.figure(figsize=(10, 6))
#     plt.plot(df["offset"][:1000], label="offset")
#     plt.savefig("iolog_time_series.png", bbox_inches="tight")

#     # plot a histogram of the inter-arrival times by taking the difference between consecutive timestamps
#     df_copy = df.copy()
#     df_copy["time"] = df_copy["time"].diff()
#     df_copy = df_copy.dropna()
#     df_copy = df_copy[df_copy["time"] > 0]
#     df_copy = df_copy[df_copy["time"] < 1e6]
#     print(
#         "Percent inter-arrival times < 100us: %.3f%%"
#         % (len(df_copy[df_copy["time"] < 100]) / len(df_copy) * 100.0)
#     )
#     plt.style.use("ggplot")
#     plt.figure(figsize=(10, 6))
#     plt.hist(df_copy["time"], bins=100)
#     plt.savefig("iolog_time_hist.png", bbox_inches="tight")

#     # We want to compute the skewness of the block offsets. We will use the skewness
#     # to determine how much potential there is for the splay tree approach to capture
#     # the temporal locality of the block accesses.
#     s = df["offset"].skew()
#     print("overall skewness: ", s)
#     s = list()
#     n = 1000
#     for i in range(0, n):
#         s.append(df[i * len(df) // n : (i + 1) * len(df) // n]["offset"].skew())
#     print("skewness of bins: ", np.sort(s)[:100])

#     # Now we want to create a new frame that flattens out the offset and size columns into multiple rows
#     # The offset and size are both in terms of blocks.
#     # For each row in the original frame, we will create size rows in the new frame, where each new row will have the first offset incremented by 4096 each time.
#     # This will allow us to plot the ECDF of the block offsets.
#     # df_new = pd.DataFrame(columns=["op", "offset"])
#     # for i in range(0, len(df)):
#     #     df_new = pd.concat(
#     #         [
#     #             df_new,
#     #             pd.DataFrame(
#     #                 {
#     #                     "time": [df.iloc[i]["time"]] * df.iloc[i]["size"],
#     #                     "vol_id": [df.iloc[i]["vol_id"]] * df.iloc[i]["size"],
#     #                     "op": [df.iloc[i]["op"]] * df.iloc[i]["size"],
#     #                     "offset": np.arange(
#     #                         df.iloc[i]["offset"],
#     #                         df.iloc[i]["offset"] + df.iloc[i]["size"],
#     #                     ),
#     #                     "size": [df.iloc[i]["size"]] * df.iloc[i]["size"],
#     #                 }
#     #             ),
#     #         ]
#     #     )

#     # df = df_new

#     df_reads = df[df["op"] == "read"]
#     df_writes = df[df["op"] == "write"]

#     print("percentage of reads: ", len(df_reads) / len(df))
#     print("percentage of writes: ", len(df_writes) / len(df))

#     # Compute the frequencies of bins (ranks) of the block offsets and sort them.
#     df = (
#         df["offset"]
#         .value_counts(normalize=True, sort=True, ascending=False, bins=100)
#         .reset_index()
#     )
#     df_writes = (
#         df_writes["offset"]
#         .value_counts(normalize=True, sort=True, ascending=False, bins=100)
#         .reset_index()
#     )
#     if len(df_reads) != 0:
#         df_reads = (
#             df_reads["offset"]
#             .value_counts(normalize=True, sort=True, ascending=False, bins=100)
#             .reset_index()
#         )

#     # Now print the entropy of the block offsets
#     H = entropy(df["proportion"], base=2)
#     print("entropy: ", H)

#     # Now we want to basically fit the distribution of offsets to a zipf distribution and plot the ecdf of the zipf distribution. We can do this by performing a maximum likelihood estimation of the zipf distribution parameters.
#     # Initial guess for the s parameter
#     # initial_s = 1.5
#     # result = optimize.minimize_scalar(
#     #     zipfian_likelihood,
#     #     args=(df,),
#     #     bounds=(1, 10),
#     #     method="bounded",
#     #     options={"xatol": 1e-8},
#     # )
#     # if result.success:
#     #     fitted_s = result.x
#     #     print(f"Estimated Zipfian parameter s: {fitted_s}")
#     # else:
#     #     print(
#     #         "Optimization was not successful. Try different initial guesses or bounds."
#     #     )

#     # The index is made up of half-open intervals. Now we want to take each interval and
#     # compute the % of the addr space that it represents.
#     # We will then compute the cumulative sum of these %s and use them as the x-axis.
#     if len(df_reads) != 0:
#         df_reads["index"] = (
#             df_reads["index"]
#             .apply(lambda x: (x.right - x.left) / (cap // 4096))
#             .cumsum()
#             * 100.0
#         )
#     df_writes["index"] = (
#         df_writes["index"].apply(lambda x: (x.right - x.left) / (cap // 4096)).cumsum()
#         * 100.0
#     )
#     df["index"] = (
#         df["index"].apply(lambda x: (x.right - x.left) / (cap // 4096)).cumsum() * 100.0
#     )

#     if len(df_reads) != 0:
#         df_reads["proportion"] = df_reads["proportion"].cumsum() * 100.0
#     df_writes["proportion"] = df_writes["proportion"].cumsum() * 100.0
#     df["proportion"] = df["proportion"].cumsum() * 100.0

#     if len(df_reads) != 0:
#         df_reads.set_index("index", inplace=True)
#     df_writes.set_index("index", inplace=True)
#     df.set_index("index", inplace=True)

#     # Compute the cumulative sums of the bin frequencies and plot them. The x-axis
#     # represents the rank of the bins (in increments of x% of the block addr space).
#     # The y-axis represents the offset of accesses that fall within some x% of
#     # the block addr space.
#     plt.style.use("ggplot")
#     plt.figure(figsize=(10, 3))
#     # if len(df_reads) != 0:
#     #     plt.plot(df_reads, label="read")
#     # plt.plot(df_writes, label="write")
#     plt.plot(df, label="any", linewidth=4.0)

#     plt.xlabel("% of addr space", fontsize=15)
#     plt.ylabel("% of accesses", fontsize=15)
#     # plt.legend(loc="lower right", fontsize=13, ncol=2)
#     plt.grid(visible=True, alpha=0.5)
#     plt.yticks([0, 25, 50, 75, 100], fontsize=15)
#     # plt.xticks([0, 0.01, 0.015, 0.02], fontsize=15)
#     line_at = 5.0
#     plt.axvline(x=line_at, color="black", linewidth=1.0, label=str(line_at))
#     plt.text(
#         line_at + 1.0,
#         5.0,
#         str(df[df.index > line_at].iloc[0]["proportion"])[:5]
#         + "% of accesses\nto "
#         + str(line_at)
#         + "% of blocks",
#         fontsize=15,
#         color="black",
#     )
#     for item in plt.gca().get_xticklabels():
#         item.set_fontsize(15)
#     for item in plt.gca().get_yticklabels():
#         item.set_fontsize(15)
#     # plt.title(stem + p, fontsize=15)
#     plt.savefig(p + "_accesses_vs_addr.pdf", bbox_inches="tight")

#     # # Plot a zipf curve alongside the trace
#     # a = 1.1
#     # n = 53687091200
#     # ent = int(1e7)
#     # s = np.random.zipf(a, ent)
#     # s = s[s < n]
#     # while len(s[s < n]) < ent:
#     #     print("Not enough values in [0, n): %d/%d" % (len(s[s < n]), ent))
#     #     k = np.random.zipf(a, ent - len(s[s < n]))
#     #     k -= 1  # we want the values to be in [0, n)
#     #     s = np.append(s, k[k < n])
#     # f = pd.DataFrame(s)
#     # f = f // 4096
#     # f = (
#     #     f[0]
#     #     .value_counts(normalize=True, sort=True, ascending=False, bins=100)
#     #     .reset_index()
#     # )
#     # f["index"] = f["index"].apply(lambda x: (x.right - x.left) / (n // 4096)).cumsum()
#     # f = f.set_axis(["index", "offset"], axis=1)
#     # f["offset"] = f["offset"].cumsum()

#     # plt.style.use("ggplot")
#     # plt.figure(figsize=(10, 3))
#     # # if len(df_reads) != 0:
#     # #     plt.plot(df_reads, label="read")
#     # # plt.plot(df_writes, label="write")
#     # plt.plot(f, label="zipf", linewidth=4.0)

#     # plt.xlabel("% of addr space", fontsize=15)
#     # plt.ylabel("% of accesses", fontsize=15)
#     # # plt.legend(loc="lower right", fontsize=13, ncol=2)
#     # plt.grid(visible=True, alpha=0.5)
#     # # plt.yticks([0, 0.25, 0.5, 0.75, 1], fontsize=15)
#     # # plt.xticks([0, 0.01, 0.015, 0.02], fontsize=15)
#     # line_at = 0.01
#     # plt.axvline(x=line_at, color="black", linewidth=1.0, label=str(line_at))
#     # plt.text(
#     #     line_at + 0.01,
#     #     0.1,
#     #     str(f[f.index > line_at].iloc[0]["offset"] * 100.0)[:5]
#     #     + "% of accesses\nto "
#     #     + str(line_at * 100.0)
#     #     + "% of blocks",
#     #     fontsize=15,
#     #     color="black",
#     # )
#     # for item in plt.gca().get_xticklabels():
#     #     item.set_fontsize(15)
#     # for item in plt.gca().get_yticklabels():
#     #     item.set_fontsize(15)
#     # plt.title(p, fontsize=15)
#     # plt.savefig("iolog_time_dist_zipf.png", bbox_inches="tight")


def set_patches(df, groups=6):
    # return
    for i, bar in enumerate(df.patches):
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
            bar.set_hatch("x")


def plot_micro():
    p = os.getenv("BFS_HOME") + "/benchmarks/new_bench/o/filebench-directio/"
    plt.style.use("bmh")
    fs_type_markers = ["s", "^", "d", "o", "P", "*"]
    fs_types = ["bfs_ne", "bfs_ci", "nfs_ne", "nfs_we"]

    # We want to plot throughput vs I/O size for different workloads and different fs types (bfs, bfs_ne, nfs_ne, nfs_we)
    # The results are stored in filebench log files in p/ of the form: workload_numthreads_iosize_fstype_threadid.log
    # We want to grep the filebench log files for the throughput and plot it against the I/O size.
    res_json = glob.glob(p + "*.log")
    res = {
        workload: {
            fs_type: {
                iosize: 0.0 for iosize in ["4K", "8K", "16K", "32K", "64K", "128K"]
            }
            for fs_type in fs_types
        }
        for workload in ["seqread", "rread", "seqwrite", "rwrite"]
    }

    res_open_lat = {
        workload: {
            fs_type: {
                iosize: 0.0 for iosize in ["4K", "8K", "16K", "32K", "64K", "128K"]
            }
            for fs_type in fs_types
        }
        for workload in ["seqread", "rread", "seqwrite", "rwrite"]
    }

    res_close_lat = {
        workload: {
            fs_type: {
                iosize: 0.0 for iosize in ["4K", "8K", "16K", "32K", "64K", "128K"]
            }
            for fs_type in fs_types
        }
        for workload in ["seqread", "rread", "seqwrite", "rwrite"]
    }

    res_fsync_lat = {
        workload: {
            fs_type: {
                iosize: 0.0 for iosize in ["4K", "8K", "16K", "32K", "64K", "128K"]
            }
            for fs_type in fs_types
        }
        for workload in ["seqread", "rread", "seqwrite", "rwrite"]
    }

    for r in res_json:
        if "bench" in r.split("/")[-1]:
            continue
        if "nfsg" in r:
            continue
        with open(r) as f:
            fs_type = None
            for t in fs_types:
                if t in f.name:
                    fs_type = t
                    break
            workload = f.name.split("/")[-1].split("_")[0]
            if fs_type == "nexus":
                iosize = f.name.split("/")[-1].split("_")[-3]
            else:
                iosize = f.name.split("/")[-1].split("_")[-4]

            if int(iosize) // 1024 > 1024:
                iosize = str(int(iosize) // 1024 // 1024) + "M"
            else:
                iosize = str(int(iosize) // 1024) + "K"

            lines = f.readlines()

            # Get the throghput by searching the second to last line
            # tokens = lines[-2].split(" ")
            # tokens = [t for t in tokens if t]
            # throughput = float(tokens[9][:-4])

            # Get the raw r/w throughput by searching for the line with "<workload>OP"
            tokens = None
            found = False
            for l in lines:
                tokens = l.split(" ")
                if (
                    "rreadOP" in tokens
                    or "rwriteOP" in tokens
                    or "seqreadOP" in tokens
                    or "seqwriteOP" in tokens
                    or "readOP" in tokens
                    or "writeOP" in tokens
                ):
                    found = True
                    break
                # if (
                #     "rreadOP" in tokens
                #     or "rwriteOP" in tokens
                #     or "seqreadOP" in tokens
                #     or "seqwriteOP" in tokens
                # ):
                #     break
                # if "readOP" in tokens or "writeOP" in tokens:
                #     break
            if not found:
                continue
            tokens = [t for t in tokens if t]
            # lat = float(tokens[4][:-5]) / 1e3
            # throughput = float(iosize) / lat
            throughput = float(tokens[3][:-4])

            res[workload][fs_type][iosize] = throughput

            # now get open lat
            tokens = None
            found = False
            for l in lines:
                tokens = l.split(" ")
                if ("openOP" in tokens) or ("createOP" in tokens):
                    found = True
                    break
            if not found:
                continue
            tokens = [t for t in tokens if t]
            open_lat = float(tokens[4][:-5])
            res_open_lat[workload][fs_type][iosize] = open_lat

            # now get close lat
            tokens = None
            found = False
            for l in lines:
                tokens = l.split(" ")
                if "closeOP" in tokens:
                    found = True
                    break
            if not found:
                continue
            tokens = [t for t in tokens if t]
            close_lat = float(tokens[4][:-5])
            res_close_lat[workload][fs_type][iosize] = close_lat

            # now get fsync lat
            tokens = None
            found = False
            for l in lines:
                tokens = l.split(" ")
                if "fsyncOP" in tokens:
                    found = True
                    break
            if not found:
                continue
            tokens = [t for t in tokens if t]
            fsync_lat = float(tokens[4][:-5])
            res_fsync_lat[workload][fs_type][iosize] = fsync_lat

    # create df from nested dict
    df = pd.DataFrame.from_dict(
        {(i, j): res[i][j] for i in res.keys() for j in res[i].keys()}, orient="index"
    )

    df_open_lat = pd.DataFrame.from_dict(
        {
            (i, j): res_open_lat[i][j]
            for i in res_open_lat.keys()
            for j in res_open_lat[i].keys()
        },
        orient="index",
    )

    df_close_lat = pd.DataFrame.from_dict(
        {
            (i, j): res_close_lat[i][j]
            for i in res_close_lat.keys()
            for j in res_close_lat[i].keys()
        },
        orient="index",
    )

    df_fsync_lat = pd.DataFrame.from_dict(
        {
            (i, j): res_fsync_lat[i][j]
            for i in res_fsync_lat.keys()
            for j in res_fsync_lat[i].keys()
        },
        orient="index",
    )

    # # for each fs type, for each workload, plot the throughput vs I/O size
    # fig, axes = plt.subplots(nrows=2, ncols=2, figsize=(10, 6))
    # plt.subplots_adjust(
    #     left=None, bottom=None, right=None, top=None, wspace=0.5, hspace=0.5
    # )
    # for i, fs_type in enumerate(res.keys()):
    #     for j, workload in enumerate(res[fs_type].keys()):
    #         x = list(res[fs_type][workload].keys())
    #         x = [float(i) / 1024 for i in x]
    #         y = list(res[fs_type][workload].values())
    #         axes[j // 2][j % 2].plot(
    #             x,
    #             y,
    #             marker=fs_type_markers[i // (len(res.keys()) // 2)],
    #             # label=fs_type + "-" + workload,
    #             label="bfs" if fs_type == "bfs_ci" else fs_type,
    #             markeredgecolor="black",
    #             markersize=6,
    #             markeredgewidth=0.5,
    #             linewidth=0.5,
    #         )
    #         axes[j // 2][j % 2].set_facecolor("white")

    #         axes[j // 2][j % 2].set_title(workload, fontsize=15)
    #         axes[j // 2][j % 2].set_xlabel("I/O size (KB)", fontsize=15)
    #         axes[j // 2][j % 2].set_ylabel("Throughput (MB/s)", fontsize=15)
    #         axes[j // 2][j % 2].set_xticks(np.arange(0, 151, 50))
    #         axes[j // 2][j % 2].grid(visible=True, alpha=0.5)
    #         # axes[j // 2][j % 2].set_ylim(0, 2000)
    #         for item in axes[j // 2][j % 2].get_xticklabels():
    #             item.set_fontsize(15)
    #         for item in axes[j // 2][j % 2].get_yticklabels():
    #             item.set_fontsize(15)
    #         # axes[j // 2][j % 2].legend(loc="lower right", fontsize=13, ncol=2)
    # axes[0][0].legend(
    #     fontsize=13,
    #     ncol=6,
    #     bbox_to_anchor=(1.85, -1.75),
    #     facecolor="white",
    #     edgecolor="white",
    # )
    # # set legend background color to white and remove border
    # # axes[0][0].legend().get_frame().set_facecolor("white")
    # # axes[0][0].legend().get_frame().set_linewidth(0.0)
    # plt.savefig(p + "tp_vs_io_size.pdf", bbox_inches="tight")

    # plot the throughput vs I/O size for each workload as a bar plot
    fig, axes = plt.subplots(nrows=2, ncols=2, figsize=(10, 6))
    plt.subplots_adjust(
        left=None, bottom=None, right=None, top=None, wspace=0.3, hspace=0.4
    )
    barWidth = 0.75

    df_concat = pd.concat(
        [df.T[workload] for workload in res.keys()], axis=1, keys=res.keys()
    )

    d = df_concat["seqread"].plot(
        kind="bar",
        ax=axes[0][0],
        title="seqread",
        xlabel="I/O size",
        ylabel="Throughput (MB/s)",
        legend=True,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    axes[0][0].legend(
        handles,
        labels,
        fontsize=12,
        ncol=6,
        bbox_to_anchor=(1.85, 1.4),
        edgecolor="white",
    )
    axes[0][0].grid(visible=True, alpha=0.5)

    d = df_concat["rread"].plot(
        kind="bar",
        ax=axes[0][1],
        title="rread",
        xlabel="I/O size",
        ylabel="Throughput (MB/s)",
        legend=False,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[0][1].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][1].get_yticklabels():
    #     item.set_fontsize(12)
    axes[0][1].grid(visible=True, alpha=0.5)

    d = df_concat["seqwrite"].plot(
        kind="bar",
        ax=axes[1][0],
        title="seqwrite",
        xlabel="I/O size",
        ylabel="Throughput (MB/s)",
        legend=False,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[1][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[1][0].get_yticklabels():
    #     item.set_fontsize(12)
    axes[1][0].grid(visible=True, alpha=0.5)

    d = df_concat["rwrite"].plot(
        kind="bar",
        ax=axes[1][1],
        title="rwrite",
        xlabel="I/O size",
        ylabel="Throughput (MB/s)",
        legend=False,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[1][1].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[1][1].get_yticklabels():
    #     item.set_fontsize(12)
    axes[1][1].grid(visible=True, alpha=0.5)

    plt.savefig(p + "tp_vs_io_size.pdf", bbox_inches="tight")

    # plot open lats
    plt.figure(figsize=(10, 6))
    df_open_lat_f = pd.concat(
        [df_open_lat.T[workload] for workload in res.keys()],
        axis=1,
        keys=res.keys(),
    )
    print("mean open lat (seqread):\n", df_open_lat_f["seqread"].mean())
    d = df_open_lat_f["seqread"].plot(
        kind="bar",
        title="seqread",
        xlabel="I/O size",
        ylabel="Open Latency (µs)",
        legend=True,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    axes[0][0].legend(
        handles,
        labels,
        fontsize=12,
        ncol=6,
        bbox_to_anchor=(1.85, 1.4),
        edgecolor="white",
    )
    axes[0][0].grid(visible=True, alpha=0.5)
    plt.savefig(p + "open_lat_vs_io_size_seqread.pdf", bbox_inches="tight")

    plt.figure(figsize=(10, 6))
    df_open_lat_f = pd.concat(
        [df_open_lat.T[workload] for workload in res.keys()],
        axis=1,
        keys=res.keys(),
    )
    print("mean open lat (rread): \n", df_open_lat_f["rread"].mean())
    d = df_open_lat_f["rread"].plot(
        kind="bar",
        title="rread",
        xlabel="I/O size",
        ylabel="Open Latency (µs)",
        legend=True,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    axes[0][0].legend(
        handles,
        labels,
        fontsize=12,
        ncol=6,
        bbox_to_anchor=(1.85, 1.4),
        edgecolor="white",
    )
    axes[0][0].grid(visible=True, alpha=0.5)
    plt.savefig(p + "open_lat_vs_io_size_rread.pdf", bbox_inches="tight")

    plt.figure(figsize=(10, 6))
    df_open_lat_f = pd.concat(
        [df_open_lat.T[workload] for workload in res.keys()],
        axis=1,
        keys=res.keys(),
    )
    print("mean open lat (seqwrite): \n", df_open_lat_f["seqwrite"].mean())
    d = df_open_lat_f["seqwrite"].plot(
        kind="bar",
        title="seqwrite",
        xlabel="I/O size",
        ylabel="Open Latency (µs)",
        legend=True,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    axes[0][0].legend(
        handles,
        labels,
        fontsize=12,
        ncol=6,
        bbox_to_anchor=(1.85, 1.4),
        edgecolor="white",
    )
    axes[0][0].grid(visible=True, alpha=0.5)
    plt.savefig(p + "open_lat_vs_io_size_seqwrite.pdf", bbox_inches="tight")

    plt.figure(figsize=(10, 6))
    df_open_lat_f = pd.concat(
        [df_open_lat.T[workload] for workload in res.keys()],
        axis=1,
        keys=res.keys(),
    )
    print("mean open lat (rwrite): \n", df_open_lat_f["rwrite"].mean())
    d = df_open_lat_f["rwrite"].plot(
        kind="bar",
        title="rwrite",
        xlabel="I/O size",
        ylabel="Open Latency (µs)",
        legend=True,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    axes[0][0].legend(
        handles,
        labels,
        fontsize=12,
        ncol=6,
        bbox_to_anchor=(1.85, 1.4),
        edgecolor="white",
    )
    axes[0][0].grid(visible=True, alpha=0.5)
    plt.savefig(p + "open_lat_vs_io_size_rwrite.pdf", bbox_inches="tight")

    # plot close lats
    print("=====")
    plt.figure(figsize=(10, 6))
    df_close_lat_f = pd.concat(
        [df_close_lat.T[workload] for workload in res.keys()],
        axis=1,
        keys=res.keys(),
    )
    print("mean close lat (seqread):\n", df_close_lat_f["seqread"].mean())
    d = df_close_lat_f["seqread"].plot(
        kind="bar",
        title="seqread",
        xlabel="I/O size",
        ylabel="close latency (µs)",
        legend=True,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    axes[0][0].legend(
        handles,
        labels,
        fontsize=12,
        ncol=6,
        bbox_to_anchor=(1.85, 1.4),
        edgecolor="white",
    )
    axes[0][0].grid(visible=True, alpha=0.5)
    plt.savefig(p + "close_lat_vs_io_size_seqread.pdf", bbox_inches="tight")

    plt.figure(figsize=(10, 6))
    df_close_lat_f = pd.concat(
        [df_close_lat.T[workload] for workload in res.keys()],
        axis=1,
        keys=res.keys(),
    )
    print("mean close lat (rread): \n", df_close_lat_f["rread"].mean())
    d = df_close_lat_f["rread"].plot(
        kind="bar",
        title="rread",
        xlabel="I/O size",
        ylabel="close latency (µs)",
        legend=True,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    axes[0][0].legend(
        handles,
        labels,
        fontsize=12,
        ncol=6,
        bbox_to_anchor=(1.85, 1.4),
        edgecolor="white",
    )
    axes[0][0].grid(visible=True, alpha=0.5)
    plt.savefig(p + "close_lat_vs_io_size_rread.pdf", bbox_inches="tight")

    plt.figure(figsize=(10, 6))
    df_close_lat_f = pd.concat(
        [df_close_lat.T[workload] for workload in res.keys()],
        axis=1,
        keys=res.keys(),
    )
    print("mean close lat (seqwrite): \n", df_close_lat_f["seqwrite"].mean())
    d = df_close_lat_f["seqwrite"].plot(
        kind="bar",
        title="seqwrite",
        xlabel="I/O size",
        ylabel="close latency (µs)",
        legend=True,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    axes[0][0].legend(
        handles,
        labels,
        fontsize=12,
        ncol=6,
        bbox_to_anchor=(1.85, 1.4),
        edgecolor="white",
    )
    axes[0][0].grid(visible=True, alpha=0.5)
    plt.savefig(p + "close_lat_vs_io_size_seqwrite.pdf", bbox_inches="tight")

    plt.figure(figsize=(10, 6))
    df_close_lat_f = pd.concat(
        [df_close_lat.T[workload] for workload in res.keys()],
        axis=1,
        keys=res.keys(),
    )
    print("mean close lat (rwrite): \n", df_close_lat_f["rwrite"].mean())
    d = df_close_lat_f["rwrite"].plot(
        kind="bar",
        title="rwrite",
        xlabel="I/O size",
        ylabel="close latency (µs)",
        legend=True,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    axes[0][0].legend(
        handles,
        labels,
        fontsize=12,
        ncol=6,
        bbox_to_anchor=(1.85, 1.4),
        edgecolor="white",
    )
    axes[0][0].grid(visible=True, alpha=0.5)
    plt.savefig(p + "close_lat_vs_io_size_rwrite.pdf", bbox_inches="tight")

    # plot fsync lats
    print("=====")
    plt.figure(figsize=(10, 6))
    df_fsync_lat_f = pd.concat(
        [df_fsync_lat.T[workload] for workload in res.keys()],
        axis=1,
        keys=res.keys(),
    )
    print("mean fsync lat (seqread):\n", df_fsync_lat_f["seqread"].mean())
    d = df_fsync_lat_f["seqread"].plot(
        kind="bar",
        title="seqread",
        xlabel="I/O size",
        ylabel="fsync latency (µs)",
        legend=True,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    axes[0][0].legend(
        handles,
        labels,
        fontsize=12,
        ncol=6,
        bbox_to_anchor=(1.85, 1.4),
        edgecolor="white",
    )
    axes[0][0].grid(visible=True, alpha=0.5)
    plt.savefig(p + "fsync_lat_vs_io_size_seqread.pdf", bbox_inches="tight")

    plt.figure(figsize=(10, 6))
    df_fsync_lat_f = pd.concat(
        [df_fsync_lat.T[workload] for workload in res.keys()],
        axis=1,
        keys=res.keys(),
    )
    print("mean fsync lat (rread): \n", df_fsync_lat_f["rread"].mean())
    d = df_fsync_lat_f["rread"].plot(
        kind="bar",
        title="rread",
        xlabel="I/O size",
        ylabel="fsync latency (µs)",
        legend=True,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    axes[0][0].legend(
        handles,
        labels,
        fontsize=12,
        ncol=6,
        bbox_to_anchor=(1.85, 1.4),
        edgecolor="white",
    )
    axes[0][0].grid(visible=True, alpha=0.5)
    plt.savefig(p + "fsync_lat_vs_io_size_rread.pdf", bbox_inches="tight")

    plt.figure(figsize=(10, 6))
    df_fsync_lat_f = pd.concat(
        [df_fsync_lat.T[workload] for workload in res.keys()],
        axis=1,
        keys=res.keys(),
    )
    print("mean fsync lat (seqwrite): \n", df_fsync_lat_f["seqwrite"].mean())
    d = df_fsync_lat_f["seqwrite"].plot(
        kind="bar",
        title="seqwrite",
        xlabel="I/O size",
        ylabel="fsync latency (µs)",
        legend=True,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    axes[0][0].legend(
        handles,
        labels,
        fontsize=12,
        ncol=6,
        bbox_to_anchor=(1.85, 1.4),
        edgecolor="white",
    )
    axes[0][0].grid(visible=True, alpha=0.5)
    plt.savefig(p + "fsync_lat_vs_io_size_seqwrite.pdf", bbox_inches="tight")

    plt.figure(figsize=(10, 6))
    df_fsync_lat_f = pd.concat(
        [df_fsync_lat.T[workload] for workload in res.keys()],
        axis=1,
        keys=res.keys(),
    )
    print("mean fsync lat (rwrite): \n", df_fsync_lat_f["rwrite"].mean())
    d = df_fsync_lat_f["rwrite"].plot(
        kind="bar",
        title="rwrite",
        xlabel="I/O size",
        ylabel="fsync latency (µs)",
        legend=True,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    axes[0][0].legend(
        handles,
        labels,
        fontsize=12,
        ncol=6,
        bbox_to_anchor=(1.85, 1.4),
        edgecolor="white",
    )
    axes[0][0].grid(visible=True, alpha=0.5)
    plt.savefig(p + "fsync_lat_vs_io_size_rwrite.pdf", bbox_inches="tight")
    
    # plot a grouped bar chart of the mean open, close, and fsync lats
    fig, axes = plt.subplots(nrows=2, ncols=2, figsize=(10, 6))
    plt.subplots_adjust(
        left=None, bottom=None, right=None, top=None, wspace=0.3, hspace=0.4
    )
    barWidth = 0.25

    workload = "seqread"
    merged = pd.concat(
        [
            df_open_lat_f.mean().to_frame().T[workload],
            df_close_lat_f.mean().to_frame().T[workload],
            df_fsync_lat_f.mean().to_frame().T[workload],
        ],
        axis=0,
        keys=["open", "close", "fsync"],
    )
    merged.index = ["open", "close", "fsync"]
    merged = merged.drop("fsync")
    d = merged.plot(
        kind="bar",
        ax=axes[0][0],
        title=workload,
        xlabel="Operation",
        ylabel="Latency (µs)",
        legend=True,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d, 2)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    axes[0][0].legend(
        handles,
        labels,
        fontsize=12,
        ncol=6,
        bbox_to_anchor=(1.85, 1.4),
        edgecolor="white",
    )
    axes[0][0].grid(visible=True, alpha=0.5)
    
    workload = "rread"
    merged = pd.concat(
        [
            df_open_lat_f.mean().to_frame().T[workload],
            df_close_lat_f.mean().to_frame().T[workload],
            df_fsync_lat_f.mean().to_frame().T[workload],
        ],
        axis=0,
        keys=["open", "close", "fsync"],
    )
    merged.index = ["open", "close", "fsync"]
    merged = merged.drop("fsync")
    d = merged.plot(
        kind="bar",
        ax=axes[0][1],
        title=workload,
        xlabel="Operation",
        ylabel="Latency (µs)",
        legend=False,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d, 2)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    # axes[0][0].legend(
    #     handles,
    #     labels,
    #     fontsize=12,
    #     ncol=6,
    #     bbox_to_anchor=(1.85, 1.4),
    #     edgecolor="white",
    # )
    axes[0][0].grid(visible=True, alpha=0.5)
    
    workload = "seqwrite"
    merged = pd.concat(
        [
            df_open_lat_f.mean().to_frame().T[workload],
            df_close_lat_f.mean().to_frame().T[workload],
            df_fsync_lat_f.mean().to_frame().T[workload],
        ],
        axis=0,
        keys=["open", "close", "fsync"],
    )
    merged.index = ["open", "close", "fsync"]
    d = merged.plot(
        kind="bar",
        ax=axes[1][0],
        title=workload,
        xlabel="Operation",
        ylabel="Latency (µs)",
        legend=False,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d, 3)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    # axes[0][0].legend(
    #     handles,
    #     labels,
    #     fontsize=12,
    #     ncol=6,
    #     bbox_to_anchor=(1.85, 1.4),
    #     edgecolor="white",
    # )
    axes[0][0].grid(visible=True, alpha=0.5)
    
    workload = "rwrite"
    merged = pd.concat(
        [
            df_open_lat_f.mean().to_frame().T[workload],
            df_close_lat_f.mean().to_frame().T[workload],
            df_fsync_lat_f.mean().to_frame().T[workload],
        ],
        axis=0,
        keys=["open", "close", "fsync"],
    )
    merged.index = ["open", "close", "fsync"]
    d = merged.plot(
        kind="bar",
        ax=axes[1][1],
        title=workload,
        xlabel="Operation",
        ylabel="Latency (µs)",
        legend=False,
        edgecolor="black",
        width=barWidth,
        rot=0,
    )
    set_patches(d, 3)
    # for item in axes[0][0].get_xticklabels():
    #     item.set_fontsize(12)
    # for item in axes[0][0].get_yticklabels():
    #     item.set_fontsize(12)
    # replace the strings "nexus"->"NeXUS" and "bfs_ci"->"bfs" in the legend
    handles, labels = axes[0][0].get_legend_handles_labels()
    labels = [l.replace("nexus", "NeXUS [20]") for l in labels]
    labels = [l.replace("bfs_ci", "bfs") for l in labels]
    # axes[0][0].legend(
    #     handles,
    #     labels,
    #     fontsize=12,
    #     ncol=6,
    #     bbox_to_anchor=(1.85, 1.4),
    #     edgecolor="white",
    # )
    axes[0][0].grid(visible=True, alpha=0.5)
    
    plt.savefig(p + "lat_vs_io_size.pdf", bbox_inches="tight")
        
        
        

    # # print a LaTeX table of the mean values
    # tab = """
    # \begin{table}[!h]
    #     \centering
    #     \begin{tabular}{lcccc}
    #         \toprule
    #         & \texttt{bfs\_ne} & \texttt{bfs} & \texttt{nfs\_ne} & \texttt{nfs\_we} \\
    #         \midrule"""

    # # for workload in res.keys():
    # #     tab += f"\n{workload} & "
    # #     for fs_type in res[workload].keys():
    # #         tab += f"{df_fsync_lat_f[workload][fs_type].mean():.2f} & "
    # #     tab = tab[:-2] + "\\\\"

    # # \texttt{open} & X & X & X & X \\
    # # \texttt{close} & X & X & X & X \\
    # # \texttt{fsync} & X & X & X & X \\
    
    # tab += """
    #         \bottomrule
    #     \end{tabular}
    #     \caption{Metadata operation latency (µs).}
    #     \label{tab:metaops}
    # \end{table}
    # """
    # print("=====")
    # print(tab)


def plot_x_vs_y():
    p = os.getenv("BFS_HOME") + "/benchmarks/o/fio_test/"
    plt.style.use("ggplot")

    res_json = glob.glob(p + "*/*/*/*/*/*/randrw*/*/randrw-*-*.json")

    mt_types = ["bfs_ci"]

    bw_dfs = {
        t: pd.DataFrame(
            columns=[
                "write_iops_mean",
                "read_iops_mean",
                "write_bw_mean",
                "read_bw_mean",
                "write_lat_ns",
                "read_lat_ns",
                "write_clat_ns_p50",
                "write_clat_ns_p99",
                "write_clat_ns_p999",
                "total_cpu",
                # "cache_hit_rate",
                # "write_ios",
                # "read_ios",
                # "write_sectors",
                # "read_sectors",
                # "write_ticks",
                # "read_ticks",
                # "in_queue",
                # "wKBs",
            ]
        )
        for t in mt_types
    }

    exp_filter = {
        "exp_type": "raw/",
        "mt": "mt-1/",
        "workload": "random/",
        "cache_size": "c0.0/",
        "cap": "1073741824",
        "rr": "randrw100/",
        # "io_size": "4k/",
        "dep_and_jobs": "randrw-1-1",
    }
    xlab = "I/O size"

    seen = set()
    for r in res_json:
        if not all(exp_filter[k] in r for k in exp_filter):
            continue
        with open(r) as f:
            df = pd.read_json(f)
            mt_type = f.name.split("fio_test/")[1].split("/")[5]

            # also just skip if the mt_type is not listed in mt_types
            if not mt_type in mt_types:
                continue

            x_val = f.name.split("fio_test/")[1].split("/")[7]
            # x_val = float(_x_val)

            # abort if we see duplicate entries
            if (mt_type, x_val) in seen:
                exit(1)
            else:
                seen.add((mt_type, x_val))

            # if x_val==64.0 or x_val==16.0 or x_val==8192:
            #     continue

            # Also read the bench_dev.tmp.log file and grep for the cache hit rate
            # log_file = f.name[: f.name.rfind("/") + 1] + "bench_dev.tmp.log"
            # with open(log_file) as lf:
            #     for line in lf:
            #         if "Secure cache hit rate:" in line:
            #             cache_hit_rate = float(line.split(" ")[4])
            #             break
            # bw_dfs[mt_type].loc[x_val, "cache_hit_rate"] = cache_hit_rate

            if df["jobs"][0]["write"]["clat_ns"]["N"] > 0:
                bw_dfs[mt_type].loc[x_val, "write_iops_mean"] = df["jobs"][0]["write"][
                    "iops_mean"
                ]

                bw_dfs[mt_type].loc[x_val, "write_bw_mean"] = (
                    df["jobs"][0]["write"]["bw_mean"] / 1024
                )

                bw_dfs[mt_type].loc[x_val, "write_lat_ns"] = (
                    df["jobs"][0]["write"]["lat_ns"]["mean"] / 1e3
                )

                bw_dfs[mt_type].loc[x_val, "write_clat_ns_p50"] = (
                    df["jobs"][0]["write"]["clat_ns"]["percentile"]["50.000000"] / 1e3
                )
                bw_dfs[mt_type].loc[x_val, "write_clat_ns_p99"] = (
                    df["jobs"][0]["write"]["clat_ns"]["percentile"]["99.000000"] / 1e3
                )
                bw_dfs[mt_type].loc[x_val, "write_clat_ns_p999"] = (
                    df["jobs"][0]["write"]["clat_ns"]["percentile"]["99.900000"] / 1e3
                )

            if df["jobs"][0]["read"]["clat_ns"]["N"] > 0:
                bw_dfs[mt_type].loc[x_val, "read_iops_mean"] = df["jobs"][0]["read"][
                    "iops_mean"
                ]

                bw_dfs[mt_type].loc[x_val, "read_bw_mean"] = (
                    df["jobs"][0]["read"]["bw_mean"] / 1024
                )

                bw_dfs[mt_type].loc[x_val, "read_lat_ns"] = (
                    df["jobs"][0]["read"]["lat_ns"]["mean"] / 1e3
                )

            bw_dfs[mt_type].loc[x_val, "total_cpu"] = (
                df["jobs"][0]["usr_cpu"] + df["jobs"][0]["sys_cpu"]
            )

            """
            bw_dfs[mt_type].loc[x_val, "write_ios"] = df["disk_util"][0]["write_ios"]
            bw_dfs[mt_type].loc[x_val, "read_ios"] = df["disk_util"][0]["read_ios"]
            bw_dfs[mt_type].loc[x_val, "write_sectors"] = df["disk_util"][0][
                "write_sectors"
            ]
            bw_dfs[mt_type].loc[x_val, "read_sectors"] = df["disk_util"][0][
                "read_sectors"
            ]
            bw_dfs[mt_type].loc[x_val, "write_ticks"] = df["disk_util"][0][
                "write_ticks"
            ]
            bw_dfs[mt_type].loc[x_val, "read_ticks"] = df["disk_util"][0]["read_ticks"]
            bw_dfs[mt_type].loc[x_val, "in_queue"] = df["disk_util"][0]["in_queue"]

            # Now that we collected all fio data, we need to collect iostat data
            iostat_file = f.name[: f.name.rfind("/") + 1] + "iostat.log"
            with open(iostat_file) as lf:
                bw_dfs[mt_type].loc[x_val, "wKBs"] = 0.0
                num_records = 0
                for line in lf:
                    # We know the iostat log file format with extended stats is as follows:
                    # Device r/s rkB/s rrqm/s %rrqm r_await rareq-sz w/s wkB/s wrqm/s %wrqm w_await wareq-sz d/s dkB/s drqm/s %drqm d_await dareq-sz aqu-sz %util
                    # Find wKB/s column and add it to the dataframe
                    # First break the line into columns
                    cols = line.split()
                    if len(cols) == 0:
                        continue
                    if not cols[0].startswith("bdus"):
                        continue
                    # Must have 21 columns
                    if len(cols) != 21:
                        exit(1)

                    # Should we only count lines that have non-zero wkB/s? Seems like that is the only fair way to ensure that we only count time periods where I/Os were actually issued. We can double check with the w/s column.
                    if float(cols[7]) != 0.0 and float(cols[8]) == 0.0:
                        print("Bad")
                        exit(1)
                    if float(cols[8]) != 0.0:
                        bw_dfs[mt_type].loc[x_val, "wKBs"] += float(cols[8])
                        num_records += 1
                if bw_dfs[mt_type].loc[x_val, "wKBs"] != 0.0:
                    bw_dfs[mt_type].loc[x_val, "wKBs"] /= num_records * 1e3
            """

    # sort index
    for mt_type in mt_types:
        bw_dfs[mt_type] = bw_dfs[mt_type].sort_index()

    exp_filter_str = str(exp_filter)
    if len(exp_filter_str) > 30:
        exp_filter_str = (
            exp_filter_str[: len(exp_filter_str) // 2]
            + "\n"
            + exp_filter_str[len(exp_filter_str) // 2 :]
        )

    print("%s" % exp_filter_str)

    # plot iops_mean
    plt.figure()
    barWidth = 0.75
    df = pd.concat(
        [bw_dfs[mt_type]["write_iops_mean"] for mt_type in mt_types],
        axis=1,
        keys=[lookup_mt_type(t) for t in mt_types],
    ).plot(kind="bar", width=barWidth, edgecolor="black")
    plt.xlabel(xlab, fontsize=15)
    plt.ylabel("IOPS", fontsize=15)
    # plt.title("%s" % exp_filter_str, fontsize=15)
    plt.grid(visible=True, alpha=0.5)
    # set_patches(df)
    # df.set_xticklabels([lookup_size(t) for t in df.get_xticks()])
    # df.set_xticklabels([t.get_text()[1:] for t in df.get_xticklabels()])
    # df.set_xticklabels(
    #     [
    #         (
    #             t.get_text() + "G"
    #             if int(t.get_text()) // 1024 < 1
    #             else str(int(t.get_text()) // 1024) + "T"
    #         )
    #         for t in df.get_xticklabels()
    #     ]
    # )
    plt.legend(fontsize=13, ncol=6, bbox_to_anchor=(0.65, -0.3))
    # plt.legend().set_visible(False)
    for item in plt.gca().get_xticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    for item in plt.gca().get_yticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    plt.gcf().set_size_inches(17, 3)
    plt.savefig(p + "write_iops_mean.pdf", bbox_inches="tight")

    # plot read_iops_mean
    plt.figure()
    barWidth = 0.75
    df = pd.concat(
        [bw_dfs[mt_type]["read_iops_mean"] for mt_type in mt_types],
        axis=1,
        keys=[lookup_mt_type(t) for t in mt_types],
    ).plot(kind="bar", width=barWidth, edgecolor="black")
    plt.xlabel(xlab, fontsize=15)
    plt.ylabel("IOPS", fontsize=15)
    # plt.title("%s" % exp_filter_str, fontsize=15)
    plt.grid(visible=True, alpha=0.5)
    set_patches(df)
    # df.set_xticklabels([lookup_size(t) for t in df.get_xticks()])
    # df.set_xticklabels([t.get_text()[1:] for t in df.get_xticklabels()])
    # df.set_xticklabels(
    #     [
    #         (
    #             t.get_text() + "G"
    #             if int(t.get_text()) // 1024 < 1
    #             else str(int(t.get_text()) // 1024) + "T"
    #         )
    #         for t in df.get_xticklabels()
    #     ]
    # )
    plt.legend(fontsize=13, ncol=3, bbox_to_anchor=(0.65, -0.3))
    # plt.legend().set_visible(False)
    for item in plt.gca().get_xticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    for item in plt.gca().get_yticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    plt.gcf().set_size_inches(17, 3)
    plt.savefig(p + "read_iops_mean.pdf", bbox_inches="tight")

    # plot lat_mean (us)
    fig, ax = plt.subplots()
    barWidth = 0.75
    df = pd.concat(
        [bw_dfs[mt_type]["write_lat_ns"] for mt_type in mt_types],
        axis=1,
        keys=[lookup_mt_type(t) for t in mt_types],
    ).plot(kind="bar", ax=ax, width=barWidth, edgecolor="black")
    plt.xlabel(xlab, fontsize=15)
    plt.ylabel("Latency (µs)", fontsize=15)
    set_patches(df)
    # ax.set_xticklabels([lookup_size(t) for t in ax.get_xticks()])
    # df.set_xticklabels([t.get_text()[1:] for t in df.get_xticklabels()])
    # df.set_xticklabels(
    #     [
    #         (
    #             t.get_text() + "G"
    #             if int(t.get_text()) // 1024 < 1
    #             else str(int(t.get_text()) // 1024) + "T"
    #         )
    #         for t in df.get_xticklabels()
    #     ]
    # )
    plt.legend(fontsize=13, ncol=3, bbox_to_anchor=(0.675, -0.3))
    # plt.legend().set_visible(False)
    # plt.title("%s" % exp_filter_str, fontsize=15)
    plt.grid(visible=True, alpha=0.5)
    for item in plt.gca().get_xticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    for item in plt.gca().get_yticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    plt.gcf().set_size_inches(17, 3)
    plt.savefig(p + "write_lat_ns.pdf", bbox_inches="tight")

    # plot read_lat_mean (us)
    fig, ax = plt.subplots()
    barWidth = 0.75
    df = pd.concat(
        [bw_dfs[mt_type]["read_lat_ns"] for mt_type in mt_types],
        axis=1,
        keys=[lookup_mt_type(t) for t in mt_types],
    ).plot(kind="bar", ax=ax, width=barWidth, edgecolor="black")
    plt.xlabel(xlab, fontsize=15)
    plt.ylabel("Latency (µs)", fontsize=15)
    set_patches(df)
    # ax.set_xticklabels([lookup_size(t) for t in ax.get_xticks()])
    # df.set_xticklabels([t.get_text()[1:] for t in df.get_xticklabels()])
    # df.set_xticklabels(
    #     [
    #         (
    #             t.get_text() + "G"
    #             if int(t.get_text()) // 1024 < 1
    #             else str(int(t.get_text()) // 1024) + "T"
    #         )
    #         for t in df.get_xticklabels()
    #     ]
    # )
    plt.legend(fontsize=13, ncol=3, bbox_to_anchor=(0.65, -0.3))
    # plt.legend().set_visible(False)
    # plt.title("%s" % exp_filter_str, fontsize=15)
    plt.grid(visible=True, alpha=0.5)
    for item in plt.gca().get_xticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    for item in plt.gca().get_yticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    plt.gcf().set_size_inches(17, 3)
    plt.savefig(p + "read_lat_ns.pdf", bbox_inches="tight")

    # plot write_bw_mean
    plt.figure()
    barWidth = 0.75
    ax = pd.concat(
        [bw_dfs[mt_type]["write_bw_mean"] for mt_type in mt_types],
        axis=1,
        keys=[lookup_mt_type(t) for t in mt_types],
    ).plot(kind="bar", width=barWidth, edgecolor="black")
    # for patch in ax.patches:
    #     ax.annotate(str(patch.get_height())[:5], (patch.get_x() * 1.005, patch.get_height() * 1.005))
    plt.xlabel(xlab, fontsize=15)
    plt.ylabel("Throughput (MB/s) ", fontsize=15)
    set_patches(ax)
    # Change x-axis labels by calling lookup_size
    # ax.set_xticklabels([lookup_size(t) for t in ax.get_xticks()])
    plt.legend(fontsize=13, ncol=3, bbox_to_anchor=(0.65, -0.3))
    # plt.legend().set_visible(False)
    # plt.title("%s" % exp_filter_str, fontsize=15)
    plt.grid(visible=True, alpha=0.5)
    for item in plt.gca().get_xticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    for item in plt.gca().get_yticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    plt.gcf().set_size_inches(10, 3)
    plt.savefig(p + "write_bw_mean.pdf", bbox_inches="tight")

    # plot read_bw_mean
    plt.figure()
    barWidth = 0.75
    ax = pd.concat(
        [bw_dfs[mt_type]["read_bw_mean"] for mt_type in mt_types],
        axis=1,
        keys=[lookup_mt_type(t) for t in mt_types],
    ).plot(kind="bar", width=barWidth, edgecolor="black")
    plt.xlabel(xlab, fontsize=15)
    plt.ylabel("Throughput (MB/s) ", fontsize=15)
    set_patches(ax)
    # ax.set_xticklabels([lookup_size(t) for t in ax.get_xticks()])
    plt.legend(fontsize=13, ncol=4, bbox_to_anchor=(0.65, -0.3))
    plt.grid(visible=True, alpha=0.5)
    for item in plt.gca().get_xticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    for item in plt.gca().get_yticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    plt.gcf().set_size_inches(10, 3)
    plt.savefig(p + "read_bw_mean.pdf", bbox_inches="tight")

    # plot a grouped bar chart for clat_ns_p50, clat_ns_p99, clat_ns_p999
    fig, ax = plt.subplots()
    barWidth = 0.75
    df = pd.concat(
        [
            bw_dfs[mt_type][
                ["write_clat_ns_p50", "write_clat_ns_p99", "write_clat_ns_p999"]
            ]
            for mt_type in mt_types
        ],
        axis=1,
        keys=[lookup_mt_type(t) for t in mt_types],
    ).plot(kind="bar", ax=ax, width=barWidth, edgecolor="black")
    plt.xlabel(xlab, fontsize=15)
    plt.ylabel("Latency (µs)", fontsize=15)
    set_patches(df)
    plt.legend(fontsize=13, ncol=3, bbox_to_anchor=(0.65, -0.3))
    # plt.title("%s" % exp_filter_str, fontsize=15)
    plt.grid(visible=True, alpha=0.5)
    for item in plt.gca().get_xticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    for item in plt.gca().get_yticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    plt.gcf().set_size_inches(10, 3)
    plt.savefig(p + "write_clat_ns.pdf", bbox_inches="tight")

    # plot total_cpu
    plt.figure()
    barWidth = 0.75
    df = pd.concat(
        [bw_dfs[mt_type]["total_cpu"] for mt_type in mt_types],
        axis=1,
        keys=[lookup_mt_type(t) for t in mt_types],
    ).plot(kind="bar", width=barWidth, edgecolor="black")
    plt.xlabel(xlab, fontsize=15)
    plt.ylabel("CPU Utilization (%)", fontsize=15)
    set_patches(df)
    plt.legend(fontsize=13, ncol=3, bbox_to_anchor=(0.65, -0.3))
    # plt.title("%s" % exp_filter_str, fontsize=15)
    plt.grid(visible=True, alpha=0.5)
    for item in plt.gca().get_xticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    for item in plt.gca().get_yticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    plt.gcf().set_size_inches(10, 3)
    plt.savefig(p + "total_cpu.pdf", bbox_inches="tight")

    # plot bw_mean/cpu, which is the I/O yield
    plt.figure()
    barWidth = 0.75
    df = pd.concat(
        [
            bw_dfs[mt_type]["write_bw_mean"] / bw_dfs[mt_type]["total_cpu"]
            for mt_type in mt_types
        ],
        axis=1,
        keys=[lookup_mt_type(t) for t in mt_types],
    ).plot(kind="bar", width=barWidth, edgecolor="black")
    plt.xlabel(xlab, fontsize=15)
    plt.ylabel("I/O Yield (TP/CPU%)", fontsize=15)
    set_patches(df)
    plt.legend(fontsize=13, ncol=3, bbox_to_anchor=(0.65, -0.3))
    # plt.title("%s" % exp_filter_str, fontsize=15)
    plt.grid(visible=True, alpha=0.5)
    for item in plt.gca().get_xticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    for item in plt.gca().get_yticklabels():
        item.set_fontsize(15)
        item.set_rotation(0)
    plt.gcf().set_size_inches(10, 3)
    plt.savefig(p + "io_yield.pdf", bbox_inches="tight")

    # # plot cache_hit_rate
    # plt.figure()
    # barWidth = 0.75
    # df = pd.concat(
    #     [bw_dfs[mt_type]["cache_hit_rate"] for mt_type in mt_types],
    #     axis=1,
    #     keys=[lookup_mt_type(t) for t in mt_types],
    # ).plot(kind="bar", width=barWidth, edgecolor="black")
    # plt.xlabel(xlab, fontsize=15)
    # plt.ylabel("Cache Hit Rate (%)", fontsize=15)
    # # plt.ylim(0.95, 1.0)
    # set_patches(df)
    # # ax.set_xticklabels([lookup_size(t) for t in ax.get_xticks()])
    # # df.set_xticklabels([t.get_text()[1:] for t in df.get_xticklabels()])
    # # plt.legend(fontsize=13, ncol=3, bbox_to_anchor=(0.65, -0.3))
    # plt.legend().set_visible(False)
    # # # plt.title("%s" % exp_filter_str, fontsize=15)
    # plt.grid(visible=True, alpha=0.5)
    # for item in plt.gca().get_xticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # for item in plt.gca().get_yticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # plt.gcf().set_size_inches(10, 3)
    # plt.savefig(p + "cache_hit_rate.pdf", bbox_inches="tight")

    # # plot write_ios
    # plt.figure()
    # barWidth = 0.75
    # df = pd.concat(
    #     [bw_dfs[mt_type]["write_ios"] for mt_type in mt_types],
    #     axis=1,
    #     keys=[lookup_mt_type(t) for t in mt_types],
    # ).plot(kind="bar", width=barWidth, edgecolor="black")
    # plt.xlabel(xlab, fontsize=15)
    # plt.ylabel("Write I/Os", fontsize=15)
    # # plt.title("%s" % exp_filter_str, fontsize=15)
    # plt.grid(visible=True, alpha=0.5)
    # # set_patches(df)
    # # df.set_xticklabels([lookup_size(t) for t in df.get_xticks()])
    # # df.set_xticklabels([t.get_text()[1:] for t in df.get_xticklabels()])
    # df.set_xticklabels(
    #     [
    #         (
    #             t.get_text() + "G"
    #             if int(t.get_text()) // 1024 < 1
    #             else str(int(t.get_text()) // 1024) + "T"
    #         )
    #         for t in df.get_xticklabels()
    #     ]
    # )
    # plt.legend(fontsize=13, ncol=6, bbox_to_anchor=(0.65, -0.3))
    # # plt.legend().set_visible(False)
    # for item in plt.gca().get_xticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # for item in plt.gca().get_yticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # plt.gcf().set_size_inches(17, 3)
    # plt.savefig(p + "write_ios.pdf", bbox_inches="tight")

    # # plot read_ios
    # plt.figure()
    # barWidth = 0.75
    # df = pd.concat(
    #     [bw_dfs[mt_type]["read_ios"] for mt_type in mt_types],
    #     axis=1,
    #     keys=[lookup_mt_type(t) for t in mt_types],
    # ).plot(kind="bar", width=barWidth, edgecolor="black")
    # plt.xlabel(xlab, fontsize=15)
    # plt.ylabel("Read I/Os", fontsize=15)
    # # plt.title("%s" % exp_filter_str, fontsize=15)
    # plt.grid(visible=True, alpha=0.5)
    # # set_patches(df)
    # # df.set_xticklabels([lookup_size(t) for t in df.get_xticks()])
    # # df.set_xticklabels([t.get_text()[1:] for t in df.get_xticklabels()])
    # df.set_xticklabels(
    #     [
    #         (
    #             t.get_text() + "G"
    #             if int(t.get_text()) // 1024 < 1
    #             else str(int(t.get_text()) // 1024) + "T"
    #         )
    #         for t in df.get_xticklabels()
    #     ]
    # )
    # plt.legend(fontsize=13, ncol=6, bbox_to_anchor=(0.65, -0.3))
    # # plt.legend().set_visible(False)
    # for item in plt.gca().get_xticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # for item in plt.gca().get_yticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # plt.gcf().set_size_inches(17, 3)
    # plt.savefig(p + "read_ios.pdf", bbox_inches="tight")

    # # plot write_sectors
    # plt.figure()
    # barWidth = 0.75
    # df = pd.concat(
    #     [bw_dfs[mt_type]["write_sectors"] for mt_type in mt_types],
    #     axis=1,
    #     keys=[lookup_mt_type(t) for t in mt_types],
    # ).plot(kind="bar", width=barWidth, edgecolor="black")
    # plt.xlabel(xlab, fontsize=15)
    # plt.ylabel("Write Sectors", fontsize=15)
    # # plt.title("%s" % exp_filter_str, fontsize=15)
    # plt.grid(visible=True, alpha=0.5)
    # # set_patches(df)
    # # df.set_xticklabels([lookup_size(t) for t in df.get_xticks()])
    # # df.set_xticklabels([t.get_text()[1:] for t in df.get_xticklabels()])
    # df.set_xticklabels(
    #     [
    #         (
    #             t.get_text() + "G"
    #             if int(t.get_text()) // 1024 < 1
    #             else str(int(t.get_text()) // 1024) + "T"
    #         )
    #         for t in df.get_xticklabels()
    #     ]
    # )
    # plt.legend(fontsize=13, ncol=6, bbox_to_anchor=(0.65, -0.3))
    # # plt.legend().set_visible(False)
    # for item in plt.gca().get_xticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # for item in plt.gca().get_yticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # plt.gcf().set_size_inches(17, 3)
    # plt.savefig(p + "write_sectors.pdf", bbox_inches="tight")

    # # plot read_sectors
    # plt.figure()
    # barWidth = 0.75
    # df = pd.concat(
    #     [bw_dfs[mt_type]["read_sectors"] for mt_type in mt_types],
    #     axis=1,
    #     keys=[lookup_mt_type(t) for t in mt_types],
    # ).plot(kind="bar", width=barWidth, edgecolor="black")
    # plt.xlabel(xlab, fontsize=15)
    # plt.ylabel("Read Sectors", fontsize=15)
    # # plt.title("%s" % exp_filter_str, fontsize=15)
    # plt.grid(visible=True, alpha=0.5)
    # # set_patches(df)
    # # df.set_xticklabels([lookup_size(t) for t in df.get_xticks()])
    # # df.set_xticklabels([t.get_text()[1:] for t in df.get_xticklabels()])
    # df.set_xticklabels(
    #     [
    #         (
    #             t.get_text() + "G"
    #             if int(t.get_text()) // 1024 < 1
    #             else str(int(t.get_text()) // 1024) + "T"
    #         )
    #         for t in df.get_xticklabels()
    #     ]
    # )
    # plt.legend(fontsize=13, ncol=6, bbox_to_anchor=(0.65, -0.3))
    # # plt.legend().set_visible(False)
    # for item in plt.gca().get_xticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # for item in plt.gca().get_yticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # plt.gcf().set_size_inches(17, 3)
    # plt.savefig(p + "read_sectors.pdf", bbox_inches="tight")

    # # plot write_sectors/write_ticks
    # plt.figure()
    # barWidth = 0.75
    # df = pd.concat(
    #     [
    #         bw_dfs[mt_type]["write_sectors"]
    #         / bw_dfs[mt_type]["write_ticks"]
    #         * 512
    #         * 1000
    #         / 1e6
    #         for mt_type in mt_types
    #     ],
    #     axis=1,
    #     keys=[lookup_mt_type(t) for t in mt_types],
    # ).plot(kind="bar", width=barWidth, edgecolor="black")
    # plt.xlabel(xlab, fontsize=15)
    # plt.ylabel("Throughput (MB/s)", fontsize=15)
    # # plt.title("%s" % exp_filter_str, fontsize=15)
    # plt.grid(visible=True, alpha=0.5)
    # # set_patches(df)
    # # df.set_xticklabels([lookup_size(t) for t in df.get_xticks()])
    # # df.set_xticklabels([t.get_text()[1:] for t in df.get_xticklabels()])
    # df.set_xticklabels(
    #     [
    #         (
    #             t.get_text() + "G"
    #             if int(t.get_text()) // 1024 < 1
    #             else str(int(t.get_text()) // 1024) + "T"
    #         )
    #         for t in df.get_xticklabels()
    #     ]
    # )
    # plt.legend(fontsize=13, ncol=6, bbox_to_anchor=(0.65, -0.3))
    # # plt.legend().set_visible(False)
    # for item in plt.gca().get_xticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # for item in plt.gca().get_yticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # plt.gcf().set_size_inches(17, 3)
    # plt.savefig(p + "disk_write_tp.pdf", bbox_inches="tight")

    # # plot read_sectors/read_ticks
    # plt.figure()
    # barWidth = 0.75
    # df = pd.concat(
    #     [
    #         bw_dfs[mt_type]["read_sectors"]
    #         / bw_dfs[mt_type]["read_ticks"]
    #         * 512
    #         * 1000
    #         / 1e6
    #         for mt_type in mt_types
    #     ],
    #     axis=1,
    #     keys=[lookup_mt_type(t) for t in mt_types],
    # ).plot(kind="bar", width=barWidth, edgecolor="black")
    # plt.xlabel(xlab, fontsize=15)
    # plt.ylabel("Throughput (MB/s)", fontsize=15)
    # # plt.title("%s" % exp_filter_str, fontsize=15)
    # plt.grid(visible=True, alpha=0.5)
    # # set_patches(df)
    # # df.set_xticklabels([lookup_size(t) for t in df.get_xticks()])
    # # df.set_xticklabels([t.get_text()[1:] for t in df.get_xticklabels()])
    # df.set_xticklabels(
    #     [
    #         (
    #             t.get_text() + "G"
    #             if int(t.get_text()) // 1024 < 1
    #             else str(int(t.get_text()) // 1024) + "T"
    #         )
    #         for t in df.get_xticklabels()
    #     ]
    # )
    # plt.legend(fontsize=13, ncol=6, bbox_to_anchor=(0.65, -0.3))
    # # plt.legend().set_visible(False)
    # for item in plt.gca().get_xticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # for item in plt.gca().get_yticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # plt.gcf().set_size_inches(17, 3)
    # plt.savefig(p + "disk_read_tp.pdf", bbox_inches="tight")

    # # plot in_queue/(write_sectors+read_sectors)
    # plt.figure()
    # barWidth = 0.75
    # df = pd.concat(
    #     [
    #         bw_dfs[mt_type]["in_queue"]
    #         / (bw_dfs[mt_type]["write_sectors"] + bw_dfs[mt_type]["read_sectors"])
    #         * 1e3
    #         for mt_type in mt_types
    #     ],
    #     axis=1,
    #     keys=[lookup_mt_type(t) for t in mt_types],
    # ).plot(kind="bar", width=barWidth, edgecolor="black")
    # plt.xlabel(xlab, fontsize=15)
    # plt.ylabel("Avg in-queue time R/W (µs)", fontsize=15)
    # # plt.title("%s" % exp_filter_str, fontsize=15)
    # plt.grid(visible=True, alpha=0.5)
    # # set_patches(df)
    # # df.set_xticklabels([lookup_size(t) for t in df.get_xticks()])
    # # df.set_xticklabels([t.get_text()[1:] for t in df.get_xticklabels()])
    # df.set_xticklabels(
    #     [
    #         (
    #             t.get_text() + "G"
    #             if int(t.get_text()) // 1024 < 1
    #             else str(int(t.get_text()) // 1024) + "T"
    #         )
    #         for t in df.get_xticklabels()
    #     ]
    # )
    # plt.legend(fontsize=13, ncol=6, bbox_to_anchor=(0.65, -0.3))
    # # plt.legend().set_visible(False)
    # for item in plt.gca().get_xticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # for item in plt.gca().get_yticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # plt.gcf().set_size_inches(17, 3)
    # plt.savefig(p + "avg_in_queue_time.pdf", bbox_inches="tight")

    # # plot iostat wkBs
    # plt.figure()
    # barWidth = 0.75
    # df = pd.concat(
    #     [bw_dfs[mt_type]["wKBs"] for mt_type in mt_types],
    #     axis=1,
    #     keys=[lookup_mt_type(t) for t in mt_types],
    # ).plot(kind="bar", width=barWidth, edgecolor="black")
    # plt.xlabel(xlab, fontsize=15)
    # plt.ylabel("Throughput (MB/s)", fontsize=15)
    # # plt.title("%s" % exp_filter_str, fontsize=15)
    # plt.grid(visible=True, alpha=0.5)
    # # set_patches(df)
    # # df.set_xticklabels([lookup_size(t) for t in df.get_xticks()])
    # # df.set_xticklabels([t.get_text()[1:] for t in df.get_xticklabels()])
    # df.set_xticklabels(
    #     [
    #         (
    #             t.get_text() + "G"
    #             if int(t.get_text()) // 1024 < 1
    #             else str(int(t.get_text()) // 1024) + "T"
    #         )
    #         for t in df.get_xticklabels()
    #     ]
    # )
    # plt.legend(fontsize=13, ncol=6, bbox_to_anchor=(0.65, -0.3))
    # # plt.legend().set_visible(False)
    # for item in plt.gca().get_xticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # for item in plt.gca().get_yticklabels():
    #     item.set_fontsize(15)
    #     item.set_rotation(0)
    # plt.gcf().set_size_inches(17, 3)
    # plt.savefig(p + "iostat_wKBs.pdf", bbox_inches="tight")


def main():
    # traces = [
    #     "fio_test/raw/mt4/zipf:2.5/c0.01/1073741824/dmt-mp/randrw1/4k/recorded_write_trace",
    #     "fio_test/raw/mt0/zipf:2.5/c0.01/1073741824/dmt-mp/randrw1/4k/recorded_write_trace",
    #     # "recorded_write_trace",
    #     # "meta-recorded_write_trace",
    #     # "recorded_read_trace",
    #     # "meta-recorded_read_trace",
    #     # "recorded_trace",
    #     # "meta-recorded_trace",
    # ]
    # for t in traces:
    #     plot_recorded_trace_details(t)

    # logs = [
    #     "fio_test/raw/mt4/zipf:2.5/c0.01/1073741824/dmt-mp/randrw1/4k/randrw-iodepth-1-numjobs-1_bw.1.log",
    #     "fio_test/raw/mt0/zipf:2.5/c0.01/1073741824/dmt-mp/randrw1/4k/randrw-iodepth-1-numjobs-1_bw.1.log",
    #     # "raw/mt0/scaled_iolog_alibaba_loop_399/c1.0/8589934592/bdus-209/randrw0/4k/randrw-iodepth-1-numjobs-1_bw.1.log",
    #     # "raw/mt4/scaled_iolog_alibaba_loop_399/c1.0/8589934592/bdus-208/randrw0/4k/randrw-iodepth-1-numjobs-1_bw.1.log",
    #     # "raw/mt3/scaled_iolog_alibaba_loop_399/c1.0/8589934592/bdus-210/randrw0/4k/randrw-iodepth-1-numjobs-1_bw.1.log",
    #     # "raw/mt5/scaled_iolog_alibaba_loop_4/c1.0/2199023255552/bdus-29/randrw0/4k/randrw-iodepth-1-numjobs-1_bw.1.log",
    # ]
    # plot_bwlog_ecdf(logs)

    # plot_iolog_details()

    # plot_x_vs_y()

    plot_micro()


if __name__ == "__main__":
    main()
