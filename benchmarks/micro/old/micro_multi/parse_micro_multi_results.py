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


# class Op(Enum):
#     INVALID_OP = -1
#     CLIENT_GETATTR_OP = 0
#     CLIENT_MKDIR_OP = 1
#     CLIENT_UNLINK_OP = 2
#     CLIENT_RMDIR_OP = 3
#     CLIENT_RENAME_OP = 4
#     CLIENT_OPEN_OP = 5
#     CLIENT_READ_OP = 6
#     CLIENT_WRITE_OP = 7
#     CLIENT_RELEASE_OP = 8
#     CLIENT_FSYNC_OP = 9
#     CLIENT_OPENDIR_OP = 10
#     CLIENT_READDIR_OP = 11
#     CLIENT_INIT_OP = 12
#     CLIENT_INIT_MKFS_OP = 13
#     CLIENT_DESTROY_OP = 14
#     CLIENT_CREATE_OP = 15


# def cut_outliers(lst):
#     """Cuts outliers from a given list and returns the new list."""
#     if len(lst) == 0:
#         return lst

#     q75, q25 = np.percentile(lst, [75, 25])
#     iqr = q75 - q25
#     high = q75 + (1.5 * iqr)
#     low = q25 - (1.5 * iqr)

#     new_lst = []
#     for e in lst:
#         if (e <= high) and (e >= low):
#             new_lst.append(e)

#     return np.array(new_lst)


# def plot_lat_histo(fstem):
#     """Plots the distribution of latencies with a histogram."""
#     lats = []

#     with open(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/%s.csv" % fstem,
#         newline="",
#     ) as f:
#         reader = csv.reader(f)
#         data = list(reader)
#         if (len(data) > 0) and (len(data[0]) > 0):
#             lats = [float(lat) / 1e3 for lat in data[0]]

#     if len(lats) == 0:
#         print("No lats found in file, skipping")
#         return

#     print("%s_lats size: %d" % (fstem, len(lats)))
#     print("mean %s lat: %.3f ms" % (fstem, np.mean(lats)))

#     plt.rcParams["font.family"] = "Times New Roman"
#     plt.figure()
#     plt.hist(
#         lats,
#         color="dodgerblue",
#         ec="black",
#         bins=100,
#     )
#     plt.xlabel("%s latency (ms)" % fstem)
#     plt.ylabel("frequency")
#     plt.grid(True, color="grey", alpha=0.1)
#     plt.savefig(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/%s.pdf" % fstem,
#         bbox_inches="tight",
#     )


# def get_res(fname, calc_mean=True):
#     __res = []
#     reader = None
#     data = None
#     with open(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/%s.csv" % fname,
#         newline="",
#     ) as f:
#         reader = csv.reader(f)
#         data = list(reader)
#         if (len(data) > 0) and (len(data[0]) > 0):
#             __res = np.array([float(res) for res in data[0]])
#         # __res = cut_outliers(__res)
#         print("\n%s size: %d" % (fname, len(__res)))
#         if len(__res) > 0 and calc_mean:
#             print("mean %s: %.3f" % (fname, np.mean(__res)))

#     return __res


# def plot_component_lats__local(outfile="comp_lats.pdf"):
#     """
#     Plots per-component measurements (client, server, and disk). First extracts all the csv data, then parses it and graphs the results.

#     Args:
#         outfile (str, optional): Name of output file to save plots.
#     """

#     c_read__lats = get_res("__c_read__lats")  # get client overall read latencies
#     c_read__c_lats = get_res(
#         "__c_read__c_lats"
#     )  # get client read non-network latencies
#     c_read__net_send_lats = get_res(
#         "__c_read__net_send_lats"
#     )  # get client read net send latencies
#     c_read__net_recv_lats = get_res(
#         "__c_read__net_recv_lats"
#     )  # get client read net recv latencies
#     c_write__lats = get_res("__c_write__lats")  # get client overall write latencies
#     c_write__c_lats = get_res(
#         "__c_write__c_lats"
#     )  # get client write non-network latencies
#     c_write__net_send_lats = get_res(
#         "__c_write__net_send_lats"
#     )  # get client write net send latencies
#     c_write__net_recv_lats = get_res(
#         "__c_write__net_recv_lats"
#     )  # get client write net recv latencies
#     s_read__lats = get_res("__s_read__lats")  # get server overall read latencies
#     s_read__s_lats = get_res(
#         "__s_read__s_lats"
#     )  # get server read non-network latencies
#     s_read__net_c_send_lats = get_res(
#         "__s_read__net_c_send_lats"
#     )  # get server read net send latencies to clients
#     s_read__net_d_send_lats = get_res(
#         "__s_read__net_d_send_lats"
#     )  # get server read net send latencies to devices
#     s_read__net_recv_lats = get_res(
#         "__s_read__net_recv_lats"
#     )  # get server read net recv latencies
#     s_write__lats = get_res("__s_write__lats")  # get server overall write latencies
#     s_write__s_lats = get_res(
#         "__s_write__s_lats"
#     )  # get server write non-network latencies
#     s_write__net_c_send_lats = get_res(
#         "__s_write__net_c_send_lats"
#     )  # get server write net send latencies to clients
#     s_write__net_d_send_lats = get_res(
#         "__s_write__net_d_send_lats"
#     )  # get server write net send latencies to devices
#     s_write__net_recv_lats = get_res(
#         "__s_write__net_recv_lats"
#     )  # get server write net recv latencies
#     s_other__net_d_send_lats = get_res(
#         "__s_other__net_d_send_lats"
#     )  # get server other-operations net send latencies to devices
#     __e_blk_reads_per_fs_read_counts = get_res(
#         "__e_blk_reads_per_fs_read_counts"
#     )  # get the number of block reads per fs-level read
#     __e_blk_writes_per_fs_read_counts = get_res(
#         "__e_blk_writes_per_fs_read_counts"
#     )  # get the number of block writes per fs-level read
#     __e_blk_reads_per_fs_write_counts = get_res(
#         "__e_blk_reads_per_fs_write_counts"
#     )  # get the number of block reads per fs-level write
#     __e_blk_writes_per_fs_write_counts = get_res(
#         "__e_blk_writes_per_fs_write_counts"
#     )  # get the number of block writes per fs-level write
#     __e_blk_reads_per_fs_other_counts = get_res(
#         "__e_blk_reads_per_fs_other_counts"
#     )  # get the number of block reads per fs-level other
#     __e_blk_writes_per_fs_other_counts = get_res(
#         "__e_blk_writes_per_fs_other_counts"
#     )  # get the number of block writes per fs-level other
#     __fs_op_sequence = get_res(
#         "__fs_op_sequence", False
#     )  # get the fs operation call sequence

