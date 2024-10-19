/**
 * @file bfs_client.cpp
 * @brief Definitions for the bfs_client FUSE hooks and helpers. Defines the
 * interface used by the fuse file system for communicating over the network to
 * the file server (enclave).
 */

// #include <arpa/inet.h>
// #include <cassert>
// #include <chrono>
// #include <cstddef>
// #include <cstdlib>
// #include <cstring>
// #include <dirent.h>
// #include <errno.h>
// #include <fcntl.h>
// #include <fstream>
// #include <iostream>
// #include <libgen.h>
// #include <string>
// #include <sys/socket.h>
// #include <sys/stat.h>
// #include <sys/time.h>
// #include <unistd.h>

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

#include "bfs_client.h"
#include <bfsConfigLayer.h>
// #include <bfsConnectionMux.h>
// #include <bfsCryptoError.h>
// #include <bfsNetworkConnection.h>
// #include <bfsSecAssociation.h>
// #include <bfsUtilError.h>
// #include <bfsUtilLayer.h>
// #include <bfs_common.h>
// #include <bfs_core.h>
// #include <bfs_log.h>
// #include <bfs_server.h>
// #include <bfs_util.h>

// #define CLIENT_TEST_LOG_LEVEL bfs_client_test_log_level
// #define CLIENT_TEST_VRB_LOG_LEVEL bfs_client_test_vrb_log_level
// #define CLIENT_TEST_CONFIG "bfsClientLayerTest"

// static uint64_t bfs_client_test_log_level = 0;
// static uint64_t bfs_client_test_vrb_log_level = 0;

// test parameters

static bool write_results = true;

static std::vector<double> read_latencies;
static std::vector<double> write_latencies;
double read_start_time, read_end_time, write_start_time, write_end_time;
static std::string vec_to_str(std::vector<double> &);
void run_write(int);
void run_read(int);

const int num_samples = 25;
const int num_files = 1;

const int fsz = 1048576;
// const int fsz = 1039872;

const int iterations_per_sample = 8;
const int op_sz = fsz / iterations_per_sample;
const int write_sz = op_sz, read_sz = op_sz;

struct fuse_file_info fi[num_files];

// static int bfs_unit__bfs_client_init() {
// 	bfsCfgItem *config;
// 	bool fstlog, fstvlog, log_to_file;
// 	std::string logfile;

// 	try {
// 		if (bfsUtilLayer::bfsUtilLayerInit() != BFS_SUCCESS) {
// 			logMessage(LOG_ERROR_LEVEL, "Failed initialized util layer");
// 			return BFS_FAILURE;
// 		}

// 		config = bfsConfigLayer::getConfigItem(CLIENT_TEST_CONFIG);

// 		if (config->bfsCfgItemType() != bfsCfgItem_STRUCT) {
// 			logMessage(LOG_ERROR_LEVEL,
// 					   "Unable to find configuration in system config: %s",
// 					   CLIENT_TEST_CONFIG);
// 			return BFS_FAILURE;
// 		}

// 		fstlog = (config->getSubItemByName("log_enabled")->bfsCfgItemValue() ==
// 				  "true");
// 		bfs_client_test_log_level =
// 			registerLogLevel("CLIENT_TEST_LOG_LEVEL", fstlog);
// 		fstvlog = (config->getSubItemByName("log_verbose")->bfsCfgItemValue() ==
// 				   "true");
// 		bfs_client_test_vrb_log_level =
// 			registerLogLevel("CLIENT_TEST_VRB_LOG_LEVEL", fstvlog);
// 		log_to_file =
// 			(config->getSubItemByName("log_to_file")->bfsCfgItemValue() ==
// 			 "true");

// 		if (log_to_file) {
// 			logfile = config->getSubItemByName("logfile")->bfsCfgItemValue();
// 			initializeLogWithFilename(logfile.c_str());
// 		} else {
// 			initializeLogWithFilehandle(STDOUT_FILENO);
// 		}

// 		logMessage(CLIENT_TEST_LOG_LEVEL, "Client test initialized.");
// 	} catch (bfsCfgError *e) {
// 		logMessage(LOG_ERROR_LEVEL, "Failure reading system config: %s",
// 				   e->getMessage().c_str());
// 		return BFS_FAILURE;
// 	}

// 	return BFS_SUCCESS;
// }

