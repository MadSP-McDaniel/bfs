/**
 * @file bfs_server.cpp
 * @brief Definitions for the bfs server interface methods and helpers. This
 * represents part of the non-enclave component of bfs, simply using the network
 * to hand encrypted messages between clients and the actual bfs enclave.
 * Supports multithreading in a worker-thread model.
 */

#include <cstring>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <utility>

#include "bfs_fs_layer.h"
#include "bfs_server.h"
#include <bfsConfigLayer.h>
#include <bfs_acl.h>
#include <bfs_common.h>
#include <bfs_log.h>
#include <bfs_util.h>

#ifdef __BFS_DEBUG_NO_ENCLAVE
#include <arpa/inet.h>
#include <sys/socket.h>
/* For non-enclave testing; just directly calls the ecall functions */
#include "bfs_server_ecalls.h"
#elif defined(__BFS_NONENCLAVE_MODE)
#include <arpa/inet.h>
#include <sys/socket.h>
/* For making legitimate ecalls */
#include <bfs_enclave_u.h>
#include <sgx_urts.h>
static sgx_enclave_id_t eid = 0;
#endif

static int bfs_server_listener_status = 0;
uint64_t bfs_server_log_level = 0, bfs_server_vrb_log_level = 0;
static std::list<pthread_t *> client_worker_threads;
static unsigned short bfs_server_port = -1;

/* For performance testing */
static std::vector<long> s_read__lats, s_read__s_lats, s_read__net_c_send_lats,
	s_read__net_recv_lats, s_write__lats, s_write__s_lats,
	s_write__net_c_send_lats, s_write__net_recv_lats;
double net_c_send_start_time = 0.0,
	   net_c_send_end_time = 0.0; // make global so the ocall can track it
static void write_server_latencies();

static void *client_worker_entry(void *);
static int start_dispatcher();
static int64_t handle_in_msg(bfsNetworkConnection *, bfsFlexibleBuffer *);
static void server_signal_handler(int);

/**
 * @brief Initializes a new enclave for the file system then starts the main
 * event loop for bfs server.
 *
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int start_server() {
	int ret = -1;

#ifdef __BFS_NONENCLAVE_MODE
	sgx_launch_token_t tok = {0};
	int tok_updated = 0;
	if (sgx_create_enclave(
			(std::string(getenv("BFS_HOME")) + std::string("/build/bin/") +
			 std::string(BFS_CORE_ENCLAVE_FILE))
				.c_str(),
			SGX_DEBUG_FLAG, &tok, &tok_updated, &eid, NULL) != SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to initialize enclave.");
		return BFS_FAILURE;
	}
	logMessage(SERVER_LOG_LEVEL,
			   "Enclave successfully initialized. Starting bfs server...\n");

	// // Initialize the (enclave-mode) access control layer from the main
	// thread
	// // before beginning to accept new incoming client connections and
	// requests. int64_t ecall_status = 0; if (((ecall_status =
	// ecall_enclave_init(eid, &ret)) != SGX_SUCCESS) || 	(ret !=
	// BFS_SUCCESS))
	// { 	logMessage(LOG_ERROR_LEVEL, 			   "Failed during
	// ecall_enclave_init. " "Error code: %d\n", 			   ret); 	return
	// BFS_FAILURE;
	// }
#else
	// if ((ret = ecall_enclave_init()) != BFS_SUCCESS) {
	// 	logMessage(LOG_ERROR_LEVEL,
	// 			   "Failed during ecall_enclave_init (debug). "
	// 			   "Error code: %d\n",
	// 			   ret);
	// 	return BFS_FAILURE;
	// }
#endif

	// Register signal for the server before starting threads
	struct sigaction new_action;
	memset(&new_action, 0x0, sizeof(struct sigaction));
	new_action.sa_handler = server_signal_handler;
	new_action.sa_flags = SA_NODEFER | SA_ONSTACK;
	sigaction(SIGINT, &new_action, NULL);

	/* set server status OK for workers to watch */
	bfs_server_listener_status = 1;

	// If the config indicates there are worker threads, we assume the server
	// should be multithreaded. Otherwise the server is single threaded.

	logMessage(SERVER_LOG_LEVEL, "Server initialization OK.");

	if ((ret = start_dispatcher()) != BFS_SUCCESS)
		logMessage(LOG_ERROR_LEVEL, "bfs failed during main loop\n");

	/* set server status DONE for workers to begin cleanup */
	bfs_server_listener_status = 0;

	// make sure client workers are done
	if (num_file_worker_threads > 0) {
		for (auto ct : client_worker_threads) {
			logMessage(SERVER_LOG_LEVEL,
					   "Waiting for client worker thread [%lu] to complete ...",
					   ct);
			pthread_join(*ct, NULL);
			delete ct;
		}
	}

	// then make sure file workers are done
	// if (num_file_worker_threads > 0) {
	// 	for (int i = 0; i < num_file_worker_threads; i++) {
	// 		logMessage(SERVER_LOG_LEVEL,
	// 				   "Waiting for file worker thread [%d] to complete ...",
	// 				   i);
	// 		pthread_join(file_worker_threads[i], NULL);
	// 	}

	// 	delete[] file_worker_threads;
	// }