#     # get client, server, and device averages and plot
#     num_components = 4
#     num_ops = 2
#     relative_lats = [np.array([0.0, 0.0]) for _ in range(num_components)]

#     relative_lats[0][0] = (
#         np.mean(c_read__lats) - np.mean(c_read__net_send_lats) - np.mean(s_read__lats)
#     )  # client handler
#     relative_lats[1][0] = np.mean(c_read__net_send_lats)  # client sends
#     relative_lats[2][0] = np.mean(s_read__s_lats)  # server handler
#     relative_lats[3][0] = np.mean(s_read__net_c_send_lats)  # server sends to client

#     relative_lats[0][1] = (
#         np.mean(c_write__lats)
#         - np.mean(c_write__net_send_lats)
#         - np.mean(s_write__lats)
#     )  # client handler
#     relative_lats[1][1] = np.mean(c_write__net_send_lats)  # client sends
#     relative_lats[2][1] = np.mean(s_write__s_lats)  # server handler
#     relative_lats[3][1] = np.mean(s_write__net_c_send_lats)  # server sends to client

#     avg_total_lat = np.array(
#         [
#             np.sum([relative_lats[i][j] for i in range(num_components)])
#             for j in range(num_ops)
#         ]
#     )

#     print(
#         "\n[read] op latencies (usec):\n\tclient: %.3f\n\tclient->serv: %.3f\n\tserv: %.3f\n\tserv->client: %.3f\n\ttotal: %.3f\n"
#         % (
#             relative_lats[0][0],
#             relative_lats[1][0],
#             relative_lats[2][0],
#             relative_lats[3][0],
#             avg_total_lat[0],
#         )
#     )

#     print(
#         "[write] op latencies (usec):\n\tclient: %.3f\n\tclient->serv: %.3f\n\tserv: %.3f\n\tserv->client: %.3f\n\ttotal: %.3f\n"
#         % (
#             relative_lats[0][1],
#             relative_lats[1][1],
#             relative_lats[2][1],
#             relative_lats[3][1],
#             avg_total_lat[1],
#         )
#     )

#     plt.rcParams["font.family"] = "Times New Roman"
#     plt.rcParams["font.size"] = 15
#     plt.rcParams["legend.fancybox"] = False
#     plt.figure()
#     bot = 0.0
#     labels = [
#         "client handler",  # c_read__c_lats
#         r"client$\rightarrow$server link",  # c_read__net_send_lats
#         "server handler",  # s_read__s_lats (includes the local device activity)
#         r"server$\rightarrow$client link",  # s_read__net_c_send_lats
#     ]
#     for i in range(num_components):
#         bot = 0.0 if i == 0 else bot + relative_lats[i - 1] / avg_total_lat
#         plt.bar(
#             ["read", "write"],
#             relative_lats[i] / avg_total_lat,
#             ec="black",
#             bottom=bot,
#             label=labels[i],
#         )

#     plt.xlabel("I/O type")
#     plt.ylabel("relative execution time (%)")
#     plt.grid(True, color="grey", alpha=0.1)
#     plt.legend(framealpha=0.5, edgecolor="black")
#     plt.savefig(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/%s" % outfile,
#         bbox_inches="tight",
#     )

#     return


# def plot_component_lats__remote(outfile="comp_lats.pdf"):
#     """
#     Plots per-component measurements (client, server, and disk). First extracts all the csv data, then parses it and graphs the results.

#     Args:
#         outfile (str, optional): Name of output file to save plots.
#     """

#     print("Not updated, aborting")
#     exit(-1)

#     c_read__lats = get_res("__c_read__lats")  # get client overall read latencies
#     c_read__c_lats = get_res("c_read__c_lats")  # get client read non-network latencies
#     c_read__net_send_lats = get_res(
#         "__c_read__net_send_lats"
#     )  # get client read net send latencies
#     c_read__net_recv_lats = get_res(
#         "c_read__net_recv_lats"
#     )  # get client read net recv latencies
#     c_write__lats = get_res("c_write__lats")  # get client overall write latencies
#     c_write__c_lats = get_res(
#         "__c_write__c_lats"
#     )  # get client write non-network latencies
#     c_write__net_send_lats = get_res(
#         "c_write__net_send_lats"
#     )  # get client write net send latencies
#     c_write__net_recv_lats = get_res(
#         "c_write__net_recv_lats"
#     )  # get client write net recv latencies
#     s_read__lats = get_res("s_read__lats")  # get server overall read latencies
#     s_read__s_lats = get_res("s_read__s_lats")  # get server read non-network latencies
#     s_read__net_c_send_lats = get_res(
#         "s_read__net_c_send_lats"
#     )  # get server read net send latencies to clients
#     s_read__net_d_send_lats = get_res(
#         "s_read__net_d_send_lats"
#     )  # get server read net send latencies to devices
#     s_read__net_recv_lats = get_res(
#         "s_read__net_recv_lats"
#     )  # get server read net recv latencies
#     s_write__lats = get_res("s_write__lats")  # get server overall write latencies
#     s_write__s_lats = get_res(
#         "s_write__s_lats"
#     )  # get server write non-network latencies
#     s_write__net_c_send_lats = get_res(
#         "s_write__net_c_send_lats"
#     )  # get server write net send latencies to clients
#     s_write__net_d_send_lats = get_res(
#         "s_write__net_d_send_lats"
#     )  # get server write net send latencies to devices
#     s_write__net_recv_lats = get_res(
#         "s_write__net_recv_lats"
#     )  # get server write net recv latencies
#     s_other__net_d_send_lats = get_res(
#         "s_other__net_d_send_lats"
#     )  # get server other-operations net send latencies to devices
#     __e_blk_reads_per_fs_read_counts = get_res(
#         "__e_blk_reads_per_fs_read_counts"
#     )  # get the number of block reads per fs-level read
#     __e_blk_writes_per_fs_read_counts = get_res(
#         "__e_blk_writes_per_fs_read_counts"
#     )  # get the number of block writes per fs-level read
#     __e_blk_reads_per_fs_write_counts = get_res(
#         "__e_blk_reads_per_fs_write_counts"
#     )  # get the number of block reads per fs-level write
#     __e_blk_writes_per_fs_write_counts = get_res(
#         "__e_blk_writes_per_fs_write_counts"
#     )  # get the number of block writes per fs-level write
#     __e_blk_reads_per_fs_other_counts = get_res(
#         "__e_blk_reads_per_fs_other_counts"
#     )  # get the number of block reads per fs-level other
#     __e_blk_writes_per_fs_other_counts = get_res(
#         "__e_blk_writes_per_fs_other_counts"
#     )  # get the number of block writes per fs-level other
#     d_read__lats = get_res("d_read__lats")  # get device10 overall read latencies
#     d_read__d_lats = get_res(
#         "d_read__d_lats"
#     )  # get device10 read non-network latencies
#     d_read__net_send_lats = get_res(
#         "d_read__net_send_lats"
#     )  # get device10 read net send latencies
#     d_write__lats = get_res("d_write__lats")  # get device10 overall write latencies
#     d_write__d_lats = get_res(
#         "d_write__d_lats"
#     )  # get device10 write non-network latencies
#     d_write__net_send_lats = get_res(
#         "d_write__net_send_lats"
#     )  # get device10 write net send latencies
#     __fs_op_sequence = get_res(
#         "__fs_op_sequence", False
#     )  # get the fs operation call sequence

