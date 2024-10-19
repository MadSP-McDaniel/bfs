/**
 * @file bfsNetworkConnection.cpp
 * @brief This is the main file for the BFS communication layer
 */

/* Include files  */

/* Project include files */
#ifdef __BFS_ENCLAVE_MODE
#include "bfs_enclave_t.h" /* For ocalls */
#include "sgx_trts.h"
#else
#include <arpa/inet.h>
#include <bfs_rawnet.h>
#endif

#include <bfsNetworkConnection.h>
#include <bfs_log.h>

/* Macros */

/* Globals  */

/* Function prototypes  */

/**
 * @brief The constructor for the class (private)
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

bfsNetworkConnection::bfsNetworkConnection(void)
	: chState(SCH_INITIALIZED), chType(SCH_UKNOWN), chPort(0), socket(-1) {

	// Return, no return code
	return;
}

/**
 * @brief The destructor for the class.
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

bfsNetworkConnection::~bfsNetworkConnection(void) {

	// Return, no return code
	return;
}

/**
 * @brief Connect the channel (client connects to server, server binds)
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsNetworkConnection::connect(void) {
	// Check whatever connection type is used
	setState(SCH_CONNECTING);
	if (chType == SCH_SERVER) {

#ifdef __BFS_ENCLAVE_MODE
		int32_t ocall_status = 0;
		if ((ocall_status = ocall_rawnet_connect_server(&socket, chPort)) !=
			SGX_SUCCESS)
			return 0;
#else
		socket = rawnet_connect_server(chPort);
#endif
		// Attempt to connect to the server (listen socket)
		if (socket <= 0) {
			logMessage(LOG_ERROR_LEVEL,
					   "Connection failed on (server) socket, aborting.");
			setState(SCH_ERRORED);
			return (-1);
		}

	} else if (chType == SCH_CLIENT) {
		// Attempt to connect the client communication (socket)
		setState(SCH_CONNECTING);

#ifdef __BFS_ENCLAVE_MODE
		int32_t ocall_status = 0;
		if ((ocall_status = ocall_rawnet_client_connect(
				 &socket, chAddress.c_str(),
				 ((int32_t)strlen(chAddress.c_str()) + 1), chPort)) !=
			SGX_SUCCESS)
			return -1;
#else
		socket = rawnet_client_connect(chAddress.c_str(), chPort);
#endif
		if (socket <= 0) {
			logMessage(LOG_ERROR_LEVEL,
					   "Connection failed on (client) socket, aborting.");
			setState(SCH_ERRORED);
			return (-1);
		}

	} else {

		// Unknown type, should never happen
		logMessage(LOG_ERROR_LEVEL, "Connecting unknown connection type, [%d]",
				   chType);
		setState(SCH_ERRORED);
		return (-1);
	}

	// Set to connected, return successfully
	setState(SCH_CONNECTED);
	return (0);
}

/**
 * @brief Disconnect the channel (client disconnect, server closes and stop
 * listening)
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsNetworkConnection::disconnect(void) {

	// Close the connection, set the state
	int retval = 0;
#ifdef __BFS_ENCLAVE_MODE
	int32_t ocall_status = 0;
	if ((ocall_status = ocall_rawnet_close(&retval, socket)) != SGX_SUCCESS)
		return -1;
#else
	retval = rawnet_close(socket);
#endif
	socket = -1;
	setState(SCH_INITIALIZED);

	// Return successfully
	return (retval);
}

/**
 * @brief Accept and incoming connection (on server socket)
 *
 * @param len - the length of the data to recv
 * @param buf - maximum bytes to receive
 * @return int : bytes recved if success, -1 is failure
 */

bfsNetworkConnection *bfsNetworkConnection::accept(void) {

	// Local variables
	bfsNetworkConnection *newconn;
	int newsock;

#ifdef __BFS_ENCLAVE_MODE
	int32_t ocall_status = 0;
	if ((ocall_status = ocall_rawnet_accept_connection(&newsock, socket)) !=
		SGX_SUCCESS)
		return NULL;
#else
	newsock = rawnet_accept_connection(socket);
#endif

	// Accept the new socket, check for error
	if (newsock == -1) {
		logMessage(LOG_ERROR_LEVEL, "Accept failed on server.");
		return (NULL);
	}

	// Create new object, set state and return it
	newconn = new bfsNetworkConnection();
	newconn->chType = SCH_CLIENT;
	newconn->chState = SCH_CONNECTED;
	newconn->socket = newsock;
	return (newconn);
}

//
// Send the data (not buffer)

/**
 * @brief Send data over the connection using a 32-bit length.
 *
 * @param len - the length of the data to send
 * @param buf - the buffer to send
 * @return int : bytes send if success, -1 is failure
 */

