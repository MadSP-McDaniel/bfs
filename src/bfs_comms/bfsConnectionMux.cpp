/**
 * @file bfsConnectionMux.cpp
 * @brief This is the connection multiplexer implementation.
 */

/* Include files  */
#ifdef __BFS_ENCLAVE_MODE
#include "bfs_enclave_t.h" /* For ocalls */
#include "sgx_trts.h"
#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <vector>

#include <bfsConnectionMux.h>
#include <bfs_log.h>

int64_t num_file_worker_threads = -1;

/**
 * @brief The constructor for the class
 *
 * @param none
 * @return none
 */

bfsConnectionMux::bfsConnectionMux(void) {

	// Return, no return code
	return;
}

/**
 * @brief The deconstructor for the class
 *
 * @param none
 * @return none
 */
bfsConnectionMux::~bfsConnectionMux(void) {

	// Return, no return code
	return;
}

/**
 * @brief Wait for incoming data on all of the connections.
 *
 * @param dready - list to put connections with activity
 * @param wait - the length of time to wai
 * @return 0 if successful, -1 if failure
 */

int bfsConnectionMux::waitConnections(bfsConnectionList &dready, uint16_t wt) {
#ifdef __BFS_ENCLAVE_MODE
	int ret;
	uint64_t socks_count = connections.size();

	// uint32_t *ready_socks = (uint32_t *)calloc(
	//     sizeof(uint32_t), socks_count); // will point to nonenclave memory
	// uint32_t *ready_socks = NULL;
	uint32_t ready_cnt = 0;

	// serialize the socket list so we can do a select on them
	int32_t *all_socks = (int32_t *)calloc(sizeof(int32_t), socks_count);
	uint32_t ix = 0;
	for (auto itc = connections.begin(); itc != connections.end(); itc++) {
		all_socks[ix] = itc->first;
		ix++;
	}

    // unused
	// ocall_waitConnections(&ret, wt, socks_count, all_socks, ready_cnt,
	//                       &ready_socks);
	ocall_waitConnections(&ret, wt, socks_count, all_socks, &ready_cnt);

	// Check the return value
	if (ret == -1) {
		logMessage(LOG_ERROR_LEVEL, "MUX select() failed.");
		return (-1);
	}

#else
	// Local variables
	fd_set rfds;
	int nfds, ret;
	bfsConnectionList::iterator it;
	struct timeval wait;

	// Setup and perform the select
	FD_ZERO(&rfds);
	nfds = 0;
	for (it = connections.begin(); it != connections.end(); it++) {
		if (it->first >= nfds) {
			nfds = it->first + 1;
		}
		FD_SET(it->first, &rfds);
	}

	// Do the select, wait or not
	if (wt > 0) {
		wait.tv_sec = (int)wt / 1000;
		wait.tv_usec = (wt % 1000) * 1000;
		ret = select(nfds, &rfds, NULL, NULL, &wait);
	} else {
		ret = select(nfds, &rfds, NULL, NULL, NULL);
	}

	// Check the return value
	if (ret == -1) {
		logMessage(LOG_ERROR_LEVEL, "MUX select() failed : [%s]",
				   strerror(errno));
		return (-1);
	}
#endif

	// Clean out the data ready, add the data to the read
	dready.clear();

#ifdef __BFS_ENCLAVE_MODE
	bfsConnectionList::iterator it;
	for (uint32_t i = 0; i < ready_cnt; i++) {
		it = connections.find(all_socks[i]);
		if (it != connections.end()) {
			dready[it->first] = it->second;
		} else {
			logMessage(LOG_ERROR_LEVEL,
					   "MUX select() returned invalid socket descriptor: [%d]",
					   it->first);
			return (-1);
		}
	}
#else
	for (it = connections.begin(); it != connections.end(); it++) {
		if (FD_ISSET(it->first, &rfds)) {
			dready[it->first] = it->second;
		}
	}
#endif

	// Return succesfully
	return (0);
}

/**
 * @brief Cleanup (and free) all of the connections in the MUX
 *
 * @param none
 * @return 0 if successful, -1 if failure
 */

int bfsConnectionMux::cleanup(void) {

	// Local variables
	bfsConnectionList::iterator it;
	bfsNetworkConnection *con;

	// Keep cleaning up the connections and freeing them
	while (!connections.empty()) {
		it = connections.begin();
		it->second->disconnect();
		con = it->second;
		connections.erase(it);
		delete con;
	}

	// Return successfully
	return (0);
}