/**
 * @brief Unit test for bfs file operation methods from client.
 *
 * @return uint32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static uint32_t bfs_unit__bfs_client() {
	// if (bfs_unit__bfs_client_init() != BFS_SUCCESS) {
	// 	logMessage(LOG_ERROR_LEVEL,
	// 			   "Error during bfs_unit__bfs_client_init.\n");
	// 	return BFS_FAILURE;
	// }

	if (client_init() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed client_init\n");
		return BFS_FAILURE;
	}

	if (bfs_init(NULL, NULL) != NULL) {
		logMessage(LOG_ERROR_LEVEL, "Failed bfs_init\n");
		return BFS_FAILURE;
	}

	if (!bfsConfigLayer::systemConfigLoaded()) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed to load system configuration, aborting.\n");
		return BFS_FAILURE;
	}

	std::string read_lats_fname, write_lats_fname;
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
		if (bfs_create((std::string("/") + std::to_string(f)).c_str(), 0777,
					   &fi[f]) != BFS_SUCCESS)
			return BFS_FAILURE;
	}

	double all_writes_start_time = 0.0, all_writes_end_time = 0.0,
		   total_write_time = 0.0, all_reads_start_time = 0.0,
		   all_reads_end_time = 0.0, total_read_time = 0.0;

	// run several samples from start to finish
	all_writes_start_time =
		(double)std::chrono::time_point_cast<std::chrono::microseconds>(
			std::chrono::high_resolution_clock::now())
			.time_since_epoch()
			.count();

	for (int s = 0; s < num_samples; s++)
		run_write(s);

	all_writes_end_time =
		(double)std::chrono::time_point_cast<std::chrono::microseconds>(
			std::chrono::high_resolution_clock::now())
			.time_since_epoch()
			.count();
	total_write_time = all_writes_end_time - all_writes_start_time;

	all_reads_start_time =
		(double)std::chrono::time_point_cast<std::chrono::microseconds>(
			std::chrono::high_resolution_clock::now())
			.time_since_epoch()
			.count();

	for (int s = 0; s < num_samples; s++)
		run_read(s);

	all_reads_end_time =
		(double)std::chrono::time_point_cast<std::chrono::microseconds>(
			std::chrono::high_resolution_clock::now())
			.time_since_epoch()
			.count();
	total_read_time = all_reads_end_time - all_reads_start_time;

	double total_MB =
		(op_sz * num_samples * num_files * iterations_per_sample) / 1e6;

	printf("Read throughput: (%.3f MB / %.3f s) %.3f MB/s\n", total_MB,
		   total_read_time / 1e6, total_MB / (total_read_time / 1e6));
	printf("Write throughput: (%.3f MB / %.3f s) %.3f MB/s\n", total_MB,
		   total_write_time / 1e6, total_MB / (total_write_time / 1e6));

	// close the files
	for (int f = 0; f < num_files; f++)
		bfs_release(NULL, &fi[f]);

	if (write_results) {
		std::string read_lats = vec_to_str(read_latencies);
		read_lats_f << read_lats.c_str();
		// printf("Read latencies (us, %lu records):\n[%s]\n",
		// 	   read_latencies.size(), read_lats.c_str());

		// log and write the results to a file
		std::string write_lats = vec_to_str(write_latencies);
		write_lats_f << write_lats.c_str();
		// printf("Write latencies (us, %lu records):\n[%s]\n",
		// 	   write_latencies.size(), write_lats.c_str());

		read_lats_f.close();
		write_lats_f.close();
	}

	return BFS_SUCCESS;
}

void run_write(int sample) {
	(void)sample;
	char buf[op_sz] = {0}; // sized to read/write 1 raw eff block

	// do sequential writes
	for (int f = 0; f < num_files; f++) {
		for (int i = 0; i < iterations_per_sample; i++) {
			write_start_time =
				(double)std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();
			// printf("WRITE: sample [%d], iteration [%d]\n", sample, i);
			// if ((ret = pwrite(fds[f], buf, write_sz, op_sz * i)) != write_sz)
			// { 	printf("Error during write: %s\n", strerror(errno));
			// return;
			// }
			if (bfs_write(NULL, buf, write_sz, op_sz * i, &fi[f]) != write_sz) {
				printf("Error during write: %s\n", strerror(errno));
				return;
			}
			write_end_time =
				(double)std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();
			// printf("Write latency: %.3fμs\n",
			// 	   write_end_time - write_start_time);
			write_latencies.push_back(write_end_time - write_start_time);
		}
	}
}

void run_read(int sample) {
	(void)sample;
	char buf[op_sz] = {0}; // sized to read/write 1 raw eff block

	// do sequential reads
	for (int f = 0; f < num_files; f++) {
		for (int i = 0; i < iterations_per_sample; i++) {
			read_start_time =
				(double)std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();
			// printf("READ: sample [%d], iteration [%d]\n", sample, i);
			// if (pread(fds[f], buf, read_sz, op_sz * i) != read_sz) {
			// 	printf("Error during read: %s\n", strerror(errno));
			// 	return;
			// }
			if (bfs_read(NULL, buf, read_sz, op_sz * i, &fi[f]) != read_sz) {
				printf("Error during write: %s\n", strerror(errno));
				return;
			}
			read_end_time =
				(double)std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();
			// printf("Read latency: %.3fμs\n", read_end_time -
			// read_start_time);
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

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;
	return bfs_unit__bfs_client();
}