#     # Parses the data and creates new lists suitable for graphing

#     # get devices averages by grouping certain block reads and writes
#     blk_level_lats_per_fs_read = []  # reads+writes lats
#     blk_level_lats_per_fs_write = []  # reads+writes lats
#     blk_reads_per_fs_read_idx = 0
#     blk_writes_per_fs_read_idx = 0
#     blk_reads_per_fs_write_idx = 0
#     blk_writes_per_fs_write_idx = 0
#     blk_reads_per_fs_other_idx = 0
#     blk_writes_per_fs_other_idx = 0
#     # track the index into the dev lats list (assumes all reads/writes go to single disk (10) for now)
#     dev_read_idx = 0  # index into the device read latencies list
#     dev_write_idx = 0  # index into the device write latencies list

#     s_send_lats_per_fs_read = []
#     s_send_lats_per_fs_write = []
#     s_read__net_send_idx = 0
#     s_write__net_send_idx = 0
#     for op_idx in range(len(__fs_op_sequence)):
#         if __fs_op_sequence[op_idx] == Op.CLIENT_READ_OP.value:
#             # add the block-read latencies for this fs-level read
#             dev_lat_sum = 0.0
#             for lat_idx in range(
#                 int(dev_read_idx),
#                 int(
#                     dev_read_idx
#                     + __e_blk_reads_per_fs_read_counts[blk_reads_per_fs_read_idx]
#                 ),
#             ):
#                 dev_lat_sum += d_read__lats[lat_idx]  # update latency sum
#             dev_read_idx += __e_blk_reads_per_fs_read_counts[blk_reads_per_fs_read_idx]

#             # add the block-write latencies for this fs-level read
#             for lat_idx in range(
#                 int(dev_write_idx),
#                 int(
#                     dev_write_idx
#                     + __e_blk_writes_per_fs_read_counts[blk_writes_per_fs_read_idx]
#                 ),
#             ):
#                 dev_lat_sum += d_write__lats[lat_idx]  # update latency sum
#             dev_write_idx += __e_blk_writes_per_fs_read_counts[
#                 blk_writes_per_fs_read_idx
#             ]

#             # add the network send latencies to devices for this fs-level read . Note that they may be summed out of order (ie block reads/writes might be intertwined) but it doesnt matter
#             s_read__net_d_lat_sum = 0.0
#             for lat_idx in range(
#                 int(s_read__net_send_idx),
#                 int(
#                     s_read__net_send_idx
#                     + __e_blk_reads_per_fs_read_counts[blk_reads_per_fs_read_idx]
#                 ),
#             ):
#                 s_read__net_d_lat_sum += s_read__net_d_send_lats[
#                     lat_idx
#                 ]  # update send latency sum
#             s_read__net_send_idx += __e_blk_reads_per_fs_read_counts[
#                 blk_reads_per_fs_read_idx
#             ]

#             for lat_idx in range(
#                 int(s_read__net_send_idx),
#                 int(
#                     s_read__net_send_idx
#                     + __e_blk_writes_per_fs_read_counts[blk_writes_per_fs_read_idx]
#                 ),
#             ):
#                 s_read__net_d_lat_sum += s_write__net_d_send_lats[
#                     lat_idx
#                 ]  # update send latency sum
#             s_read__net_send_idx += __e_blk_writes_per_fs_read_counts[
#                 blk_writes_per_fs_read_idx
#             ]

#             # update these indices for both sums
#             blk_reads_per_fs_read_idx += 1
#             blk_writes_per_fs_read_idx += 1

#             # append all the per-read dev latencies (the sum of all associated block ops)
#             blk_level_lats_per_fs_read.append(dev_lat_sum)
#             s_send_lats_per_fs_read.append(s_read__net_d_lat_sum)
#         elif __fs_op_sequence[op_idx] == Op.CLIENT_WRITE_OP.value:
#             # add the block-read latencies for this fs-level write
#             dev_lat_sum = 0.0
#             for lat_idx in range(
#                 int(dev_read_idx),
#                 int(
#                     dev_read_idx
#                     + __e_blk_reads_per_fs_write_counts[blk_reads_per_fs_write_idx]
#                 ),
#             ):
#                 dev_lat_sum += d_read__lats[lat_idx]
#             dev_read_idx += __e_blk_reads_per_fs_write_counts[
#                 blk_reads_per_fs_write_idx
#             ]

#             # add the block-write latencies for this fs-level write
#             for lat_idx in range(
#                 int(dev_write_idx),
#                 int(
#                     dev_write_idx
#                     + __e_blk_writes_per_fs_write_counts[blk_writes_per_fs_write_idx]
#                 ),
#             ):
#                 dev_lat_sum += d_write__lats[lat_idx]
#             dev_write_idx += __e_blk_writes_per_fs_write_counts[
#                 blk_writes_per_fs_write_idx
#             ]