#ifdef __BFS_NONENCLAVE_MODE
	sgx_status_t enclave_status = SGX_SUCCESS;
	if ((enclave_status = sgx_destroy_enclave(eid)) != SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to destroy enclave: %d\n",
				   enclave_status);
		return BFS_FAILURE;
	}

	logMessage(SERVER_LOG_LEVEL, "Destroyed enclave successfully.\n");
#endif

	logMessage(SERVER_LOG_LEVEL, "Server shut down complete.");

	return BFS_SUCCESS;
}

/**
 * @brief Signal handler for gracefully shutting down the server.
 *
 * @param no: the signal number
 */
void server_signal_handler(int no) {
	(void)no;
	bfs_server_listener_status = 0;
}

/**
 * @brief Initializes utils, config, comms (doesn't really have an init), and
 * crypto for nonenclave code of the server.
 *
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int server_init() {
	bfsCfgItem *config;
	bool srvlog, vrblog, log_to_file;
	std::string logfile;

	try {
		// if (bfsBlockLayer::bfsBlockLayerInit() != BFS_SUCCESS) {
		// 	logMessage(LOG_ERROR_LEVEL, "Failed bfsBlockLayerInit\n");
		// 	return BFS_FAILURE;
		// }

		if (bfsUtilLayer::bfsUtilLayerInit() != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL, "Failed bfsUtilLayerInit\n");
			return BFS_FAILURE;
		}

		config = bfsConfigLayer::getConfigItem(BFS_SERVER_CONFIG);

		if (config->bfsCfgItemType() != bfsCfgItem_STRUCT) {
			logMessage(LOG_ERROR_LEVEL,
					   "Unable to find configuration in system config: %s",
					   BFS_SERVER_CONFIG);
			return BFS_FAILURE;
		}

		srvlog = (config->getSubItemByName("log_enabled")->bfsCfgItemValue() ==
				  "true");
		bfs_server_log_level = registerLogLevel("SERVER_LOG_LEVEL", srvlog);
		vrblog = (config->getSubItemByName("log_verbose")->bfsCfgItemValue() ==
				  "true");
		bfs_server_vrb_log_level =
			registerLogLevel("SERVER_VRB_LOG_LEVEL", vrblog);
		log_to_file =
			(config->getSubItemByName("log_to_file")->bfsCfgItemValue() ==
			 "true");

		if (log_to_file) {
			logfile = config->getSubItemByName("logfile")->bfsCfgItemValue();
			initializeLogWithFilename(logfile.c_str());
		} else {
			initializeLogWithFilehandle(STDOUT_FILENO);
		}

		bfs_server_port =
			(unsigned short)config->getSubItemByName("bfs_server_port")
				->bfsCfgItemValueLong();

		num_file_worker_threads =
			config->getSubItemByName("num_file_worker_threads")
				->bfsCfgItemValueLong();

	} catch (bfsCfgError *e) {
		logMessage(LOG_ERROR_LEVEL, "Failure reading system config: %s",
				   e->getMessage().c_str());
		return BFS_FAILURE;
	}

	return BFS_SUCCESS;
}

/**
 * @brief Entry point for client-worker thread. It waits for client messages on
 * the socket and handles the requests/responses inline (through ecalls and
 * ocalls).
 *
 * @param arg: client connection pointer from dispatcher
 * @return void*: unused
 */
