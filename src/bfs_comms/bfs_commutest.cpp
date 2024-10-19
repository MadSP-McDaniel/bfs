////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfs_commutest.cpp
//  Description   : This is the unit test program for the BFS comms library.
//                  The program is run in pairs, one client and one server,
//                  which sends data back and forth and benchmarks throughput
//                  and latency between the client and server.
//
//   Author        : Patrick McDaniel
//   Last Modified : Thu 11 Mar 2021 07:03:24 AM EST
//

// Include Files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

// STL includes
#include <string>
using namespace std;

// Project Include Files
#include <bfsConnectionMux.h>
#include <bfsNetworkConnection.h>
#include <bfs_log.h>
#include <bfs_util.h>

// Defines
#define BFSCOMMS_ARGUMENTS "vhl:p:a:r"
#define USAGE                                                                  \
	"USAGE: bfs_commutest [-h] [-v] [-l <logfile>] [-p <port>] [-a "           \
	"<address>]\n"                                                             \
	"\n"                                                                       \
	"where:\n"                                                                 \
	"    -h - help mode (display this message)\n"                              \
	"    -v - verbose output\n"                                                \
	"    -l - write log messages to the filename <logfile>\n"                  \
	"    -p - port number of server to bind to, client to connect to.\n"       \
	"    -a - address to connect to (enables client mode).\n"                  \
	"    -r - enables the \"raw\" communication mode (low level U/O).\n"       \
	"\n"
#define BFS_COMM_MAX_TEST_BUF 2048

// Global data

// Functional Prototypes
int bfsServerTest(unsigned short port);
int bfsClientTest(unsigned short port, string address);

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : main
// Description  : The main function for the BFS comms unit test.
//
// Inputs       : argc - the number of command line parameters
//                argv - the parameters
// Outputs      : 0 if successful, -1 if failure

int main(int argc, char *argv[]) {

	// Local variables
	int ch, verbose = 0, log_initialized = 0, retval;
	uint16_t port;
	string address;
	bool client = false, raw = false;

	// Process the command line parameters
	while ((ch = getopt(argc, argv, BFSCOMMS_ARGUMENTS)) != -1) {

		switch (ch) {
		case 'h': // Help, print usage
			fprintf(stderr, USAGE);
			return (-1);

		case 'v': // Verbose Flag
			verbose = 1;
			break;

		case 'l': // Set the log filename
			initializeLogWithFilename(optarg);
			log_initialized = 1;
			break;

		case 'p': // Set the network port number
			if (sscanf(optarg, "%hu", &port) != 1) {
				logMessage(LOG_ERROR_LEVEL, "Bad  port number [%s]", optarg);
				return (-1);
			}
			break;

		case 'a': // Set the network address
			address = optarg;
			client = true;
			break;

		case 'r': // Enable the raw mode
			raw = true;
			break;

		default: // Default (unknown)
			fprintf(stderr, "Unknown command line option (%c), aborting.\n",
					ch);
			return (-1);
		}
	}

	// Check for sane parameters
	if (port == 0) {
		fprintf(stderr, "Missing port for communication, aborting\n");
		fprintf(stderr, USAGE);
		return (-1);
	}

	// Setup the log as needed
	if (!log_initialized) {
		initializeLogWithFilehandle(STDERR_FILENO);
	}

	enableLogLevels(LOG_OUTPUT_LEVEL);
	if (verbose) {
		enableLogLevels(LOG_INFO_LEVEL);
	}

	// Run in raw mode or normal mode
	if (raw == true) {
		retval = (client == true)
					 ? rawnet_client_unittest(address.c_str(), port)
					 : rawnet_server_unittest(port);
	} else {

		// Use the comms layer
		if (client == true) {
			retval = bfsClientTest(port, address);
		} else {
			retval = bfsServerTest(port);
		}
	}

	// Return successfully
	return (retval);
}

/**
 * @brief The test server loop
 *
 * @param port - port to listen for incoming connections
 * @return 0 if successful, -1 if failure
 */

int bfsServerTest(unsigned short port) {

	// Local variables
	bfsNetworkConnection *server, *client;
	bfsConnectionMux *mux;
	bfsConnectionList ready;
	bfsConnectionList::iterator it;
	bfs_size_t rlen;
	bfsFlexibleBuffer rpkt;
	bool done;

	// Create the network and mux objects
	server = bfsNetworkConnection::bfsChannelFactory(port);
	mux = new bfsConnectionMux();

	// Now connect the server, then add to mux
	if (server->connect()) {
		logMessage(LOG_ERROR_LEVEL, "Server connection failed, test aborting.");
		return (-1);
	}
	mux->addConnection(server);

	// Now keep listening to sockets until you are done
	done = false;
	while (!done) {

		// Wait for incoming data
		if (mux->waitConnections(ready, 0)) {
			logMessage(LOG_ERROR_LEVEL, "Mux wait failed, aborting test");
			done = true;
		} else {

			// Walk the list of sockets (which have data/processing)
			for (it = ready.begin(); it != ready.end(); it++) {
				if (it->second->getType() == SCH_SERVER) {

					// Accept the connection, add to the mux
					if ((client = it->second->accept()) != NULL) {
						mux->addConnection(client);
						logMessage(LOG_OUTPUT_LEVEL,
								   "Accepted new client connection [%d]",
								   client->getSocket());
					} else {
						logMessage(LOG_ERROR_LEVEL, "Accept failed, aborting.");
						done = true;
					}

				} else if (it->second->getType() == SCH_CLIENT) {

					// Just receive the incoming data
					rlen = (size_t)it->second->recvPacketizedBuffer(rpkt);
					if (rlen == 0) {
						// Socket closed, cleanup
						logMessage(LOG_OUTPUT_LEVEL,
								   "Connection [%d] closed, cleaning up.",
								   it->first);
						client = it->second;
						mux->removeConnection(it->second);
						delete client;
					} else {
						// Data received, keep processing
						logMessage(LOG_INFO_LEVEL,
								   "Received [%d] bytes on connection [%d]",
								   rlen, it->first);
						if (it->second->sendPacketizedBuffer(rpkt) !=
							(int)rlen) {
							logMessage(LOG_ERROR_LEVEL,
									   "Failure sending back to client.");
							done = true;
						}
						logMessage(LOG_INFO_LEVEL,
								   "Sent [%d] bytes on connection [%d]", rlen,
								   it->first);
					}

				} else {
					// Super weird case where the connection is
					// corrupted/uninitialized
					logMessage(LOG_ERROR_LEVEL,
							   "Weird socket in test, aborting");
					done = true;
				}
			}
		}
	}

	// Remove the server from the connection list, cleanup
	mux->removeConnection(server);
	mux->cleanup();
	delete server;
	delete mux;

	// Return successfully, log completion
	logMessage(LOG_OUTPUT_LEVEL, "Server test shutdown, complete.");
	return (0);
}