#             s_write__net_d_lat_sum = 0.0
#             for lat_idx in range(
#                 int(s_write__net_send_idx),
#                 int(
#                     s_write__net_send_idx
#                     + __e_blk_reads_per_fs_write_counts[blk_reads_per_fs_write_idx]
#                 ),
#             ):
#                 s_write__net_d_lat_sum += s_write__net_d_send_lats[
#                     lat_idx
#                 ]  # update send latency sum
#             s_write__net_send_idx += __e_blk_reads_per_fs_write_counts[
#                 blk_reads_per_fs_write_idx
#             ]

#             for lat_idx in range(
#                 int(s_write__net_send_idx),
#                 int(
#                     s_write__net_send_idx
#                     + __e_blk_writes_per_fs_write_counts[blk_writes_per_fs_write_idx]
#                 ),
#             ):
#                 s_write__net_d_lat_sum += s_write__net_d_send_lats[
#                     lat_idx
#                 ]  # update send latency sum
#             s_write__net_send_idx += __e_blk_writes_per_fs_write_counts[
#                 blk_writes_per_fs_write_idx
#             ]

#             blk_reads_per_fs_write_idx += 1
#             blk_writes_per_fs_write_idx += 1

#             # append all the per-read dev latencies (the sum of all associated block ops)
#             blk_level_lats_per_fs_write.append(dev_lat_sum)
#             s_send_lats_per_fs_write.append(s_write__net_d_lat_sum)
#         else:
#             # just skip the appropriate number of reads and writes
#             dev_read_idx += __e_blk_reads_per_fs_other_counts[
#                 blk_reads_per_fs_other_idx
#             ]
#             dev_write_idx += __e_blk_writes_per_fs_other_counts[
#                 blk_writes_per_fs_other_idx
#             ]

#             # s_read__net_send_idx += __e_blk_reads_per_fs_read_counts[
#             #     blk_reads_per_fs_other_idx
#             # ]
#             # s_read__net_send_idx += __e_blk_writes_per_fs_read_counts[
#             #     blk_writes_per_fs_other_idx
#             # ]

#             # s_write__net_send_idx += __e_blk_reads_per_fs_write_counts[
#             #     blk_reads_per_fs_other_idx
#             # ]
#             # s_write__net_send_idx += __e_blk_writes_per_fs_write_counts[
#             #     blk_writes_per_fs_other_idx
#             # ]

#             blk_reads_per_fs_other_idx += 1
#             blk_writes_per_fs_other_idx += 1

#     # get client, server, and device averages and plot
#     # list format: [[read, write] x {c_read__c_lats, c_read__net_send_lats, c_read__net_recv_lats, server, disk}]
#     # TODO: get rid of overlapping times here
#     num_components = 7
#     num_ops = 2
#     relative_lats = [np.array([0.0, 0.0]) for _ in range(num_components)]

#     # relative_lats[0][0] = np.mean(c_read__c_lats)
#     # relative_lats[1][0] = np.mean(c_read__net_send_lats)
#     # relative_lats[2][0] = np.mean(s_read__s_lats) - np.mean(
#     #     blk_level_lats_per_fs_read
#     # )  # subtract the block r/w time to get all fs method time + send time for now
#     # relative_lats[3][0] = np.mean(blk_level_lats_per_fs_read)
#     # relative_lats[4][0] = np.mean(d_read__net_send_lats)
#     # relative_lats[5][0] = np.mean(s_read__net_c_send_lats)

#     # relative_lats[0][1] = np.mean(c_write__c_lats)
#     # relative_lats[1][1] = np.mean(c_write__net_send_lats)
#     # relative_lats[2][1] = np.mean(s_write__s_lats) - np.mean(
#     #     blk_level_lats_per_fs_write
#     # )  # subtract the block r/w time to get all fs method time + send time for now
#     # relative_lats[3][1] = np.mean(blk_level_lats_per_fs_write)
#     # relative_lats[4][1] = np.mean(d_write__net_send_lats)
#     # relative_lats[5][1] = np.mean(s_write__net_send_lats)

#     relative_lats[0][0] = (
#         np.mean(c_read__lats) - np.mean(c_read__net_send_lats) - np.mean(s_read__lats)
#     )  # client handler
#     relative_lats[1][0] = np.mean(c_read__net_send_lats)  # client sends
#     relative_lats[2][0] = np.mean(s_read__lats) - np.mean(
#         s_send_lats_per_fs_read
#     )  # server handler
#     relative_lats[3][0] = np.mean(s_send_lats_per_fs_read) - np.mean(
#         blk_level_lats_per_fs_read
#     )  # server sends to device (without waitConn)
#     relative_lats[4][0] = np.mean(d_read__lats) - np.mean(
#         d_read__net_send_lats
#     )  # device handler
#     relative_lats[5][0] = np.mean(d_read__net_send_lats)  # device sends
#     relative_lats[6][0] = np.mean(s_read__net_c_send_lats)  # server sends to client

#     relative_lats[0][1] = (
#         np.mean(c_write__lats)
#         - np.mean(c_write__net_send_lats)
#         - np.mean(s_write__lats)
#     )  # client handler
#     relative_lats[1][1] = np.mean(c_write__net_send_lats)  # client sends
#     relative_lats[2][1] = np.mean(s_write__lats) - np.mean(
#         s_send_lats_per_fs_write
#     )  # server handler
#     relative_lats[3][1] = np.mean(s_send_lats_per_fs_write) - np.mean(
#         blk_level_lats_per_fs_write
#     )  # server sends to device (without waitConn)
#     relative_lats[4][1] = np.mean(d_write__lats) - np.mean(
#         d_write__net_send_lats
#     )  # device handler
#     relative_lats[5][1] = np.mean(d_write__net_send_lats)  # device sends
#     relative_lats[6][1] = np.mean(s_write__net_c_send_lats)  # server sends to client

#     avg_total_lat = np.array(
#         [
#             np.sum([relative_lats[i][j] for i in range(num_components)])
#             for j in range(num_ops)
#         ]
#     )