void *client_worker_entry(void *arg) {
	bfsNetworkConnection *client = (bfsNetworkConnection *)arg;
	bfsConnectionMux *mux;
	bfsConnectionList ready;
	bfsConnectionList::iterator it;
	bfsFlexibleBuffer rpkt_enc;
	int32_t rlen; // should be signed type for detecting short buffer error
	bool done;
	mux = new bfsConnectionMux();
	mux->addConnection(client);

	// use same ecall for initial connection setup, in place
	// of ecall_add_user_context
	int64_t handler_rc = BFS_FAILURE;
	if ((handler_rc = handle_in_msg(client, NULL)) == BFS_FAILURE) {
		logMessage(LOG_ERROR_LEVEL, "Error handling client initial connect.\n");
		abort();
	}

	done = false;
	while (!done) {
		// check if we should try to handle any new requests (inline or
		// dispatch) or begin cleanup
		if (!bfs_server_listener_status) {
			logMessage(LOG_ERROR_LEVEL, "Server encountered fatal failure, "
										"shutting down client worker.");
			break;
		}

		// Wait for incoming data
		if (mux->waitConnections(ready, 0)) {
			logMessage(LOG_ERROR_LEVEL, "Mux wait failed, aborting");
			break;
		}

		for (it = ready.begin(); it != ready.end(); it++) {
			if (it->second->getType() == SCH_CLIENT) {
				client = it->second;
				rlen = client->recvPacketizedBuffer(rpkt_enc);
				if (!((rlen > 0) &&
					  (handle_in_msg(client, &rpkt_enc) == BFS_SUCCESS))) {
					logMessage(LOG_ERROR_LEVEL,
							   "Failed while receiving/handling client "
							   "request: connection [%d], rlen=%d\n",
							   it->first, rlen);
					mux->removeConnection(client);
					delete client;
					done = true;
					break;
				}
				logMessage(SERVER_VRB_LOG_LEVEL,
						   "Received [%d] bytes on connection [%d]", rlen,
						   it->first);
			} else {
				logMessage(LOG_ERROR_LEVEL, "Weird socket, aborting");
				done = true;
				break;
			}
		}
	}

	delete mux;

	logMessage(SERVER_LOG_LEVEL, "Client worker shutting down.");

	return NULL;
}

