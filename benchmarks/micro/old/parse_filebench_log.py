#! /usr/bin/env python

from zplot import *

# populate zplot table from data file
t = table('line.data')

canvas = pdf('line.pdf', dimensions=['4in','1.5in'], font="Times-Roman")

# a drawable is a region of a canvas, and is used to convert data
# coordinates to raw pixel coordinates on the canvas, based on
# xrange and yrange.
d = drawable(canvas, xrange=[0,1024], yrange=[0,10])

# create an axis for our drawable.  Specify axis labels with
# [START,END,STEP] via xauto and yauto.
axis(d, xtitle='File Size (MB)', xauto=[0,1024,256],
     ytitle='Copy Time (s)', yauto=[0,10,2], ytitleshift=[0,0])

# a plotter fetches coordinates from the table, querying for the
# specified xfield and yfield, and converting these via the drawable.
p = plotter()

# each thing we plot can be added to the legend
L = legend()

# for line color, we use a string that is 'red,green,blue'.
# A primary color can range from 0 to 1, so '1,1,1' is white,
# '0,0,0' is black, and '1,0,0' is red.
p.line(d, t, xfield='size', yfield='disk_time',
       linewidth=2, linecolor='0.6,0.6,0.6',
       legend=L, legendtext='disk')

# for the linedash pattern, we specify [LINE,SPACE].  So in this
# case, the line will consist of 3-pixel dashes, each separated by
# a 1-pixel space.
p.line(d, t, xfield='size', yfield='ssd_time',
       linedash=[3,1], legend=L, legendtext='SSD')

# draw legend near top-left of drawable
L.draw(canvas, coord=[d.left()+20, d.top()-10])

# save to eps file
canvas.render()

# #! /usr/bin/env python3

# import sys
# from zplot import *


# def print_usage_and_exit():
#     print(f"Usage: {sys.argv[0]} <plot_name> <webserver_data_path>")
#     exit(1)


# if len(sys.argv) != 3:
#     print_usage_and_exit()

# plot_name = sys.argv[1]
# webserver_data_path = sys.argv[2]

# LINE_WIDTH = 1
# LINES = ["ufs_0", "ufs_50", "ufs_75", "ufs_100", "ext4"]

# LINES_COLOR_MAP = {
#     "ufs_100": "0,0,0",
#     "ufs_75": "0.4,0.4,0.4",
#     "ufs_50": "0.6,0.6,0.6",
#     "ufs_0": "0.8,0.8,0.8",
#     "ext4": "black"
# }

# LINES_DASH_MAP = {
#     "ufs_0": 0,
#     "ufs_50": 0,
#     "ufs_75": 0,
#     "ufs_100": 0,
#     "ext4": [4, 1.6],
# }

# # LINES_POINT_MAP = {}

# LINES_NAME_MAP = {
#     "ufs_0": "uFS-0%",
#     "ufs_50": "uFS-50%",
#     "ufs_75": "uFS-75%",
#     "ufs_100": "uFS-100%",
#     "ext4": "ext4"
# }

# POINTS_MAP = {
#     "ufs_50": "xline",
#     "ufs_75": "triangle",
# }

# DRAWABLE_X_LEN = 200
# DRAWABLE_Y_LEN = 100
# DRAWABLE_COORD_X = 20
# DRAWABLE_COORD_Y = 22

# YLABEL_SHIFT = [0, 4]

# XTITLE_SHIFT = [0, 2]
# YTITLE_SHIFT = [6, 0]

# LEGEND_BASE = [30, 120]
# LEGEND_X_OFFSET = 50
# LEGEND_Y_OFFSET = 10
# LEGEND_EACH_ROW = 2
# LEGEND_FONT_SZ = 7

# TITLE_FONT_SZ = 7
# LABEL_FONT_SZ = 7

# legend_map = {line: legend() for line in LINES}
# point_legend_map = {
#     "ufs_50": legend(),
#     "ufs_75": legend(),
# }

# ctype = 'eps'
# if len(sys.argv) == 2:
#     ctype = sys.argv[1]