#     plt.rcParams["font.family"] = "Times New Roman"
#     plt.figure()
#     bot = 0.0
#     labels = [
#         "client handler",  # c_read__c_lats
#         r"client$\rightarrow$server link",  # c_read__net_send_lats
#         "server handler",  # s_read__s_lats-blk_level_lats_per_fs_read
#         r"server$\rightarrow$device link",  # s_send_lats_per_fs_read
#         "device handler",  # blk_level_lats_per_fs_read
#         r"device$\rightarrow$server link",  # d_read__net_send_lats
#         r"server$\rightarrow$client link",  # s_read__net_c_send_lats
#     ]
#     for i in range(num_components):
#         bot = 0.0 if i == 0 else bot + relative_lats[i - 1] / avg_total_lat
#         plt.bar(
#             ["read", "write"],
#             relative_lats[i] / avg_total_lat,
#             ec="black",
#             bottom=bot,
#             label=labels[i],
#         )

#     plt.xlabel("I/O type")
#     plt.ylabel("relative execution time (%)")
#     plt.grid(True, color="grey", alpha=0.1)
#     plt.legend(framealpha=0.2)
#     plt.savefig(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/%s" % outfile,
#         bbox_inches="tight",
#     )

#     return


# def plot_fb_tp_old2(fstem, num_samples, fs_type):
#     """Get stats from filebench results (mainly used for read and write)."""

#     print("== Results for [%s]" % fstem)
#     reader = None
#     iops = [None for _ in range(num_samples)]
#     tps = [None for _ in range(num_samples)]

#     op_sizes = None
#     if fs_type == "bfs":
#         op_sizes = [4062, 8124, 16248, 32496, 64992, 129984]
#     else:
#         op_sizes = [4096, 8192, 16384, 32768, 65536, 131072]
#     op_labels = [i for i in range(len(op_sizes))]
#     op_labels_str = [str(4 * (2 ** i)) for i in range(len(op_sizes))]
#     mean_iops = None
#     mean_tps = None

#     with open(
#         os.getenv("BFS_HOME")
#         + ("/benchmarks/micro/output/micro__%s__%s.csv" % (fs_type, fstem)),
#         newline="",
#     ) as f:
#         reader = csv.reader(f)
#         iops = np.array(
#             [
#                 np.array([int(sample) for sample in isz if len(sample) > 0])
#                 for isz in list(reader)
#             ]
#         )
#         print("iops [isz][sample]:\n", iops)
#         tps = np.array([op_sizes[i] * iops[i] for i in range(len(op_sizes))]) / 1e6
#         print("tps [isz][sample]:\n", tps)

#     mean_iops = np.array([np.mean(isz_samples) for isz_samples in iops])
#     print("mean iops:", mean_iops)
#     mean_tps = np.array([np.mean(isz_samples) for isz_samples in tps])
#     print("mean tps (MB/s):", mean_tps)

#     # plt.rcParams["font.family"] = "Times New Roman"
#     plt.rcParams["font.family"] = "Arial"
#     # plt.rcParams["font.size"] = 20
#     plt.rcParams["legend.fancybox"] = False

#     fig, ax = plt.subplots(figsize=(3.5, 1.75))
#     ax.bar(
#         op_labels,
#         mean_iops,
#         label="iops",
#         color="lightblue",
#         width=1.0,
#         edgecolor="black",
#     )
#     ax.set_ylabel("Throughput (IOPS)")
#     ax.set_xlabel("I/O Size (KiB)")
#     ax.set_xticks(op_labels, op_labels_str)

#     ax2 = ax.twinx()
#     # ax2.spines["right"].set_visible(False)
#     ax2.spines["top"].set_visible(False)
#     ax2.plot(op_labels, mean_tps, label="tps", color="black", marker="s")
#     ax2.set_ylabel("Throughput (MB/s)")
#     ax2.set_xlabel("I/O Size (KiB)")

#     hdls = [
#         Patch(facecolor="lightblue", edgecolor="black", label="iops"),
#         Line2D([0], [0], color="black", lw=2, label="tps", marker="s"),
#     ]
#     ax2.legend(
#         handles=hdls, loc=(0.5, 0.725), framealpha=0.0, edgecolor="white", fontsize=8
#     )

#     plt.savefig(
#         os.getenv("BFS_HOME")
#         + "/benchmarks/micro/output/micro__%s__%s.pdf" % (fs_type, fstem),
#         bbox_inches="tight",
#     )


# def plot_fb_tp1(fstem, num_samples):
#     """Get stats from filebench results (mainly used for read and write)."""
#     print("== Results for [%s]" % fstem)

#     # plt.rcParams["font.family"] = "Times New Roman"
#     plt.rcParams["font.family"] = "Arial"
#     # plt.rcParams["font.size"] = 20
#     plt.rcParams["legend.fancybox"] = False
#     fig, ax = plt.subplots(figsize=(3.5, 1.75))
#     ax2 = ax.twinx()
#     fs_types = ["bfs", "nfs"]
#     fs_type_markers = {"bfs": "s", "nfs": "^"}
#     reader = None
#     iops = dict()
#     tps = dict()
#     mean_iops = dict()
#     mean_tps = dict()
#     op_sizes = {
#         "bfs": [4062, 8124, 16248, 32496, 64992, 129984],
#         "nfs": [4096, 8192, 16384, 32768, 65536, 131072],
#     }
#     op_positions = [i for i in range(len(op_sizes["bfs"]))]
#     op_labels_str = [str(4 * (2 ** i)) for i in range(len(op_sizes["bfs"]))]

#     # parse results
#     for fs_type in fs_types:
#         iops[fs_type] = [None for _ in range(num_samples)]
#         tps[fs_type] = [None for _ in range(num_samples)]

#         mean_iops[fs_type] = None
#         mean_tps[fs_type] = None

#         with open(
#             os.getenv("BFS_HOME")
#             + (
#                 "/benchmarks/micro/output/micro__%s__%s.csv"
#                 % (fs_type if fs_type == "bfs" else "x", fstem)
#             ),
#             newline="",
#         ) as f:
#             reader = csv.reader(f)

#             iops[fs_type] = np.array(
#                 [
#                     np.array([int(sample) for sample in isz if len(sample) > 0])
#                     for isz in list(reader)
#                 ]
#             )
#             print("iops[%s] [isz][sample]:\n" % fs_type, iops[fs_type])