int bfsNetworkConnection::sendDataL(uint32_t len, char *buf) {

	// Send the data, check for closed and errored comms
	int ret;

#ifdef __BFS_ENCLAVE_MODE
	int32_t ocall_status = 0;
	if ((ocall_status = ocall_rawnet_send_bytes(&ret, socket, len, buf)) !=
		SGX_SUCCESS)
		return -1;
#else
	ret = rawnet_send_bytes(socket, len, buf);
#endif

	if (ret == 0) {
		chState = SCH_CLOSED;
		return (0);
	}
	if (ret < 0) {
		chState = SCH_ERRORED;
		return (0);
	}

	// Return the number of bytes sent
	return (ret);
}

/**
 * @brief Receive data from the connection using a 32-bit length.
 *
 * @param len - the length of the data to recv
 * @param buf - maximum bytes to receive
 * @return int : bytes recved if success, -1 is failure
 */

int bfsNetworkConnection::recvDataL(uint32_t len, char *buf) {

	// Send the data, check for closed and errored comms
	int ret;

#ifdef __BFS_ENCLAVE_MODE
	int32_t ocall_status = 0;
	if ((ocall_status = ocall_rawnet_read_bytes(&ret, socket, len, buf)) !=
		SGX_SUCCESS)
		return -1;
#else
	ret = rawnet_read_bytes(socket, len, buf);
#endif

	if (ret == 0) {
		chState = SCH_CLOSED;
		return (0);
	}
	if (ret < 0) {
		chState = SCH_ERRORED;
		return (0);
	}

	// Return the number of bytes sent
	return (ret);
}

/**
 * @brief Send data over the connection (with header) using a 32-bit length.
 *
 * @param len - the length of the data to send
 * @param buf - the buffer to send (32 bit)
 * @return int : bytes send if success, -1 is failure
 */

int bfsNetworkConnection::sendPacketizedDataL(bfs_size_t len, char *buf) {

	// Return the number of bytes received
	int ret;

// Do the send, check for closed socket or
#ifdef __BFS_ENCLAVE_MODE
	int32_t ocall_status = 0;
	if ((ocall_status = ocall_sendPacketizedDataHdrL(&ret, socket, len)) !=
		SGX_SUCCESS)
		return -1;
	if (ret == 0) {
		chState = SCH_CLOSED;
		return (0);
	}
	if (ret < 0) {
		chState = SCH_ERRORED;
		return (0);
	}
#else
	uint32_t slen = htonl(len);
	ret = sendDataL((unsigned short)sizeof(uint32_t), (char *)&slen);
#endif

	if (ret != sizeof(uint32_t)) {
		return (ret);
	}
	return (sendDataL(len, buf));
}

/**
 * @brief Receive data from the connection (with header) using a 32-bit length.
 *
 * @param len - the length of the data to recv (32 bit)
 * @param buf - maximum bytes to receive
 * @return int : bytes recved if success, -1 is failure
 */

int bfsNetworkConnection::recvPacketizedDataL(bfs_size_t len, char *buf) {

	// Return the number of bytes received
	uint32_t slen;

#ifdef __BFS_ENCLAVE_MODE
	uint32_t uret;
	int32_t ocall_status = 0;
	if ((ocall_status = ocall_recvPacketizedDataHdrL((uint32_t *)&uret,
													 socket)) != SGX_SUCCESS)
		return -1;
	if (uret == 0) {
		chState = SCH_ERRORED;
		return (0);
	}
	slen = uret;
#else
	int ret;
	// Do the recv, check for bad return
	ret = recvDataL((unsigned short)sizeof(uint32_t), (char *)&slen);
	if (ret != sizeof(uint32_t)) {
		return (ret);
	}

	// Make sure there is enough memory to receive data
	slen = ntohl(slen);
#endif

	if (len < slen) {
		logMessage(LOG_ERROR_LEVEL, "Buffer too short on packetized read.");
		return (-1);
	}
	return (recvDataL(slen, buf));
}

//
// Buffer versions

/**
 * @brief Send buffer over the connection
 *
 * @param buf - the buffer object to send on
 * @param len - the length of the data to sned
 * @return int : bytes sent if success, -1 is failure
 */

int bfsNetworkConnection::sendBuffer(bfsFlexibleBuffer &buf) {
	int ret;

#ifdef __BFS_ENCLAVE_MODE
	int32_t ocall_status = 0;
	if ((ocall_status = ocall_rawnet_send_bytes(
			 &ret, socket, buf.getLength(), buf.getBuffer())) != SGX_SUCCESS)
		return 0;
#else
	ret = rawnet_send_bytes(socket, buf.getLength(), buf.getBuffer());
#endif

	if (ret == 0) {
		chState = SCH_CLOSED;
		return (0);
	}
	if (ret < 0) {
		chState = SCH_ERRORED;
		return (0);
	}

	// Return the number of bytes sent
	return (ret);
}