# c = canvas(ctype,
#            title=plot_name,
#            dimensions=[
#                DRAWABLE_X_LEN + DRAWABLE_COORD_X + 5,
#                DRAWABLE_Y_LEN + DRAWABLE_COORD_Y + 10
#            ])
# p = plotter()
# t = table(file=webserver_data_path)
# d = drawable(canvas=c,
#              coord=[DRAWABLE_COORD_X, DRAWABLE_COORD_Y],
#              dimensions=[DRAWABLE_X_LEN, DRAWABLE_Y_LEN],
#              xrange=[1, 10],
#              yrange=[0, 5000000])

# ymanual = [[f"{y//1000000}M", y] for y in range(1000000, 5000001, 1000000)]
# # ymanual[0] = ['0', 0]

# for line in LINES:
#     p.line(drawable=d,
#            table=t,
#            xfield='num_client',
#            yfield=line,
#            linewidth=LINE_WIDTH,
#            linecolor=LINES_COLOR_MAP[line],
#            linedash=LINES_DASH_MAP[line],
#            legend=legend_map[line],
#            legendtext=LINES_NAME_MAP[line])
#     if line in POINTS_MAP:
#         p.points(
#             drawable=d,
#             table=t,
#             xfield='num_client',
#             yfield=line,
#             size=1,
#             style=POINTS_MAP[line],
#             legend=point_legend_map[line],
#             linecolor=LINES_COLOR_MAP[line],
#         )

# axis(drawable=d,
#      xtitle='Number of Clients',
#      ytitle='IOPS',
#      xlabelfontsize=LABEL_FONT_SZ,
#      ylabelfontsize=LABEL_FONT_SZ,
#      xtitlesize=TITLE_FONT_SZ,
#      ytitlesize=TITLE_FONT_SZ,
#      ylabelshift=YLABEL_SHIFT,
#      xtitleshift=XTITLE_SHIFT,
#      ytitleshift=YTITLE_SHIFT,
#      ylabelrotate=90,
#      xauto=[1, 10, 1],
#      ymanual=ymanual)

# legend_base_x, legend_base_y = LEGEND_BASE
# cnt = 0

# # manually set order
# for line in ["ufs_100", "ufs_75", "ufs_50", "ufs_0", "ext4"]:
#     legend = legend_map[line]
#     legend.draw(
#         canvas=c,
#         coord=[
#             legend_base_x + (cnt % LEGEND_EACH_ROW) * LEGEND_X_OFFSET,
#             legend_base_y - (cnt // LEGEND_EACH_ROW) * LEGEND_Y_OFFSET
#         ],
#         # width=3,
#         # height=3,
#         fontsize=LEGEND_FONT_SZ)
#     cnt += 1

# point_legend_map["ufs_50"].draw(
#     canvas=c,
#     coord=[
#         legend_base_x + (2 % LEGEND_EACH_ROW) * LEGEND_X_OFFSET + 3.5,
#         legend_base_y - (2 // LEGEND_EACH_ROW) * LEGEND_Y_OFFSET
#     ],
#     height=3,
#     width=3)

# point_legend_map["ufs_75"].draw(
#     canvas=c,
#     coord=[
#         legend_base_x + (1 % LEGEND_EACH_ROW) * LEGEND_X_OFFSET + 3.5,
#         legend_base_y - (1 // LEGEND_EACH_ROW) * LEGEND_Y_OFFSET
#     ],
#     height=3,
#     width=3)

# c.render()




########################################################################################################################



# #! /usr/bin/env python3

# # This script merges multiple csv files into a z-plot friendly file

# import sys
# import csv


# def print_usage_and_exit():
#     print(f"Usage: {sys.argv[0]} <ufs_data_dir> <ext4_data_dir>", file=sys.stderr)
#     exit(1)


# if len(sys.argv) != 3:
#     print_usage_and_exit()

# ufs_data_dir = sys.argv[1]
# ext4_data_dir = sys.argv[2]

# fs_types = ["ext4"] + [f"ufs_{cache_ratio}" for cache_ratio in [0, 50, 75, 100]]

# # the plot use "clients" instead of "apps", so we switch name here...
# header = ["#", "num_client"] + fs_types

# # map (fs_type, num_app) to iops
# iops_map = {}

# with open(f"{ext4_data_dir}/ext4_webserver.csv", "rt") as f:
#     f_csv = csv.reader(f)
#     for line_num, line in enumerate(f_csv):
#         if line_num == 0:
#             assert line[0] == "num_app" and line[1] == "iops"
#             continue
#         num_app, iops = int(line[0]), float(line[1])
#         assert num_app == line_num
#         iops_map[("ext4", num_app)] = iops

