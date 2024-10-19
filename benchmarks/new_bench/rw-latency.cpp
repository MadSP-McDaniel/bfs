/**
 * @file latency.cpp
 * @brief Workload for measuring the latencies of I/Os to the FUSE mount.
 */

#include <chrono>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <libgen.h>
#include <random>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

/* Indicates whether or not to write the results to the file. We can either
 * record them from the 'user' (the party that uses the FUSE mount), or use the
 * results recorded within the client code instead. */
// static bool write_results = false;

// static std::vector<double> read_latencies;
// static std::vector<double> write_latencies;
// double read_start_time, read_end_time, write_start_time, write_end_time;
// static std::string vec_to_str(std::vector<double> &);
static void run_write(int);
static void run_read(int);
static double __get_time(void);
static uint64_t __get_random_value(uint64_t, uint64_t);

static int num_samples = 0;
static const int num_files = 1; // leave at 1 when measuring open/closes for now
static const std::string fstem = "/test/";
// static const int fsz = 1073741824;
static int fsz = 0;
static int op_sz = 0;
static int iterations_per_sample = 0;
static int write_sz = 0, read_sz = 0;
static int fds[num_files] = {0};
static int random_flag = 0;

static double total_write_time = 0.0, total_read_time = 0.0,
			  total_open_time_r = 0.0, total_close_time_r = 0.0,
			  total_open_time_w = 0.0, total_close_time_w = 0.0;

int main(int argc, char *argv[]) {
	// std::string read_lats_fname, write_lats_fname;
	// std::ofstream read_lats_f, write_lats_f;

	// if (write_results) {
	// 	read_lats_fname = std::string(getenv("BFS_HOME"));
	// 	read_lats_fname += "/benchmarks/micro/output/read_lats.csv";
	// 	read_lats_f.open(read_lats_fname.c_str(), std::ios::trunc);

	// 	write_lats_fname = std::string(getenv("BFS_HOME"));
	// 	write_lats_fname += "/benchmarks/micro/output/write_lats.csv";
	// 	write_lats_f.open(write_lats_fname.c_str(), std::ios::trunc);
	// }

	if (!argv[1] || !argv[2] || !argv[3] || !argv[4]) {
		printf("args are bad\n");
		exit(1);
	} else {
		if (*argv[1] == 'r')
			random_flag = 1;
		num_samples = atoi(argv[2]);
		fsz = atoi(argv[3]);
		op_sz = atoi(argv[4]);
		iterations_per_sample = fsz / op_sz;
		write_sz = op_sz;
		read_sz = op_sz;
	}

	for (int s = 0; s < num_samples; s++)
		run_write(s);

	for (int s = 0; s < num_samples; s++)
		run_read(s);

	double total_MB =
		(op_sz * num_samples * num_files * iterations_per_sample) / 1e6;

	printf("Results for [%s (%s, num_samples=%d, fsz=%d, op_sz=%d, "
		   "iterations_per_sample=%d)]\n",
		   fstem.c_str(), random_flag ? "rand" : "seq", num_samples, fsz, op_sz,
		   iterations_per_sample);
	printf("Write throughput: (%.3f MB / %.3f s) %.3f MB/s\n", total_MB,
		   total_write_time / 1e6, total_MB / (total_write_time / 1e6));
	printf("Read throughput: (%.3f MB / %.3f s) %.3f MB/s\n", total_MB,
		   total_read_time / 1e6, total_MB / (total_read_time / 1e6));
	printf("Open/create latency (w): %.3f ms\n",
		   total_open_time_w / 1e3 / num_samples);
	printf("Close latency (w): %.3f ms\n",
		   total_close_time_w / 1e3 / num_samples);
	printf("Open latency (r): %.3f ms\n",
		   total_open_time_r / 1e3 / num_samples);
	printf("Close latency (r): %.3f ms\n",
		   total_close_time_r / 1e3 / num_samples);

	// if (write_results) {
	// 	std::string read_lats = vec_to_str(read_latencies);
	// 	read_lats_f << read_lats.c_str();
	// 	// printf("Read latencies (us, %lu records):\n[%s]\n",
	// 	// 	   read_latencies.size(), read_lats.c_str());

	// 	// log and write the results to a file
	// 	std::string write_lats = vec_to_str(write_latencies);
	// 	write_lats_f << write_lats.c_str();
	// 	// printf("Write latencies (us, %lu records):\n[%s]\n",
	// 	// 	   write_latencies.size(), write_lats.c_str());

	// 	read_lats_f.close();
	// 	write_lats_f.close();
	// }

	return 0;
}

