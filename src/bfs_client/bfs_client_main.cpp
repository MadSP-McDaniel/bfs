/**
 * @file bfs_client_main.cpp
 * @brief Entry point for the client executable.
 */

#include "bfs_client.h"
#include <bfs_common.h>
#include <bfs_log.h>
#include <cstdlib>

/**
 * @brief Entry point for the bfs client. Initializes things then passes the
 * other arguments to the main FUSE entry point.
 *
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int main(int argc, char *argv[]) {
	if (client_init() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to init client\n");
		return BFS_FAILURE;
	}

	return fuse_main(argc, argv, &bfs_oper, NULL);
}