# for cache_ratio in [0, 50, 75, 100]:
#     with open(f"{ufs_data_dir}/ufs-cache-hit-{cache_ratio}_webserver.csv", "rt") as f:
#         f_csv = csv.reader(f)
#         for line_num, line in enumerate(f_csv):
#             if line_num == 0:
#                 assert line[0] == "num_app" and line[1] == "iops"
#                 continue
#             num_app, iops = int(line[0]), float(line[1])
#             assert num_app == line_num
#             iops_map[(f"ufs_{cache_ratio}", num_app)] = iops

# print("\t".join(header))
# for num_app in range(1, 11):
#     line = [f"{num_app}"]
#     for fs_type in fs_types:
#         line.append(str(iops_map[(fs_type, num_app)]))
#     print("\t".join(line))




########################################################################################################################



# #! /usr/bin/env python3
# import sys
# import csv


# def print_usage_and_exit():
#     print(f"Usage: {sys.argv[0]} <ufs|ext4> <data_dir>")
#     exit(1)


# if len(sys.argv) != 3:
#     print_usage_and_exit()

# fs_type = sys.argv[1]
# data_dir = sys.argv[2]

# if fs_type != "ufs" and fs_type != "ext4":
#     print_usage_and_exit()


# class AverageCounter:
#     def __init__(self):
#         self.value = 0
#         self.count = 0

#     def AddValue(self, value, count=1):
#         assert value >= 0
#         self.value += value * count
#         self.count += count

#     def Average(self):
#         if self.count == 0:
#             return 0
#         return self.value / self.count

#     def Clear(self):
#         self.value = 0
#         self.count = 0


# def collect_op_data(source, op_name, dst):
#     if op_name in source[0]:
#         ops = int(source[1][:-3])
#         lat = float(source[4][:-5])
#         dst.AddValue(lat, ops)


# def get_header():
#     return [
#         "num_app", "iops", "l3_hit_ratio", "open_latency", "read_latency",
#         "close_latency", "append_latency"
#     ]


# # return a list of data
# def parse_one_log(log_name):
#     open_counter = AverageCounter()
#     read_counter = AverageCounter()
#     close_counter = AverageCounter()
#     append_counter = AverageCounter()
#     iops = None
#     l3_hit_rate = None

#     with open(log_name, "rt") as f:
#         for line in f.readlines():
#             split = line.split()
#             if len(split) >= 7:
#                 collect_op_data(split, "openfile", open_counter)
#                 collect_op_data(split, "readfile", read_counter)
#                 collect_op_data(split, "closefile", close_counter)
#                 collect_op_data(split, "appendlog", append_counter)
#                 if split[2] == "Summary:":
#                     iops = float(split[5])
#                 if split[0] == "L3" and split[1] == "Hit:":
#                     l3_hit_rate = float(split[6])

#     return [
#         iops,
#         l3_hit_rate,
#         open_counter.Average(),
#         read_counter.Average(),
#         close_counter.Average(),
#         append_counter.Average(),
#     ]


# # collect data of a fixed uFS cache hit ratio/ext4 under varying number of apps,
# # which corresponds to one curve in the final figure
# def collect_data_for_one_config(cache_ratio, csv_name):
#     results = []
#     for num_app in range(1, 11):
#         if cache_ratio is None:  # for ext4
#             log_name = f"{data_dir}/num-app-{num_app}_ext4_filebench.out"
#         else:
#             log_name = f"{data_dir}/num-app-{num_app}_ufs-cache-hit-{cache_ratio}_filebench.out"
#         line = [num_app]
#         line.extend(parse_one_log(log_name))
#         results.append(line)

#     with open(csv_name, "wt") as f:
#         f_csv = csv.writer(f)
#         f_csv.writerow(get_header())
#         f_csv.writerows(results)


# if fs_type == "ufs":
#     for cache_ratio in [0, 50, 75, 100]:
#         collect_data_for_one_config(
#             cache_ratio,
#             f"{data_dir}/ufs-cache-hit-{cache_ratio}_webserver.csv")
# elif fs_type == "ext4":
#     collect_data_for_one_config(None, f"{data_dir}/ext4_webserver.csv")