static double __get_time() {
	return std::chrono::time_point_cast<std::chrono::microseconds>(
			   std::chrono::high_resolution_clock::now())
		.time_since_epoch()
		.count();
}

static uint64_t __get_random_value(uint64_t min, uint64_t max) {
	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_int_distribution<uint64_t> dis(min, max);
	return dis(gen);
}

void run_write(int sample) {
	char *buf = new char[op_sz]; // sized to read/write 1 raw eff block
	std::string fname;

	// open the files
	double curr_open_start_time, curr_open_end_time;
	for (int f = 0; f < num_files; f++) {
		fname = fstem + std::to_string(f);

		curr_open_start_time = __get_time();
		// fds[f] = open(fname.c_str(),
		// 			  O_RDWR | O_CREAT | O_DIRECT | O_SYNC | O_TRUNC, 0777);
		fds[f] = open(fname.c_str(), O_RDWR | O_CREAT, 0777);
		if (fds[f] < 0) {
			printf("Error opening file: %s\n", strerror(errno));
			return;
		}
		curr_open_end_time = __get_time();
		total_open_time_w += curr_open_end_time - curr_open_start_time;
	}

	// preselect some offsets
	int *offs = new int[iterations_per_sample];
	for (int i = 0; i < iterations_per_sample; i++) {
		if (random_flag)
			offs[i] = op_sz * __get_random_value(0, iterations_per_sample - 1);
		else
			offs[i] = op_sz * i;
		// printf("fsz=%lu, offs[i]=%d\n",
		// 	   __get_random_value(0, iterations_per_sample) % fsz, offs[i]);
		// printf("offs[i]: %d\n", offs[i]);
		// printf("op_sz: %d\n", op_sz);
		// printf("i: %d\n", i);
		// printf("fsz: %d\n", fsz);
	}

	// do writes
	double curr_writes_start_time = __get_time();
	int wret = 0;
	for (int f = 0; f < num_files; f++) {
		for (int i = 0; i < iterations_per_sample; i++) {
			// write_start_time =
			// 	std::chrono::time_point_cast<std::chrono::microseconds>(
			// 		std::chrono::high_resolution_clock::now())
			// 		.time_since_epoch()
			// 		.count();
			// printf("WRITE: sample [%d], iteration [%d]\n", sample, i);
			if ((wret = pwrite(fds[f], buf, write_sz, offs[i])) != write_sz) {
				printf("Error during write: %d bytes written, %s\n", wret,
					   strerror(errno));
				return;
			}
			// write_end_time =
			// 	std::chrono::time_point_cast<std::chrono::microseconds>(
			// 		std::chrono::high_resolution_clock::now())
			// 		.time_since_epoch()
			// 		.count();
			// // printf("Write latency: %.3fμs\n",
			// // 	   write_end_time - write_start_time);
			// write_latencies.push_back(write_end_time - write_start_time);
		}
	}
	double curr_writes_end_time = __get_time();
	total_write_time += (curr_writes_end_time - curr_writes_start_time);
	delete[] offs;

	// // close the files
	double curr_close_start_time = __get_time();
	for (int f = 0; f < num_files; f++) {
		if (close(fds[f]) < 0) {
			printf("Error closing file: %s\n", strerror(errno));
			return;
		}
	}
	double curr_close_end_time = __get_time();
	total_close_time_w += (curr_close_end_time - curr_close_start_time);

	// delete them for next sample
	// for (int f = 0; f < num_files; f++) {
	// 	fname = fstem + std::to_string(f);
	// 	if (remove((fstem + std::to_string(f)).c_str()) != 0) {
	// 		printf("Error deleting file\n");
	// 		exit(-1);
	// 	}
	// }
	delete buf;
}

