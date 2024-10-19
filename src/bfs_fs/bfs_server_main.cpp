/**
 * @file bfs_server_main.cpp
 * @brief Entry point for bfs server executable.
 */

#include <cstdlib>
#include <unistd.h>

#include "bfs_server.h"
#include <bfs_common.h>
#include <bfs_log.h>

/**
 * @brief Entry point to the bfs server. Initializes the log then calls
 * start_server to begin the main event loop for the server.
 *
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;
	// TODO: validate call chain:
	// utils >> comms >> device >> block >> fs (note that block and fs should be
	// init inside the enclave, which can be invoked on init)

	if (server_init() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to init server\n");
		abort();
	}

	return start_server();
}
