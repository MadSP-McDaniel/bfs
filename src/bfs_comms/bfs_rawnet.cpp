////////////////////////////////////////////////////////////////////////////////
//
//  File          : rawnet_network.c
//  Description   : This is the basic networking IO for the BFG system.  It is
//                  used to provide a foundation for all non-enclave network
//                  communication.
//
//  Author        : Patrick McDaniel
//  Last Modified : Wed 10 Mar 2021 01:38:58 PM EST
//

// Include Files
#include <arpa/inet.h>
#include <errno.h>
#include <list>
#include <netinet/tcp.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// Project Include Files
#include <bfs_log.h>
#include <bfs_rawnet.h>
#include <bfs_util.h>
#include <pthread.h>

// Defines
#define RAWNET_MAX_BACKLOG 5
#define RAWNET_NETWORK_ITERATIONS 10000
#define RAWNET_NETWORK_UNIT_TEST_MAX_MSG_SIZE 4096

// Global variables

unsigned char *rawnet_network_address = NULL; // Address of RAWNET server
unsigned short rawnet_network_port = 0;		  // Port of RAWNET server

// Functional Prototypes (local methods)
int rawnetNetworkUnitSend(int sock, int len, char *buf);
int rawnetNetworkUnitRecv(int sock, int len, char *buf);

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : rawnet_connect_server
// Description  : This function makes a server connection on a bound port.
//
// Inputs       : none
// Outputs      : the server socket if successful, -1 if failure

int rawnet_connect_server(unsigned short port) {

	// Local variables
	struct sockaddr_in saddr;
	int server, optval;

	// Create the socket
	if ((server = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		// Error out
		logMessage(LOG_ERROR_LEVEL, "RAWNET socket() create failed : [%s]",
				   strerror(errno));
		return (-1);
	}

	// Setup so we can reuse the address
	optval = 1;
	if (setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) !=
		0) {
		// Error out
		logMessage(LOG_ERROR_LEVEL,
				   "RAWNET set socket option create failed : [%s]",
				   SO_REUSEADDR, strerror(errno));
		close(server);
		return (-1);
	}

	// Disable nagles alg on the socket to prevent delays for small reqs
	optval = 1;
	if (setsockopt(server, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) !=
		0) {
		// Error out
		logMessage(LOG_ERROR_LEVEL,
				   "RAWNET set socket option [%d] create failed : [%s]",
				   TCP_NODELAY, strerror(errno));
		close(server);
		return (-1);
	}

	// zero-copy reads/writes
	// optval = 1;
	// if (setsockopt(server, SOL_SOCKET, SO_ZEROCOPY, &optval, sizeof(optval)) !=
	// 	0) {
	// 	// Error out
	// 	logMessage(LOG_ERROR_LEVEL,
	// 			   "RAWNET set socket option [%d] create failed : [%s]",
	// 			   SO_ZEROCOPY, strerror(errno));
	// 	close(server);
	// 	return (-1);
	// }

	// Setup address and bind the server to a particular port
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port);
	saddr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Now bind to the server socket
	if (bind(server, (struct sockaddr *)&saddr, sizeof(struct sockaddr)) ==
		-1) {
		// Error out
		logMessage(LOG_ERROR_LEVEL, "RAWNET bind() on port %u : [%s]", port,
				   strerror(errno));
		close(server);
		server = -1;
		return (-1);
	}
	logMessage(LOG_INFO_LEVEL, "Server bound and listening on port [%d]", port);

	// Listen for incoming connection
	if (listen(server, RAWNET_MAX_BACKLOG) == -1) {
		logMessage(LOG_ERROR_LEVEL, "RAWNET listen() create failed : [%s]",
				   strerror(errno));
		close(server);
		server = -1;
		return (-1);
	}

	// Return the socket
	return (server);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : rawnet_accept_connection
// Description  : This function accepts an incoming connection from a client
//                (using the server socket)
//
// Inputs       : server - the server socket to accept the connection from
// Outputs      : the client socket if successful, -1 if failure