/**
 * @brief Receive buffer from the connection (param indicates bytes to recieve)
 *
 * @param buf - the buffer object to receive on
 * @param len - the length of the data to recv
 * @return int : bytes recved if success, -1 is failure
 */

int bfsNetworkConnection::recvBuffer(bfsFlexibleBuffer &buf, bfs_size_t len) {
	// Send the data, check for closed and errored comms
	int ret;
	buf.resetWithAlloc(len);

#ifdef __BFS_ENCLAVE_MODE
	int32_t ocall_status = 0;
	if ((ocall_status = ocall_rawnet_read_bytes(
			 &ret, socket, len, buf.getBuffer())) != SGX_SUCCESS)
		return -1;
#else
	ret = rawnet_read_bytes(socket, len, buf.getBuffer());
#endif

	if (ret == 0) {
		chState = SCH_CLOSED;
		return (0);
	}
	if (ret < 0) {
		chState = SCH_ERRORED;
		return (0);
	}

	// Return the number of bytes received
	return (ret);
}

/**
 * @brief Send data over the connection (with header) using a 32-bit length.
 *
 * @param len - the length of the data to send
 * @param buf - the buffer to send (32 bit)
 * @return int : bytes send if success, -1 is failure
 */

int bfsNetworkConnection::sendPacketizedBuffer(bfsFlexibleBuffer &buf) {
	// Return the number of bytes received
	int ret;

#ifdef __BFS_ENCLAVE_MODE
	int32_t ocall_status = 0;
	if ((ocall_status = ocall_sendPacketizedDataHdrL(
			 &ret, socket, buf.getLength())) != SGX_SUCCESS)
		return -1;
	if (ret == 0) {
		chState = SCH_CLOSED;
		return (0);
	}
	if (ret < 0) {
		chState = SCH_ERRORED;
		return (0);
	}
#else
	// Do the length send, check for closed socket
	bfs_size_t slen = htonl(buf.getLength());
	ret = sendDataL(sizeof(bfs_size_t), (char *)&slen);
#endif

	// Check for an error, send if it looks good
	if (ret != sizeof(bfs_size_t)) {
		return (ret);
	}
	return (sendBuffer(buf));
}

/**
 * @brief Receive data from the connection (with header) using a 32-bit length.
 *
 * @param len - the length of the data to recv (32 bit)
 * @param buf - maximum bytes to receive
 * @return int : bytes recved if success, -1 is failure
 */

int bfsNetworkConnection::recvPacketizedBuffer(bfsFlexibleBuffer &buf) {
	// Return the number of bytes received
	bfs_size_t slen;

#ifdef __BFS_ENCLAVE_MODE
	uint32_t uret;
	int32_t ocall_status = 0;
	if ((ocall_status = ocall_recvPacketizedDataHdrL((uint32_t *)&uret,
													 socket)) != SGX_SUCCESS)
		return -1;
	if (uret == 0) {
		chState = SCH_ERRORED;
		return (0);
	}
	slen = uret;
#else
	int ret;
	// Do the recv, check for bad return
	ret = recvDataL((unsigned short)sizeof(uint32_t), (char *)&slen);
	if (ret != sizeof(uint32_t)) {
		return (ret);
	}

	// Make sure there is enough memory to receive data
	slen = ntohl(slen);
#endif

	return (recvBuffer(buf, slen));
}

//
// Static Class methods

/**
 * @brief client factory (INET domain), string address
 *
 * @param addr - the address (in string form)
 * @param pt - the port to connect to
 * @return int : 0 is success, -1 is failure
 */
bfsNetworkConnection *
bfsNetworkConnection::bfsChannelFactory(string addr, unsigned short pt) {

	// Create, then connect the socket
	bfsNetworkConnection *conn = new bfsNetworkConnection();
	conn->chType = SCH_CLIENT;
	conn->chAddress = addr;
	conn->chPort = pt;

	// Return the connected client socket
	return (conn);
}

/**
 * @brief Server factory (INET domain)
 *
 * @param pt - the local port to bind to
 * @return int : 0 is success, -1 is failure
 */

bfsNetworkConnection *
bfsNetworkConnection::bfsChannelFactory(unsigned short pt) {

	// Create, then connect the socket
	bfsNetworkConnection *conn = new bfsNetworkConnection();
	conn->chType = SCH_SERVER;
	conn->chPort = pt;

	// Return the connected client socket
	return (conn);
}