#             tps[fs_type] = (
#                 np.array(
#                     [
#                         op_sizes[fs_type][i] * iops[fs_type][i]
#                         for i in range(len(op_sizes[fs_type]))
#                     ]
#                 )
#                 / 1e6
#             )
#             print("tps[%s] [isz][sample]:\n" % fs_type, tps[fs_type])

#         mean_iops[fs_type] = np.array(
#             [np.mean(isz_samples) for isz_samples in iops[fs_type]]
#         )
#         print("mean iops[%s]:" % fs_type, mean_iops[fs_type])
#         mean_tps[fs_type] = np.array(
#             [np.mean(isz_samples) for isz_samples in tps[fs_type]]
#         )
#         print("mean tps[%s] (MB/s):" % fs_type, mean_tps[fs_type])

#     # plot
#     for fs_type in fs_types:
#         # ax.bar(
#         #     op_positions,
#         #     mean_iops,
#         #     label="iops",
#         #     color="lightblue",
#         #     width=1.0,
#         #     edgecolor="black",
#         # )
#         ax.plot(
#             op_positions,
#             mean_iops[fs_type],
#             # label="iops",
#             color="red",
#             linestyle="-",
#             markerfacecolor="none",
#             marker=fs_type_markers[fs_type],
#         )
#         ax2.plot(
#             op_positions,
#             mean_tps[fs_type],
#             # label="tps",
#             color="black",
#             linestyle="--",
#             markerfacecolor="none",
#             marker=fs_type_markers[fs_type],
#         )

#     # finish setting up axes

#     # ax2.spines["right"].set_visible(False)
#     ax2.spines["top"].set_visible(False)

#     ax.set_xticks(op_positions, op_labels_str)

#     ax.set_ylabel("IOPS")
#     ax.set_xlabel("I/O Size (KiB)")

#     ax2.set_ylabel("MB/s")
#     ax2.set_xlabel("I/O Size (KiB)")

#     hdls = list()
#     for fs_type in fs_types:
#         # iops
#         hdls.append(
#             Line2D(
#                 [0],
#                 [0],
#                 color="red",
#                 lw=1.25,
#                 label=r"$\mathtt{%s}$ iops" % fs_type.upper(),
#                 linestyle="-",
#                 marker=fs_type_markers[fs_type],
#                 markerfacecolor="none",
#                 markersize=4,
#             )
#         )

#         # tps
#         hdls.append(
#             Line2D(
#                 [0],
#                 [0],
#                 color="black",
#                 lw=1.25,
#                 label=r"$\mathtt{%s}$ tps" % fs_type.upper(),
#                 linestyle="--",
#                 marker=fs_type_markers[fs_type],
#                 markerfacecolor="none",
#                 markersize=4,
#             )
#         )

#     ax2.legend(
#         handles=hdls,
#         # loc=(0.05, 0.4),
#         loc="best",
#         framealpha=0.0,
#         edgecolor="white",
#         fontsize=6,
#     )
#     plt.savefig(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/micro__%s.pdf" % fstem,
#         bbox_inches="tight",
#     )


# def plot_fb_tp():
#     plt.style.use("seaborn-deep")
#     fig, axes = plt.subplots(nrows=2, ncols=2, sharex=True, sharey=True, figsize=(4, 3))
#     fig.tight_layout()
#     fig.subplots_adjust(wspace=0.25, hspace=0.5)

#     x_sr = pd.read_csv(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/micro__x__seqread.csv"
#     )
#     x_sr.insert(
#         0,
#         "fs_type",
#         ["nfs" for iosz in list(x_sr.iosz)],
#     )
#     x_sr.insert(
#         0,
#         "_io_sz",
#         [str(int(iosz * 1.0 / 1024)) for iosz in list(x_sr.iosz)],
#     )

#     x_sw = pd.read_csv(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/micro__x__seqwrite.csv"
#     )
#     x_sw.insert(
#         0,
#         "fs_type",
#         ["nfs" for iosz in list(x_sw.iosz)],
#     )
#     x_sw.insert(
#         0,
#         "_io_sz",
#         [str(int(iosz * 1.0 / 1024)) for iosz in list(x_sw.iosz)],
#     )

#     x_rr = pd.read_csv(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/micro__x__rread.csv"
#     )
#     x_rr.insert(
#         0,
#         "fs_type",
#         ["nfs" for iosz in list(x_rr.iosz)],
#     )
#     x_rr.insert(
#         0,
#         "_io_sz",
#         [str(int(iosz * 1.0 / 1024)) for iosz in list(x_rr.iosz)],
#     )

#     x_rw = pd.read_csv(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/micro__x__rwrite.csv"
#     )
#     x_rw.insert(
#         0,
#         "fs_type",
#         ["nfs" for iosz in list(x_rw.iosz)],
#     )
#     x_rw.insert(
#         0,
#         "_io_sz",
#         [str(int(iosz * 1.0 / 1024)) for iosz in list(x_rw.iosz)],
#     )

#     b_sr = pd.read_csv(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/micro__bfs__seqread.csv"
#     )
#     b_sr.insert(
#         0,
#         "fs_type",
#         ["bfs" for iosz in list(b_sr.iosz)],
#     )
#     b_sr.insert(
#         0,
#         "_io_sz",
#         [str(int(iosz * 1.0 / 1024)) for iosz in list(b_sr.iosz)],
#     )

#     b_sw = pd.read_csv(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/micro__bfs__seqwrite.csv"
#     )
#     b_sw.insert(
#         0,
#         "fs_type",
#         ["bfs" for iosz in list(b_sw.iosz)],
#     )
#     b_sw.insert(
#         0,
#         "_io_sz",
#         [str(int(iosz * 1.0 / 1024)) for iosz in list(b_sw.iosz)],
#     )

#     b_rr = pd.read_csv(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/micro__bfs__rread.csv"
#     )
#     b_rr.insert(
#         0,
#         "fs_type",
#         ["bfs" for iosz in list(b_rr.iosz)],
#     )
#     b_rr.insert(
#         0,
#         "_io_sz",
#         [str(int(iosz * 1.0 / 1024)) for iosz in list(b_rr.iosz)],
#     )

