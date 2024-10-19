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
static bool write_results = true;

static std::vector<double> read_latencies;
static std::vector<double> write_latencies;
double read_start_time, read_end_time, write_start_time, write_end_time;
static std::string vec_to_str(std::vector<double> &);
void run_write(int);
void run_read(int);

const int num_samples = 1, iterations_per_sample = 500;
const int op_sz = 4062, num_files = 1;
const int write_sz = op_sz, read_sz = op_sz;

// const std::string fstem = "/tmp/nfs/mp/";
const std::string fstem = "/tmp/bfs/mp/";

int fds[num_files] = {0};

int main(int argc, char *argv[]) {
	std::string fname, read_lats_fname, write_lats_fname;
	std::ofstream read_lats_f, write_lats_f;

	if (write_results) {
		read_lats_fname = std::string(getenv("BFS_HOME"));
		read_lats_fname += "/benchmarks/micro/output/read_lats.csv";
		read_lats_f.open(read_lats_fname.c_str(), std::ios::trunc);

		write_lats_fname = std::string(getenv("BFS_HOME"));
		write_lats_fname += "/benchmarks/micro/output/write_lats.csv";
		write_lats_f.open(write_lats_fname.c_str(), std::ios::trunc);
	}

	// open the files
	for (int f = 0; f < num_files; f++) {
		fname = fstem + std::to_string(f);
		fds[f] = open(fname.c_str(), O_RDWR | O_CREAT | O_DIRECT | O_SYNC, 0777);
	}

	double all_writes_start_time = 0.0, all_writes_end_time = 0.0,
		   total_write_time = 0.0, all_reads_start_time = 0.0,
		   all_reads_end_time = 0.0, total_read_time = 0.0;

	// run several samples from start to finish
	all_writes_start_time =
		std::chrono::time_point_cast<std::chrono::microseconds>(
			std::chrono::high_resolution_clock::now())
			.time_since_epoch()
			.count();

	for (int s = 0; s < num_samples; s++)
		run_write(s);

	all_writes_end_time =
		std::chrono::time_point_cast<std::chrono::microseconds>(
			std::chrono::high_resolution_clock::now())
			.time_since_epoch()
			.count();
	total_write_time = all_writes_end_time - all_writes_start_time;

	all_reads_start_time =
		std::chrono::time_point_cast<std::chrono::microseconds>(
			std::chrono::high_resolution_clock::now())
			.time_since_epoch()
			.count();

	for (int s = 0; s < num_samples; s++)
		run_read(s);

	all_reads_end_time =
		std::chrono::time_point_cast<std::chrono::microseconds>(
			std::chrono::high_resolution_clock::now())
			.time_since_epoch()
			.count();
	total_read_time = all_reads_end_time - all_reads_start_time;

	double total_KB =
		(op_sz * num_samples * num_files * iterations_per_sample) / 1e3;

	printf("Write throughput: (%.3f KB / %.3f s) %.3f KB/s\n", total_KB,
		   total_write_time / 1e6, total_KB / (total_write_time / 1e6));
	printf("Read throughput: (%.3f KB / %.3f s) %.3f KB/s\n", total_KB,
		   total_read_time / 1e6, total_KB / (total_read_time / 1e6));

	// close the files
	for (int f = 0; f < num_files; f++)
		close(fds[f]);

	if (write_results) {
		std::string read_lats = vec_to_str(read_latencies);
		read_lats_f << read_lats.c_str();
		printf("Read latencies (us, %lu records):\n[%s]\n",
			   read_latencies.size(), read_lats.c_str());

		// log and write the results to a file
		std::string write_lats = vec_to_str(write_latencies);
		write_lats_f << write_lats.c_str();
		printf("Write latencies (us, %lu records):\n[%s]\n",
			   write_latencies.size(), write_lats.c_str());

		read_lats_f.close();
		write_lats_f.close();
	}

	return 0;
}

void run_write(int sample) {
	char buf[op_sz] = {0}; // sized to read/write 1 raw eff block

	// do sequential writes
	for (int f = 0; f < num_files; f++) {
		for (int i = 0; i < iterations_per_sample; i++) {
			write_start_time =
				std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();
			printf("WRITE: sample [%d], iteration [%d]\n", sample, i);
			if (pwrite(fds[f], buf, write_sz, op_sz * i) != write_sz) {
				printf("Error during write: %s\n", strerror(errno));
				return;
			}
			write_end_time =
				std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();
			write_latencies.push_back(write_end_time - write_start_time);
		}
	}
}

void run_read(int sample) {
	char buf[op_sz] = {0}; // sized to read/write 1 raw eff block

	// do sequential reads
	for (int f = 0; f < num_files; f++) {
		for (int i = 0; i < iterations_per_sample; i++) {
			read_start_time =
				std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();
			printf("READ: sample [%d], iteration [%d]\n", sample, i);
			if (pread(fds[f], buf, read_sz, op_sz * i) != read_sz) {
				printf("Error during read: %s\n", strerror(errno));
				return;
			}
			read_end_time =
				std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();
			read_latencies.push_back(read_end_time - read_start_time);
		}
	}
}

std::string vec_to_str(std::vector<double> &v) {
	std::ostringstream vstr;

	if (!v.empty()) {
		// dont add the trailing comma
		std::copy(v.begin(), v.end() - 1,
				  std::ostream_iterator<double>(vstr, ", "));
		vstr << v.back();
	}

	return vstr.str();
}