int rawnet_accept_connection(int server) {

	// Local variables
	struct sockaddr_in caddr;
	int client, optval;
	unsigned int inet_len;

	// Accept the connection, error out if failure
	inet_len = sizeof(caddr);
	if ((client = accept(server, (struct sockaddr *)&caddr, &inet_len)) == -1) {
		logMessage(LOG_ERROR_LEVEL,
				   "RAWNET server accept failed, aborting [%s].",
				   strerror(errno));
		return (-1);
	}

	// Disable nagles alg on the socket to prevent delays for small reqs
	optval = 1;
	if (setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) !=
		0) {
		// Error out
		logMessage(LOG_ERROR_LEVEL,
				   "RAWNET set socket option [%d] create failed : [%s]",
				   TCP_NODELAY, strerror(errno));
		close(client);
		return (-1);
	}

	// zero-copy reads/writes
	// optval = 1;
	// if (setsockopt(client, SOL_SOCKET, SO_ZEROCOPY, &optval, sizeof(optval)) !=
	// 	0) {
	// 	// Error out
	// 	logMessage(LOG_ERROR_LEVEL,
	// 			   "RAWNET set socket option [%d] create failed : [%s]",
	// 			   SO_ZEROCOPY, strerror(errno));
	// 	close(client);
	// 	return (-1);
	// }

	// Log the creation of the new connection, return the new client connection
	logMessage(LOG_INFO_LEVEL, "Server new client connection [%s/%d]",
			   inet_ntoa(caddr.sin_addr), caddr.sin_port);
	return (client);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : rawnet_client_connect
// Description  : Connect a client socket for network communication.
//
// Inputs       : ip - the IP address string for the client
//                port - the port number of the service
// Outputs      : socket file handle if successful, -1 if failure

int rawnet_client_connect(const char *ip, uint16_t port) {

	// Local variables
	int sock;
	struct sockaddr_in caddr;

	// Check to make sure you have a good IP address
	caddr.sin_family = AF_INET;
	caddr.sin_port = htons(port);
	if (inet_aton(ip, &caddr.sin_addr) == 0) {
		// Error out
		logMessage(LOG_ERROR_LEVEL,
				   "RAWNET client unable to interpret IP address [%s]", ip);
		return (-1);
	}

	// Create the socket
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		// Error out
		logMessage(LOG_ERROR_LEVEL, "RAWNET client socket() failed : [%s]",
				   strerror(errno));
		return (-1);
	}

	int optval = 1;
	if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) !=
		0) {
		// Error out
		logMessage(LOG_ERROR_LEVEL,
				   "RAWNET set socket option [%d] create failed : [%s]",
				   TCP_NODELAY, strerror(errno));
		close(sock);
		return (-1);
	}

	// zero-copy reads/writes
	// optval = 1;
	// if (setsockopt(sock, SOL_SOCKET, SO_ZEROCOPY, &optval, sizeof(optval)) !=
	// 	0) {
	// 	// Error out
	// 	logMessage(LOG_ERROR_LEVEL,
	// 			   "RAWNET set socket option [%d] create failed : [%s]",
	// 			   SO_ZEROCOPY, strerror(errno));
	// 	close(sock);
	// 	return (-1);
	// }

	// Now connect to the server
	if (connect(sock, (const struct sockaddr *)&caddr,
				sizeof(struct sockaddr)) == -1) {
		// Error out
		logMessage(LOG_ERROR_LEVEL, "RAWNET client connect() failed : [%s]",
				   strerror(errno));
		return (-1);
	}

	// Return the socket
	return (sock);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : rawnet_send_bytes
// Description  : Send a specific length of bytes to socket
//
// Inputs       : sock - the socket filehandle of the clinet connection
//                len - the number of bytes to send
//                buf - the buffer to send
// Outputs      : length if successful, 0 if closed, -1 if failure