#     b_rw = pd.read_csv(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/micro__bfs__rwrite.csv"
#     )
#     b_rw.insert(
#         0,
#         "fs_type",
#         ["bfs" for iosz in list(b_rw.iosz)],
#     )
#     b_rw.insert(
#         0,
#         "_io_sz",
#         [str(int(iosz * 1.0 / 1024)) for iosz in list(b_rw.iosz)],
#     )

#     # will only compute mean over the numeric values
#     fig.text(0.5, -0.01, "I/O size", ha="center")
#     fig.text(-0.05, 0.5, "IOPS", va="center", rotation="vertical")
#     # plt.ylabel("iops")
#     # plt.xlabel("IOSZ")
#     _b_sr = b_sr.pivot("_io_sz", "fs_type", "sample0").plot(ax=axes[0, 0])
#     _x_sr = x_sr.pivot("_io_sz", "fs_type", "sample0").plot(ax=axes[0, 0])
#     axes[0, 0].legend(framealpha=0.0)
#     axes[0, 0].set_title("seqread", fontsize=10)
#     axes[0, 0].spines["top"].set_visible(False)
#     axes[0, 0].spines["right"].set_visible(False)
#     axes[0, 0].axes.get_xaxis().set_label_text("")

#     _b_sw = b_sw.pivot("_io_sz", "fs_type", "sample0").plot(ax=axes[0, 1])
#     _x_sw = x_sw.pivot("_io_sz", "fs_type", "sample0").plot(ax=axes[0, 1])
#     axes[0, 1].legend(framealpha=0.0)
#     axes[0, 1].set_title("seqwrite", fontsize=10)
#     axes[0, 1].spines["top"].set_visible(False)
#     axes[0, 1].spines["right"].set_visible(False)
#     axes[0, 1].axes.get_xaxis().set_label_text("")

#     _b_rr = b_rr.pivot("_io_sz", "fs_type", "sample0").plot(ax=axes[1, 0])
#     _x_rr = x_rr.pivot("_io_sz", "fs_type", "sample0").plot(ax=axes[1, 0])
#     axes[1, 0].legend(framealpha=0.0)
#     axes[1, 0].set_title("rread", fontsize=10)
#     axes[1, 0].spines["top"].set_visible(False)
#     axes[1, 0].spines["right"].set_visible(False)
#     axes[1, 0].axes.get_xaxis().set_label_text("")

#     _b_rw = b_rw.pivot("_io_sz", "fs_type", "sample0").plot(ax=axes[1, 1])
#     _x_rw = x_rw.pivot("_io_sz", "fs_type", "sample0").plot(ax=axes[1, 1])
#     axes[1, 1].legend(framealpha=0.0)
#     axes[1, 1].set_title("rwrite", fontsize=10)
#     axes[1, 1].spines["top"].set_visible(False)
#     axes[1, 1].spines["right"].set_visible(False)
#     axes[1, 1].axes.get_xaxis().set_label_text("")
#     # _x_sr.xlabel("ioSZ")

#     # insert columns to pivot on
#     # lats.insert(
#     #     0,
#     #     "utility",
#     #     [cmd.split("./macro_")[1].split(".sh")[0] for cmd in list(lats.command)],
#     # )
#     # lats.insert(
#     #     0,
#     #     "fs_type",
#     #     [
#     #         "bfs" if (cmd.split("c /")[1].split("/")[0] == "tmp") else "nfs"
#     #         for cmd in list(lats.command)
#     #     ],
#     # )

#     # lats.pivot("utility", "fs_type", "mean").plot.bar(
#     #     # x="command",
#     #     # y="mean",
#     #     # yerr=lats.set_index(lats.command).stddev,
#     #     rot=0,
#     #     capsize=4,
#     #     edgecolor="black",
#     # )

#     # snss.despine()
#     # plt.gca().spines["right"].set_visible(False)
#     # plt.gca().spines["top"].set_visible(False)
#     # plt.gca().legend(framealpha=0.0)
#     # plt.gca().get_xaxis().tick_bottom()
#     # plt.gca().get_yaxis().tick_left()
#     # plt.gca().legend.fancybox = False
#     # plt.rcParams["axes.spines.right"] = False
#     # plt.rcParams["axes.spines.top"] = False
#     # plt.ylabel("iops")
#     plt.savefig(
#         os.getenv("BFS_HOME") + "/benchmarks/micro/output/micro_rw_res.pdf",
#         bbox_inches="tight",
#     )


# def get_fb_oc_lats(fop, num_samples):
#     """Print individual file operation stats from filebench results."""
#     print("\n== Analyzing [%s]" % fop)

#     fs_types = ["bfs", "nfs"]

#     for fs_type in fs_types:
#         reader = None
#         iops = [None for _ in range(num_samples)]
#         mean_iops = 0.0

#         with open(
#             os.getenv("BFS_HOME")
#             + (
#                 "/benchmarks/micro/output/micro__%s__%s.csv"
#                 % (fs_type if fs_type == "bfs" else "x", fop)
#             ),
#             newline="",
#         ) as f:
#             reader = csv.reader(f)
#             iops = [
#                 [int(sample) for sample in isz if len(sample) > 0]
#                 for isz in list(reader)
#             ]
#             print("iops [isz][sample]:", iops)

#         mean_iops = [np.mean(isz_samples) for isz_samples in iops]
#         print("mean iops:", mean_iops)