void run_read(int sample) {
	char *buf = new char[op_sz]; // sized to read/write 1 raw eff block
	std::string fname;

	// open the files
	double curr_open_start_time, curr_open_end_time;
	// open the files we created in the write phase so we can measure the open
	// latency (so no O_CREAT flag)
	for (int f = 0; f < num_files; f++) {
		fname = fstem + std::to_string(f);

		curr_open_start_time = __get_time();
		// fds[f] = open(fname.c_str(),
		// 			  O_RDWR | O_CREAT | O_DIRECT | O_SYNC | O_TRUNC, 0777);
		fds[f] = open(fname.c_str(), O_RDWR, 0);
		if (fds[f] < 0) {
			printf("Error opening file: %s\n", strerror(errno));
			return;
		}
		curr_open_end_time = __get_time();
		total_open_time_r += curr_open_end_time - curr_open_start_time;

		// print file size
		struct stat st;
		if (fstat(fds[f], &st) != 0) {
			printf("Error getting file size: %s\n", strerror(errno));
			return;
		}
		// printf("File size: %ld\n", st.st_size);
	}

	// write data to new file
	// int wret = 0;
	// for (int f = 0; f < num_files; f++) {
	// 	for (int i = 0; i < iterations_per_sample; i++) {
	// 		if ((wret = pwrite(fds[f], buf, write_sz, op_sz * i)) != write_sz) {
	// 			printf("Error during prealloc write: %d bytes written, %s\n",
	// 				   wret, strerror(errno));
	// 			return;
	// 		}
	// 	}
	// }

	// preselect some offsets
	int *offs = new int[iterations_per_sample];
	for (int i = 0; i < iterations_per_sample; i++) {
		if (random_flag)
			offs[i] = op_sz * __get_random_value(0, iterations_per_sample - 1);
		else
			offs[i] = op_sz * i;
	}

	// do reads
	double curr_reads_start_time = __get_time();
	for (int f = 0; f < num_files; f++) {
		for (int i = 0; i < iterations_per_sample; i++) {
			// read_start_time =
			// 	std::chrono::time_point_cast<std::chrono::microseconds>(
			// 		std::chrono::high_resolution_clock::now())
			// 		.time_since_epoch()
			// 		.count();
			// printf("READ: sample [%d], iteration [%d]\n", sample, i);
			if (pread(fds[f], buf, read_sz, offs[i]) != read_sz) {
				printf("Error during read: %s\n", strerror(errno));
				return;
			}
			// read_end_time =
			// 	std::chrono::time_point_cast<std::chrono::microseconds>(
			// 		std::chrono::high_resolution_clock::now())
			// 		.time_since_epoch()
			// 		.count();
			// // printf("Read latency: %.3fμs\n", read_end_time -
			// // read_start_time);
			// read_latencies.push_back(read_end_time - read_start_time);
		}
	}
	double curr_reads_end_time = __get_time();
	total_read_time += (curr_reads_end_time - curr_reads_start_time);
	delete[] offs;

	// // close the files
	double curr_close_start_time = __get_time();
	for (int f = 0; f < num_files; f++) {
		if (close(fds[f]) < 0) {
			printf("Error closing file: %s\n", strerror(errno));
			return;
		}
	}
	double curr_close_end_time = __get_time();
	total_close_time_r += (curr_close_end_time - curr_close_start_time);

	// delete them for next sample
	// for (int f = 0; f < num_files; f++) {
	// 	fname = fstem + std::to_string(f);
	// 	if (remove((fstem + std::to_string(f)).c_str()) != 0) {
	// 		printf("Error deleting file\n");
	// 		exit(-1);
	// 	}
	// }
	delete buf;
}

// std::string vec_to_str(std::vector<double> &v) {
// 	std::ostringstream vstr;

// 	if (!v.empty()) {
// 		// dont add the trailing comma
// 		std::copy(v.begin(), v.end() - 1,
// 				  std::ostream_iterator<double>(vstr, ", "));
// 		vstr << v.back();
// 	}

// 	return vstr.str();
// }
