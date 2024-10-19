#ifndef BFS_NETCONN_INCLUDED
#define BFS_NETCONN_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsNetworkConnection.h
//  Description   : This is the class describing network connections in the
//                  bfs file system.  This the RAW buffered network connection.
//
//  Author  : Patrick McDaniel
//  Created : Thu 04 Mar 2021 08:46:09 AM EST
//

// STL-isms
#include <string>
using namespace std;

// Project Includes
#include <bfsFlexibleBuffer.h>
#include <bfs_common.h>
#include <bfs_rawnet.h>

//
// Class definitions

// These are the states the channel can be in
typedef enum {
	SCH_INITIALIZED,
	SCH_CONNECTING,
	SCH_CONNECTED,
	SCH_CLOSED,
	SCH_ERRORED
} SchannelCommState;

// Types of channels available for communication
typedef enum {
	SCH_UKNOWN = 0, // Undetermined communication type
	SCH_SERVER = 1, // Server (listen) communication
	SCH_CLIENT = 2, // Client (send/recv) communication
} SchannelCommType;

//
// Class Definition

class bfsNetworkConnection {

public:
	//
	// Public Interfaces

	// Constructors and destructors

	virtual ~bfsNetworkConnection(void);
	// Timeout constructor

	//
	// Class Methods

	int connect(void);
	// Connect the channel

	int disconnect(void);
	// Disconnect the channel

	bfsNetworkConnection *accept(void);
	// Accept and incoming connection (on server socket)

	//
	// Send the data (not buffer)

	int sendDataL(bfs_size_t len, char *buf);
	// Send data over the connection

	int recvDataL(bfs_size_t len, char *buf);
	// Receive data from the connection

	int sendPacketizedDataL(bfs_size_t len, char *buf);
	// Send data over the connection (with header)

	int recvPacketizedDataL(bfs_size_t len, char *buf);
	// Receive data from the connection (with header)

	//
	// Buffer versions

	int sendBuffer(bfsFlexibleBuffer &buf);
	// Send buffer over the connection

	int recvBuffer(bfsFlexibleBuffer &buf, bfs_size_t len);
	// Receive buffer from the connection (len indicates bytes to recieve)

	int sendPacketizedBuffer(bfsFlexibleBuffer &buf);
	// Send buffer over the connection (with header)

	int recvPacketizedBuffer(bfsFlexibleBuffer &buf);
	// Receive buffer from the connection (with header)

	//
	// Access methods

	// Get the state for the channel
	SchannelCommState getState(void) { return (chState); }

	// Get the type of communications object
	SchannelCommType getType(void) { return (chType); }

	// Get the socket (handle)
	int getSocket(void) { return (socket); }

	//
	// Static methods

	static bfsNetworkConnection *bfsChannelFactory(string addr,
												   unsigned short pt);
	// Client factory (INET domain), string address

	static bfsNetworkConnection *bfsChannelFactory(unsigned short pt);
	// Server factory (INET domain)

protected:
	// Set the state for the channel
	void setState(SchannelCommState st) { chState = st; }

private:
	// Private class methods

	bfsNetworkConnection(void);
	// Default constructor
	// Note: Force all creation to use factory functions

	//
	// Class Data

	SchannelCommState chState;
	// This is the current state of the connection.

	SchannelCommType chType;
	// This it the type of communication object.

	string chAddress;
	// This is the address of the communication (IP, "" if server)

	unsigned short chPort;
	// This is the port for the communications channel

	int socket;
	// This is the socket for the communication.

	//
	// Static Class Variables
};

#endif