def get_tp():
    # parse all results and set up dataframes
    ops = ["seqread", "rread", "seqwrite", "rwrite"]
    cfgs = ["bfs", "nfs"]
    _res = {op_idx: [None] * len(cfgs) for op_idx in range(len(ops))}
    for op_idx in range(len(ops)):
        for cfg_idx in range(len(cfgs)):
            _res[op_idx][cfg_idx] = pd.read_csv(
                os.getenv("BFS_HOME")
                + "/benchmarks/micro_multi/output/%s-%s.csv"
                % (cfgs[cfg_idx], ops[op_idx])
            )
            _res[op_idx][cfg_idx] = (
                _res[op_idx][cfg_idx].dropna(axis=1).drop("sample", axis=1)
            )
            _res[op_idx][cfg_idx] = _res[op_idx][cfg_idx].mean(0).to_frame()
            _res[op_idx][cfg_idx] = _res[op_idx][cfg_idx].set_axis(["avg"], axis=1)
            _res[op_idx][cfg_idx].insert(
                0,
                "fs_type",
                [cfgs[cfg_idx] for _ in _res[op_idx][cfg_idx].index],
            )
            _res[op_idx][cfg_idx].insert(
                0,
                "iosz",
                [
                    int(int(iosz) * 1.0 / 1e3)
                    for iosz in list(_res[op_idx][cfg_idx].index)
                ],
            )
            # _res[op_idx][cfg_idx].insert(
            #     0,
            #     "_avg",
            #     [avg / 1e3 for avg in list(_res[op_idx][cfg_idx].avg)],
            # )
            # _res[op_idx][cfg_idx].insert(
            #     0,
            #     "tp",
            #     [
            #         (
            #             int(_res[op_idx][cfg_idx].index[iosz_idx])
            #             * _res[op_idx][cfg_idx].avg[iosz_idx]
            #         )
            #         / 1e6
            #         for iosz_idx in range(len(list(_res[op_idx][cfg_idx].index)))
            #     ],
            # )

    # plot iops and tp
    plt.style.use("seaborn-deep")
    fs_type_markers = ["s", "x"]
    fs_type_styles = ["-", "--"]

    ## iops
    # plt.figure()
    # fig, axes = plt.subplots(nrows=2, ncols=2, sharex=True, sharey=True, figsize=(4, 3))
    # fig.tight_layout()
    # fig.subplots_adjust(wspace=0.25, hspace=0.5)
    # fig.text(0.5, -0.00, "I/O size (KB)", ha="center")
    # fig.text(0.03, 0.5, "IOPS (K)", va="center", rotation="vertical")
    # for op_idx in range(len(ops)):
    #     for cfg_idx in range(len(cfgs)):
    #         _res[op_idx][cfg_idx].pivot("iosz", "fs_type", "_avg").plot(
    #             ax=axes[op_idx // 2, op_idx % 2],
    #             color="black",
    #             linestyle=fs_type_styles[cfg_idx % 2],
    #             linewidth=1,
    #             markerfacecolor="none",
    #             markersize=4,
    #             marker=fs_type_markers[cfg_idx % 2],
    #         )
    #     axes[op_idx // 2, op_idx % 2].legend(framealpha=0.0)
    #     axes[op_idx // 2, op_idx % 2].set_title(ops[op_idx], fontsize=10)
    #     axes[op_idx // 2, op_idx % 2].spines["top"].set_visible(False)
    #     axes[op_idx // 2, op_idx % 2].spines["right"].set_visible(False)
    #     axes[op_idx // 2, op_idx % 2].axes.get_xaxis().set_label_text("")
    # plt.savefig(
    #     os.getenv("BFS_HOME") + "/benchmarks/micro/output/micro_iops_res.pdf",
    #     bbox_inches="tight",
    # )

    ## tp
    plt.figure()
    fig, axes = plt.subplots(nrows=2, ncols=2, sharex=True, sharey=True, figsize=(4, 3))
    fig.tight_layout()
    fig.subplots_adjust(wspace=0.25, hspace=0.5)
    fig.text(0.5, -0.00, "I/O size (KB)", ha="center")
    fig.text(-0.02, 0.5, "MB/s", va="center", rotation="vertical")
    for op_idx in range(len(ops)):
        for cfg_idx in range(len(cfgs)):
            _res[op_idx][cfg_idx].pivot("iosz", "fs_type", "avg").plot(
                ax=axes[op_idx // 2, op_idx % 2],
                color="black",
                linestyle=fs_type_styles[cfg_idx % 2],
                linewidth=1,
                markerfacecolor="none",
                markersize=4,
                marker=fs_type_markers[cfg_idx % 2],
            )
        axes[op_idx // 2, op_idx % 2].legend(framealpha=0.0)
        axes[op_idx // 2, op_idx % 2].set_title(ops[op_idx], fontsize=10)
        axes[op_idx // 2, op_idx % 2].spines["top"].set_visible(False)
        axes[op_idx // 2, op_idx % 2].spines["right"].set_visible(False)
        axes[op_idx // 2, op_idx % 2].axes.get_xaxis().set_label_text("")
    plt.savefig(
        os.getenv("BFS_HOME") + "/benchmarks/micro_multi/output/micro_multi_tp_res.pdf",
        bbox_inches="tight",
    )


if __name__ == "__main__":
    # if len(sys.argv) < 2:
    #     print("No args passed")
    #     exit(1)

    # if sys.argv[1] == "h":
    #     print("\n=== Plotting component latency histograms:")
    #     plot_lat_histo("__c_read__lats")
    #     plot_lat_histo("__c_write__lats")
    #     plot_lat_histo("__s_read__lats")
    #     plot_lat_histo("__s_write__lats")
    #     # plot_lat_histo("__d10_read__lats")
    #     # plot_lat_histo("__d10_write__lats")
    #     # plot_lat_histo("__d20_read__lats")
    #     # plot_lat_histo("__d20_write__lats")

    # elif sys.argv[1] == "c":
    #     if len(sys.argv) < 3:
    #         print("Not enough args passed")
    #         exit(1)

    #     print("\n=== Plotting component-wise latencies:")
    #     if sys.argv[2] == "l":  # local disks
    #         plot_component_lats__local()
    #     elif sys.argv[2] == "r":  # remote disks
    #         plot_component_lats__remote()
    #     else:
    #         print("Wrong arg passed")
    #         exit(1)

    # elif sys.argv[1] == "m":
    #     if len(sys.argv) < 2:
    #         print("Not enough args passed")
    #         exit(1)

    #     print("\nPlotting throughputs ...")
    #     plot_fb_tp()

    # elif sys.argv[1] == "o":
    #     print("\nCalculating per-operation fb stats:")

    #     if len(sys.argv) < 3:
    #         print("Not enough args passed")
    #         exit(1)

    #     get_fb_oc_lats("open", int(sys.argv[2]))
    #     get_fb_oc_lats("close", int(sys.argv[2]))
    #     # plot_fb_lats("getattr")

    # elif sys.argv[1] == "t":
    print("Plotting microbenchmark results:")
    get_tp()
    print("Done")