int rawnet_send_bytes(int sock, int len, char *buf) {

	// Local variables
	long sentBytes = 0, sb;

	// Loop until you have read all the bytes
	while (sentBytes < len) {
		// Read the bytes and check for error
		if ((sb = write(sock, &buf[sentBytes], len - sentBytes)) < 0) {
			logMessage(LOG_ERROR_LEVEL, "RAWNET send bytes failed : [%s]",
					   strerror(errno));
			return (-1);
		}

		// Check for closed file
		else if (sb == 0) {
			// Close file, not an error
			logMessage(LOG_ERROR_LEVEL,
					   "RAWNET client socket closed on snd : [%s]",
					   strerror(errno));
			return (0);
		}

		// Now process what we read
		sentBytes += sb;
	}

	// Return successfully
	return (len);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : rawnet_read_bytes
// Description  : Receive a specific length of bytes from socket
//
// Inputs       : sock - the socket filehandle of the client connection
//                len - the number of bytes to be read
//                buf - the buffer to read into
// Outputs      : length if successful, 0 if closed, -1 if failure

int rawnet_read_bytes(int sock, int len, char *buf) {

	// Local variables
	long readBytes = 0, rb;

	// Loop until you have read all the bytes
	while (readBytes < len) {
		// Read the bytes and check for error
		if ((rb = read(sock, &buf[readBytes], len - readBytes)) < 0) {
			// Check for client error on read
			logMessage(LOG_ERROR_LEVEL, "RAWNET read bytes failed : [%s]",
					   strerror(errno));
			return (-1);
		}

		// Check for closed file
		else if (rb == 0) {
			// Close file, not an error
			logMessage(LOG_ERROR_LEVEL,
					   "RAWNET client socket closed on rd : [%s]",
					   strerror(errno));
			return (0);
		}

		// Now process what we read
		readBytes += rb;
	}

	// Return successfully
	return (len);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : rawnet_wait_read
// Description  : Wait for user input on the socket
//
// Inputs       : sock - the socket to write on
// Outputs      : 0 if successful, -1 if failure

int rawnet_wait_read(int sock) {

	// Local variables
	fd_set rfds;
	int nfds, ret;

	// Setup and perform the select
	nfds = sock + 1;
	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);
	ret = select(nfds, &rfds, NULL, NULL, NULL);

	// Check the return value
	if (ret == -1) {
		logMessage(LOG_ERROR_LEVEL, "RAWNET select() failed : [%s]",
				   strerror(errno));
		return (-1);
	}

	// check to make sure we are selected on the read
	if (FD_ISSET(sock, &rfds) == 0) {
		logMessage(LOG_ERROR_LEVEL,
				   "RAWNET select() returned without selecting FD : [%d]",
				   sock);
		return (-1);
	}

	// Return successfully
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : rawnet_close
// Description  : Close a socket associated with network communication.
//
// Inputs       : sock - the socket to close
// Outputs      : 0 if successful, -1 if failure

int rawnet_close(int sock) {

	// Log and return the close value
	logMessage(LOG_INFO_LEVEL, "RAWNET closing socket [%d]", sock);
	return (close(sock));
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : rawnet_server_unittest
// Description  : This is the unit test for the networking code (server side).
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int rawnet_server_unittest(unsigned short port) {

	// Local variables
	int server, client, retcode, len;
	char buf[RAWNET_NETWORK_UNIT_TEST_MAX_MSG_SIZE];

	// Connect the server
	if ((server = rawnet_connect_server(port)) == -1) {
		logMessage(LOG_ERROR_LEVEL, "RAWNET server connect failed : [%s]",
				   strerror(errno));
		return (-1);
	}

	// Wait for incoming connection, accept it
	rawnet_wait_read(server);
	if ((client = rawnet_accept_connection(server)) == -1) {
		logMessage(LOG_ERROR_LEVEL, "RAWNET client connect failed : [%s]",
				   strerror(errno));
		return (-1);
	}

	// Keep receiving until done
	do {
		// Receive some random data, then send some
		len = RAWNET_NETWORK_UNIT_TEST_MAX_MSG_SIZE;
		if ((retcode = rawnetNetworkUnitRecv(client, len, buf)) < 0) {
			logMessage(LOG_ERROR_LEVEL,
					   "RAWNET unit test protocol failed (send/recv) : [%s]",
					   strerror(errno));
			return (-1);
		} else if ((retcode > 0) &&
				   (rawnetNetworkUnitSend(client, retcode, buf) != retcode)) {
			logMessage(LOG_ERROR_LEVEL,
					   "RAWNET unit test protocol failed (send/recv) : [%s]",
					   strerror(errno));
			return (-1);
		}
	} while (retcode > 0);

	// Log and close the client and server connection
	rawnet_close(client);
	rawnet_close(server);

	// Return successfully
	logMessage(LOG_INFO_LEVEL, "RAW Network unit test completed successfully.");
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : rawnet_client_unittest
// Description  : This is the client thread function for the network unit test
//
// Inputs       : arg - arguments to the client function (should be NULL)
// Outputs      : pointer to a integer return value

int rawnet_client_unittest(const char *addr, unsigned short port) {

	// Local variables
	int client, i, len, rlen;
	char sbuf[RAWNET_NETWORK_UNIT_TEST_MAX_MSG_SIZE],
		rbuf[RAWNET_NETWORK_UNIT_TEST_MAX_MSG_SIZE], ch;

	// Log the starting of the test, connect to the server
	logMessage(LOG_INFO_LEVEL,
			   "Starting RAWNET network client iteration test %s/%d.", addr,
			   port);
	if ((client = rawnet_client_connect(addr, port)) == -1) {
		logMessage(LOG_ERROR_LEVEL, "RAWNET client connect failed : [%s]",
				   strerror(errno));
		return (-1);
	}

	// Now do a number of client processing iterations
	for (i = 0; i < RAWNET_NETWORK_ITERATIONS; i++) {

		// Create a randomized buffer to send
#ifdef __BFS_ENCLAVE_MODE
		// TODO
#else
		len = get_random_value(1, RAWNET_NETWORK_UNIT_TEST_MAX_MSG_SIZE);
		ch = (char)get_random_value(0, 255);
#endif
		memset(sbuf, ch, len);

		// Send the data
		if (rawnetNetworkUnitSend(client, len, sbuf) != len) {
			logMessage(LOG_ERROR_LEVEL, "RAWNET unit test send failed : [%s]",
					   strerror(errno));
			return (-1);
		}

		// Receive the response
		rlen = RAWNET_NETWORK_UNIT_TEST_MAX_MSG_SIZE;
		if ((rlen = rawnetNetworkUnitRecv(client, rlen, rbuf)) <= 0) {
			logMessage(LOG_ERROR_LEVEL, "RAWNET unit test recv failed : [%s]",
					   strerror(errno));
			return (-1);
		}

		// Check that both are the same length
		if (rlen != len) {
			logMessage(
				LOG_ERROR_LEVEL,
				"RAWNET network unit test buffer length mismatch : [%d !+ %d]",
				rlen, len);
			return (-1);
		}

		// Now check to see of the memory is correct
		if (memcmp(sbuf, rbuf, len) != 0) {
			logMessage(LOG_ERROR_LEVEL,
					   "RAWNET network unit test mismatch : [%s]",
					   strerror(errno));
			return (-1);
		}
	}

	// Log and close the client connection
	logMessage(LOG_INFO_LEVEL,
			   "RAWNET Network client unit test complete (%d blocks sent and "
			   "received)",
			   RAWNET_NETWORK_ITERATIONS);
	rawnet_close(client);

	// Return the return value and exit
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : rawnetNetworkUnitSend
// Description  : Perform a simple send
//
// Inputs       : sock - socket to perform test on
// Outputs      : 0 if successful, -1 if failure

int rawnetNetworkUnitSend(int sock, int len, char *buf) {

	// Now send the length, the buffer and the
	if ((rawnet_send_bytes(sock, sizeof(uint16_t), (char *)&len) !=
		 sizeof(uint16_t)) ||
		(rawnet_send_bytes(sock, len, (char *)buf) != len)) {
		// Failed, error out
		logMessage(LOG_ERROR_LEVEL,
				   "RAWNET network unit test data send fail : [%s]",
				   strerror(errno));
		return (-1);
	}

	// Return successfully
	logMessage(LOG_INFO_LEVEL,
			   "RAWNET network unit test message send : [len=%d,ch=%x]", len,
			   buf[0]);
	return (len);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : rawnetNetworkUnitRecv
// Description  : Perform a simple receive
//
// Inputs       : sock - socket to perform test on
// Outputs      : number of bytes, 0 if closed, -1 if failure

int rawnetNetworkUnitRecv(int sock, int len, char *buf) {

	// Local variables
	long rlen, buflen;

	// Read the header, checking for a close (and returning)
	buflen = len; // Save buffer length
	rlen = rawnet_read_bytes(sock, sizeof(uint16_t), (char *)&len);
	if (rlen == 0) {
		return (0);
	}

	// Check if the buffer big enough
	if (len > buflen) {
		logMessage(LOG_ERROR_LEVEL,
				   "RAWNET network unit buffer passed in too short (%d > %d)",
				   len, buflen);
		return (-1);
	}

	// Now read the buffer of length
	if ((rlen < 0) || (rawnet_read_bytes(sock, len, (char *)buf) != len)) {
		// Failed, error out
		logMessage(LOG_ERROR_LEVEL,
				   "RAWNET network unit test data read fail : [%s]",
				   strerror(errno));
		return (-1);
	}

	// Log and return successfully
	logMessage(LOG_INFO_LEVEL,
			   "RAWNET network unit test message received : [len=%d,ch=%x]",
			   len, buf[0]);
	return (len);
}