/**
 * @brief The test client loop
 *
 * @param port - port to listen for incoming connections
 * @return 0 if successful, -1 if failure
 */

int bfsClientTest(unsigned short port, string address) {

	// Local variables
	bfsNetworkConnection *client;
	bfsConnectionMux *mux;
	bfsConnectionList ready;
	bfs_size_t len, rlen, slen;
	bool done;
	// struct timeval now, previous;
	bfsFlexibleBuffer spkt, rpkt;

	// Create the network and mux objects, connect
	client = bfsNetworkConnection::bfsChannelFactory(address, port);
	if (client->connect()) {
		logMessage(LOG_ERROR_LEVEL, "Client connection failed, test aborting.");
		return (-1);
	}
	mux = new bfsConnectionMux();
	mux->addConnection(client);

	// for TP measurements
	struct timeval reads_start_time, reads_end_time, writes_start_time,
		writes_end_time;
	double total_bytes_written = 0., total_bytes_read = 0.;
	double total_write_time = 0., total_read_time = 0.;

	// Now keep listening to sockets until you are done
	// gettimeofday(&previous, NULL);
	done = false;

	bool did_read, did_write;
	while (!done) {
		did_read = false;
		did_write = false;

		// Wait for incoming data
		if (mux->waitConnections(ready, 10)) {
			logMessage(LOG_ERROR_LEVEL, "Mux wait failed, aborting test");
			done = true;
            continue;
		} else {

			// Check to see if the client is in the ready list
			if (ready.find(client->getSocket()) != ready.end()) {

				// Just receive the incoming data
				gettimeofday(&reads_start_time, NULL);
				rlen = client->recvPacketizedBuffer(rpkt);
				gettimeofday(&reads_end_time, NULL);

				if (rlen == 0) {
					// Server side socket closed, bail out
					logMessage(LOG_OUTPUT_LEVEL,
							   "Server connection closed, shutting down.");
					done = true;
					continue;
				}

				did_read = true;

				// Data received, check to see if the data is sane
				logMessage(LOG_INFO_LEVEL,
						   "Received [%d] bytes on connection [%d]", rlen,
						   client->getSocket());
				if (rlen > 0) {
					total_bytes_read += rlen;
					total_read_time += (double)compareTimes(&reads_start_time,
															&reads_end_time);
				}
			}
		}

		// Check to see if it is time to send (1/second)
		// gettimeofday(&now, NULL);
		// if (compareTimes(&previous, &now) > 10000L) {

		// Setup packet and send it
		// slen = get_random_value(4, BFS_COMM_MAX_TEST_BUF - 1);
		slen = get_random_value(4096, 1048576);
		spkt.resetWithAlloc(slen);
		get_random_data(spkt.getBuffer(), slen);

		gettimeofday(&writes_start_time, NULL);
		len = client->sendPacketizedBuffer(spkt);
		gettimeofday(&writes_end_time, NULL);
		if (len <= 0) {
			logMessage(LOG_OUTPUT_LEVEL, "Server connection closed or "
										 "errored on send, shutting down.");
			done = true;
            continue;
		}
		did_write = true;

		if (len > 0) {
			total_bytes_written += len;
			total_write_time +=
				(double)compareTimes(&writes_start_time, &writes_end_time);
		}

		logMessage(LOG_INFO_LEVEL, "Send data to server (length=%d)", slen);

		// Reset the timer
		// previous = now;
		// }

		if (did_read && did_write) {
			printf("\rRead TP: (%08.3f MB / %08.3f s) %08.3f MB/s ===== Write "
				   "TP: "
				   "(%08.3f "
				   "MB / %08.3f s) %08.3f MB/s",
				   total_bytes_read / 1e6, total_read_time / 1e6,
				   (total_bytes_read / (total_read_time / 1e6)) / 1e6,
				   total_bytes_written / 1e6, total_write_time / 1e6,
				   (total_bytes_written / (total_write_time / 1e6)) / 1e6);
		}
	}

	// Remove the client from the mux, cleanup
	mux->removeConnection(client);
	delete client;
	delete mux;

	// Return successfully, log completion
	logMessage(LOG_OUTPUT_LEVEL, "Client test shutdown, complete.");
	return (0);
}