/**
 * @brief Starts the main thread server loop that listens for new connections
 * (fs/bdev clients) or inbound messages. (Eventually will hook nfs/afs/smb
 * server to this entry point to listen/receive connections/requests.)
 *
 * @return int BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int start_dispatcher() {
	bfsNetworkConnection *server, *client;
	pthread_t *client_thread;
	bfsConnectionMux *mux;
	bfsConnectionList ready;
	bfsConnectionList::iterator it;
	bfsFlexibleBuffer rpkt_enc;
	int32_t rlen; // should be signed for detecting short buffer error
	bool done;
	int64_t handler_rc;
	// long t1, t2;
	// double s_start_time = 0.0, s_end_time = 0.0, net_recv_start_time = 0.0,
	// 	   net_recv_end_time = 0.0;

	// Create the network and mux objects
	server = bfsNetworkConnection::bfsChannelFactory(bfs_server_port);
	mux = new bfsConnectionMux();

	// Now connect the server, then add to mux
	if (server->connect()) {
		logMessage(LOG_ERROR_LEVEL, "Server connection failed, aborting.");
		return BFS_FAILURE;
	}
	mux->addConnection(server);
	logMessage(SERVER_LOG_LEVEL, "Server listening on [%d]\n", bfs_server_port);

	// Now keep listening to sockets until you are done
	done = false;
	while (!done) {
		// check if we should try to handle any new requests (inline or
		// dispatch) or begin cleanup
		if (!bfs_server_listener_status) {
			// done = true;
			break;
		}

		// Wait for incoming data
		if (mux->waitConnections(ready, 0)) {
			logMessage(LOG_ERROR_LEVEL, "Mux wait failed, aborting");
			break;
		}

		// Walk the list of sockets (which have data/processing)
		for (it = ready.begin(); it != ready.end(); it++) {
			if (num_file_worker_threads > 0) {
				// multi-threaded
				if (it->second->getType() == SCH_SERVER) {
					if (!bfs_server_listener_status) {
						done = true;
						break;
					}

					if ((client = it->second->accept()) != NULL) {

						// #ifdef __BFS_NONENCLAVE_MODE
						// 						int64_t ecall_status = 0;
						// 						if (((ecall_status =
						// ecall_add_user_context( eid, client->getSocket())) !=
						// SGX_SUCCESS)) { logMessage(LOG_ERROR_LEVEL, "Failed
						// during ecall_add_user_context. " "Error code: %d\n",
						// ecall_status); 							return
						// BFS_FAILURE;
						// 						}
						// #else
						// 						ecall_add_user_context(client->getSocket());
						// #endif

						// TODO: implement authentication protocol here
						logMessage(SERVER_LOG_LEVEL,
								   "Accepted new client connection [%d]",
								   client->getSocket());

						// create new worker thread for the client
						client_thread = new pthread_t;
						client_worker_threads.emplace_back(client_thread);
						pthread_create(client_thread, NULL, client_worker_entry,
									   (void *)client);
						logMessage(SERVER_LOG_LEVEL,
								   "Initialized new client worker thread [%lu]",
								   client_thread);
					} else {
						logMessage(LOG_ERROR_LEVEL, "Accept failed, aborting.");
						done = true;
					}

				} else {
					// Super weird case where the connection is
					// corrupted/uninitialized
					logMessage(LOG_ERROR_LEVEL, "Weird socket, aborting");
					done = true;
				}
			} else {
				// single-threaded
				/**
				 * For each ready channel, check type. If the server socket in
				 * ready state, a client is trying to connect; otherwise if a
				 * client socket is ready, then the server must read data (dont
				 * need to send back).
				 */
				if (it->second->getType() == SCH_SERVER) {
					// Check if the server socket was closed and needs to be
					// shutdown (if so, break the loops and enter cleanup
					// phase). Might hit this case if the signal was received
					// outside of a system call (e.g., a select call).
					if (!bfs_server_listener_status) {
						done = true;
						break;
					}

					// Accept the connection, add to the mux
					if ((client = it->second->accept()) != NULL) {
						mux->addConnection(client);

						// #ifdef __BFS_NONENCLAVE_MODE
						// 						int64_t ecall_status = 0;
						// 						if (((ecall_status =
						// ecall_add_user_context( eid, client->getSocket())) !=
						// SGX_SUCCESS)) { logMessage(LOG_ERROR_LEVEL, "Failed
						// during ecall_add_user_context. " "Error code: %d\n",
						// ecall_status); 							return
						// BFS_FAILURE;
						// 						}
						// #else
						// 						ecall_add_user_context(client->getSocket());
						// #endif

						// TODO: implement authentication protocol here
						logMessage(SERVER_LOG_LEVEL,
								   "Accepted new client connection [%d]",
								   client->getSocket());

						// use same ecall for initial connection setup, in place
						// of ecall_add_user_context
						handler_rc = BFS_FAILURE;
						if ((handler_rc = handle_in_msg(client, NULL)) ==
							BFS_FAILURE) {
							logMessage(
								LOG_ERROR_LEVEL,
								"Error handling client initial connect.\n");
							abort();
						}

					} else {
						logMessage(LOG_ERROR_LEVEL, "Accept failed, aborting.");
						done = true;
					}
				} else if (it->second->getType() == SCH_CLIENT) {
					client = it->second;
					rlen = client->recvPacketizedBuffer(rpkt_enc);
					if (rlen == 0) {
						// Socket closed, cleanup
						logMessage(SERVER_LOG_LEVEL,
								   "Connection [%d] closed, cleaning up.",
								   it->first);
						mux->removeConnection(client);
						delete client;
					} else if (rlen == -1) {
						/**
						 * Failed during recvPacketizedDataL (buffer likely too
						 * short for incoming data). Just ignore message and
						 * cleanup the client connection.
						 */
						logMessage(LOG_ERROR_LEVEL,
								   "Failed during recvPacketizedDataL on "
								   "connection [%d] in main loop: rlen is %d\n",
								   it->first, rlen);
						mux->removeConnection(client);
						delete client;
					} else {
						// Got _some_ data, keep processing
						logMessage(SERVER_VRB_LOG_LEVEL,
								   "Received [%d] bytes on connection [%d]",
								   rlen, it->first);

						// single threaded; handle the request directly in
						// the main thread. Only do if single-threaded
						handler_rc = BFS_FAILURE;
						if ((handler_rc = handle_in_msg(client, &rpkt_enc)) ==
							BFS_FAILURE)
							logMessage(LOG_ERROR_LEVEL,
									   "Error handling client request.\n");
					}
				} else {
					// Super weird case where the connection is
					// corrupted/uninitialized
					logMessage(LOG_ERROR_LEVEL, "Weird socket, aborting");
					done = true;
				}
			}
		}
	}

	// Remove the server from the connection list, cleanup
	mux->removeConnection(server);
	delete server;
	mux->cleanup();
	delete mux;

	// If the status is still OK here, then it wasn't as a result of a caught
	// signal, so we have a fatal error (failed select).
	if (bfs_server_listener_status) {
		logMessage(LOG_ERROR_LEVEL,
				   "Server encountered fatal failure, shutting down.");
		return BFS_FAILURE;
	}

	// otherwise it was a signal caught indicating to shutdown, so just log
	// and write the perf results to a file
	write_server_latencies();

	logMessage(SERVER_LOG_LEVEL, "Server shutting down.");

	return BFS_SUCCESS;
}

