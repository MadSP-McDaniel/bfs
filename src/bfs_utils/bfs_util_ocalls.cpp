/**
 * @file bfs_util_ocalls.cpp
 * @brief Provides support for the server. Mainly used for defining the
 * enclave->server interface for ocalls.
 */

#include <cassert>
#include <chrono>
#include <ctime>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

#include "bfs_util_ocalls.h"
#include <bfs_common.h>

/**
 * @brief Bridge function to print messages from the enclave. Only needed for
 * enclave mode modules, so the prototype is defined in the enclave_t.h header
 * when certain compile flags are given. Otherwise, non-enclave modules do not
 * use it.
 *
 * @param msg: The message to print
 * @return int64_t: 0 if success, 1 if failure
 */
int64_t ocall_printf(int32_t handle, uint32_t msg_len, char *msg) {
	int64_t ret = write(handle, msg, msg_len);
	return ret;
}

/**
 * @brief Allows the enclave to get the time. Note: not used for critical
 * operations, only for logging.
 *
 * @return int64_t: 0 if success, 1 if failure
 */
int64_t ocall_getTime(uint64_t buf_len, char *tbuf) {
	time_t tm;
	char *buf;
	(void)buf_len;

	time(&tm);

	if (!(buf = ctime((const time_t *)&tm)))
		return BFS_FAILURE;

	memcpy(tbuf, buf, strlen(buf) + 1);

	return BFS_SUCCESS;
}

double ocall_get_time2() {
	auto now = std::chrono::high_resolution_clock::now();
	return (double)std::chrono::time_point_cast<std::chrono::microseconds>(now)
		.time_since_epoch()
		.count();
}

int ocall_write_to_file(uint32_t fname_len, const char *fname, uint32_t buf_len,
						const char *buf) {
	(void)fname_len;
	(void)buf_len;
	std::string __e__lats_fname = std::string(getenv("BFS_HOME")) +
								  "/benchmarks/micro/output/" +
								  std::string(fname) + ".csv";
	std::ofstream __e__lats_f;
	__e__lats_f.open(__e__lats_fname.c_str(), std::ios::trunc);
	__e__lats_f << buf;
	__e__lats_f.close();

	return BFS_SUCCESS;
}

int32_t ocall_openLog(char *fname, uint32_t namelen) {
	int32_t fd = 0;
	assert(strlen(fname) <= namelen - 1);
	if ((fd = open(fname, O_APPEND | O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR)) ==
		-1) {
		// Error out
		// errored = 1;
		return -1;
		fprintf(stderr, "Error opening log in ocall: %s (%s)", fname,
				strerror(errno));
	}
	return fd;
}

int64_t ocall_closeLog(int32_t fd) { return close(fd); }

int64_t ocall_do_alloc(uint32_t newsz) { return (int64_t)(new char[newsz]); }

void ocall_delete_allocation(int64_t ptr) { delete[](char *) ptr; }
