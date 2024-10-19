/**
 * @file bfs_server_ocalls.cpp
 * @brief Provides support for the server. Mainly used for defining the
 * enclave->server interface for ocalls.
 */

#include "bfs_server_ocalls.h"
#include "bfs_server.h"
#include <bfsConnectionMux.h>
#include <bfs_common.h>
#include <bfs_log.h>

/**
 * @brief This method handles sending messages originating from the enclave over
 * the connection it received a message from in the first place. This is mainly
 * used to send response messages to client requests (ie read, write), while
 * messages to physical block devices is handled by the device/comms ocalls. In
 * contrast to the device connections (where the state is stored inside enclave
 * structures), here the state at the server of the client channels is stored in
 * open memory (singe the server needs to be aware of the open connections), so
 * we use this ocall to prevent vtable issues when trying to call object methods
 * (e.g., send).
 *
 * @param client_conn_ptr: pointer to the network connection object
 * @param spkt_ptr: the packet to send
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int32_t ocall_handle_out_msg(void *client_conn_ptr, uint32_t buf_len,
							 char *spkt_enc) {
	if (bfsUtilLayer::perf_test())
		net_c_send_start_time =
			(double)std::chrono::time_point_cast<std::chrono::microseconds>(
				std::chrono::high_resolution_clock::now())
				.time_since_epoch()
				.count();

	// single threaded; handle the request directly in the (1) main thread for
	// single-threaded, or (2) worker thread for multi-threaded.
	if (static_cast<bfsNetworkConnection *>(client_conn_ptr)
			->sendPacketizedDataL(buf_len, spkt_enc) == -1) {
		logMessage(LOG_ERROR_LEVEL, "Failure during ocall_handle_out_msg.\n");
		return BFS_FAILURE;
	}

	// Note: putting the timers here might end up showing the server net_send
	// latencies as faster than the enclave (not using for plots however)
	if (bfsUtilLayer::perf_test())
		net_c_send_end_time =
			(double)std::chrono::time_point_cast<std::chrono::microseconds>(
				std::chrono::high_resolution_clock::now())
				.time_since_epoch()
				.count();

	return BFS_SUCCESS;
}
