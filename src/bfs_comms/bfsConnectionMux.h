#ifndef BFS_CONNMUX_INCLUDED
#define BFS_CONNMUX_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsConnectionMux.h
//  Description   : This class implements a multiplexer for a set of network
//                  connections, and is used to control the I/O flow for all
//                  the communication.
//
//  Author  : Patrick McDaniel
//  Created : Sun 21 Mar 2021 07:55:36 AM EDT
//

// Project include
#include <bfsNetworkConnection.h>
#include <list>
#include <tuple>

// STL-isms
#include <map>
#include <string>
using namespace std;

// Project Includes

// Declare here and define in mux.cpp so that it is correctly shared between
// non-TEE server+device code (need to define here because its built before
// fs/server code)

// Flag indicting the number of file worker threads (toggles whether the server
// is single threaded or multithreaded)
extern int64_t num_file_worker_threads;

//
// Class types

// A list of connections
typedef map<int, bfsNetworkConnection *> bfsConnectionList;

//
// Class Definition

class bfsConnectionMux {

public:
	//
	// Public Interfaces

	// Constructors and destructors

	bfsConnectionMux(void);
	// Base constructor

	~bfsConnectionMux(void);
	// Base destructor

	//
	// Class Methods

	// Add the connectection object to the list
	void addConnection(bfsNetworkConnection *cn) {
		connections[cn->getSocket()] = cn;
		return;
	}

	// Remove the connection from the list
	void removeConnection(bfsNetworkConnection *cn) {
		connections.erase(cn->getSocket());
	}

	int waitConnections(bfsConnectionList &dataready, uint16_t wait);
	// Wait for incoming data on all of the connections.

	int cleanup(void);
	// Cleanup (and free) all of the connections in the MUX

private:
	//
	// Class Data

	bfsConnectionList connections;
	// The list of the connections for this MUX

	//
	// Static Class Variables
};

#endif