/**
 * @brief Forward the message buffer directly to the enclave for
 * processing. The enclave must verify integrity of contents before use.
 *
 * @param in_conn_ptr: the connection context for sending outbound messages
 * @param rpkt_enc: the encrypted inbound message to give to the enclave
 * @return int BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int64_t handle_in_msg(bfsNetworkConnection *in_conn_ptr,
							 bfsFlexibleBuffer *rpkt_enc) {
	int64_t ret = -1;

#ifdef __BFS_NONENCLAVE_MODE
	int64_t ecall_status = 0;
	if (((ecall_status = ecall_bfs_handle_in_msg(eid, &ret, (void *)in_conn_ptr,
												 (void *)rpkt_enc)) !=
		 SGX_SUCCESS) ||
		(ret == BFS_FAILURE)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed during handle_in_msg. Error code: %d\n",
				   ecall_status != SGX_SUCCESS ? ecall_status : ret);
	}
#else
	if (((ret = ecall_bfs_handle_in_msg((void *)in_conn_ptr,
										(void *)rpkt_enc)) == BFS_FAILURE)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed during handle_in_msg. Error code: %d\n", ret);
	}
#endif

	return ret;
}

/**
 * @brief Log all of the collect server results.
 *
 */
void write_server_latencies() {
	if (!bfsUtilLayer::perf_test())
		return;

	// Log total server read latencies
	std::string __s_read__lats = vec_to_str<long>(s_read__lats);
	std::string __s_read__lats_fname(getenv("BFS_HOME"));
	__s_read__lats_fname += "/benchmarks/micro/output/__s_read__lats.csv";
	std::ofstream __s_read__lats_f;
	__s_read__lats_f.open(__s_read__lats_fname.c_str(), std::ios::trunc);
	__s_read__lats_f << __s_read__lats.c_str();
	__s_read__lats_f.close();
	logMessage(SERVER_LOG_LEVEL,
			   "Read latencies (overall, us, %lu records):\n[%s]\n",
			   s_read__lats.size(), __s_read__lats.c_str());

	// Log non-network-related server read latencies
	std::string __s_read__s_lats = vec_to_str<long>(s_read__s_lats);
	std::string __s_read__s_lats_fname(getenv("BFS_HOME"));
	__s_read__s_lats_fname += "/benchmarks/micro/output/__s_read__s_lats.csv";
	std::ofstream __s_read__s_lats_f;
	__s_read__s_lats_f.open(__s_read__s_lats_fname.c_str(), std::ios::trunc);
	__s_read__s_lats_f << __s_read__s_lats.c_str();
	__s_read__s_lats_f.close();
	logMessage(SERVER_LOG_LEVEL,
			   "Read latencies (non-network, us, %lu records):\n[%s]\n",
			   s_read__s_lats.size(), __s_read__s_lats.c_str());

	// Log network-related sends for server reads
	std::string __s_read__net_c_send_lats =
		vec_to_str<long>(s_read__net_c_send_lats);
	std::string __s_read__net_c_send_lats_fname(getenv("BFS_HOME"));
	__s_read__net_c_send_lats_fname +=
		"/benchmarks/micro/output/__s_read__net_c_send_lats.csv";
	std::ofstream __s_read__net_c_send_lats_f;
	__s_read__net_c_send_lats_f.open(__s_read__net_c_send_lats_fname.c_str(),
									 std::ios::trunc);
	__s_read__net_c_send_lats_f << __s_read__net_c_send_lats.c_str();
	__s_read__net_c_send_lats_f.close();
	logMessage(SERVER_LOG_LEVEL,
			   "Read latencies (network sends, us, %lu records):\n[%s]\n",
			   s_read__net_c_send_lats.size(),
			   __s_read__net_c_send_lats.c_str());

	// Log network-related receives for server reads
	std::string __s_read__net_recv_lats =
		vec_to_str<long>(s_read__net_recv_lats);
	std::string __s_read__net_recv_lats_fname(getenv("BFS_HOME"));
	__s_read__net_recv_lats_fname +=
		"/benchmarks/micro/output/__s_read__net_recv_lats.csv";
	std::ofstream __s_read__net_recv_lats_f;
	__s_read__net_recv_lats_f.open(__s_read__net_recv_lats_fname.c_str(),
								   std::ios::trunc);
	__s_read__net_recv_lats_f << __s_read__net_recv_lats.c_str();
	__s_read__net_recv_lats_f.close();
	logMessage(SERVER_LOG_LEVEL,
			   "Read latencies (network recvs, us, %lu records):\n[%s]\n",
			   s_read__net_recv_lats.size(), __s_read__net_recv_lats.c_str());

	// Log total server write latencies
	std::string __s_write__lats = vec_to_str<long>(s_write__lats);
	std::string __s_write__lats_fname(getenv("BFS_HOME"));
	__s_write__lats_fname += "/benchmarks/micro/output/__s_write__lats.csv";
	std::ofstream __s_write__lats_f;
	__s_write__lats_f.open(__s_write__lats_fname.c_str(), std::ios::trunc);
	__s_write__lats_f << __s_write__lats.c_str();
	__s_write__lats_f.close();
	logMessage(SERVER_LOG_LEVEL,
			   "Write latencies (overall, us, %lu records):\n[%s]\n",
			   s_write__lats.size(), __s_write__lats.c_str());

	// Log non-network-related server write latencies
	std::string __s_write__s_lats = vec_to_str<long>(s_write__s_lats);
	std::string __s_write__s_lats_fname(getenv("BFS_HOME"));
	__s_write__s_lats_fname += "/benchmarks/micro/output/__s_write__s_lats.csv";
	std::ofstream __s_write__s_lats_f;
	__s_write__s_lats_f.open(__s_write__s_lats_fname.c_str(), std::ios::trunc);
	__s_write__s_lats_f << __s_write__s_lats.c_str();
	__s_write__s_lats_f.close();
	logMessage(SERVER_LOG_LEVEL,
			   "Write latencies (non-network, us, %lu records):\n[%s]\n",
			   s_write__s_lats.size(), __s_write__s_lats.c_str());

	// Log network-related sends for server writes
	std::string __s_write__net_c_send_lats =
		vec_to_str<long>(s_write__net_c_send_lats);
	std::string __s_write__net_c_send_lats_fname(getenv("BFS_HOME"));
	__s_write__net_c_send_lats_fname +=
		"/benchmarks/micro/output/__s_write__net_c_send_lats.csv";
	std::ofstream __s_write__net_c_send_lats_f;
	__s_write__net_c_send_lats_f.open(__s_write__net_c_send_lats_fname.c_str(),
									  std::ios::trunc);
	__s_write__net_c_send_lats_f << __s_write__net_c_send_lats.c_str();
	__s_write__net_c_send_lats_f.close();
	logMessage(SERVER_LOG_LEVEL,
			   "Write latencies (network sends, us, %lu records):\n[%s]\n",
			   s_write__net_c_send_lats.size(),
			   __s_write__net_c_send_lats.c_str());

	// Log network-related receives for server writes
	std::string __s_write__net_recv_lats =
		vec_to_str<long>(s_write__net_recv_lats);
	std::string __s_write__net_recv_lats_fname(getenv("BFS_HOME"));
	__s_write__net_recv_lats_fname +=
		"/benchmarks/micro/output/__s_write__net_recv_lats.csv";
	std::ofstream __s_write__net_recv_lats_f;
	__s_write__net_recv_lats_f.open(__s_write__net_recv_lats_fname.c_str(),
									std::ios::trunc);
	__s_write__net_recv_lats_f << __s_write__net_recv_lats.c_str();
	__s_write__net_recv_lats_f.close();
	logMessage(SERVER_LOG_LEVEL,
			   "Write latencies (network recvs, us, %lu records):\n[%s]\n",
			   s_write__net_recv_lats.size(), __s_write__net_recv_lats.c_str());
}
