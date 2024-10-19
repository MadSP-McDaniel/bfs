#ifndef BFS_NETWORK_DEVICE_INCLUDED
#define BFS_NETWORK_DEVICE_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsNetworkDevice.h
//  Description   : This is the class describing storage device interface for
//                  the bfs file system.  This is the device side of the
//                  communication subsystem.
//
//  Author  : Patrick McDaniel
//  Created : Wed 17 Mar 2021 03:14:31 PM EDT
//

// STL-isms
#include <string>
using namespace std;

// Project Includes
#include <bfsConnectionMux.h>
#include <bfsDeviceError.h>
#include <bfsDeviceLayer.h>
#include <bfsDeviceStorage.h>
#include <bfsNetworkConnection.h>
#include <bfsSecAssociation.h>
#include <bfs_common.h>

//
// Class definitions

//
// Class types

//
// Class Definition

class bfsNetworkDevice {

public:
	//
	// Public Interfaces0

	// Constructors and destructors

	bfsNetworkDevice(bfs_device_id_t did);
	// Device geometry constructor

	virtual ~bfsNetworkDevice(void);
	// Destructor

	//
	// Getter and Setter Methods

	// Return the device ID
	bfs_device_id_t getDeviceIdenfier(void) {
		if (storage == NULL) {
			throw new bfsDeviceError("Using NULL storage device, failed");
		}
		return (storage->getDeviceIdenfier());
	}

	// Return the number of blocks in the storage
	uint64_t getNumBlocks(void) {
		if (storage == NULL) {
			throw new bfsDeviceError("Using NULL storage device, failed");
		}
		return (storage->getNumBlocks());
	}

	// Set the security association
	void setSecurityAssociation(bfsSecAssociation *sa) { secContext = sa; }

	//
	// Class Methods

	int execute(void);
	// Execute the state machine

	// Get a block from the device (at block id)
	char *getBlock(bfs_block_id_t blkid, char *blk) {
		if (storage == NULL) {
			throw new bfsDeviceError("Using NULL storage device, failed");
		}
		return (storage->getBlock(blkid, blk));
	}

	// Put a block into the device
	char *putBlock(bfs_block_id_t blkid, char *blk) {
		if (storage == NULL) {
			throw new bfsDeviceError("Using NULL storage device, failed");
		}
		return (storage->putBlock(blkid, blk));
	}

private:
	// Private class methods

	bfsNetworkDevice(void);
	// Default constructor
	// Note: Force all creation to use factory functions

	int bfsNetworkDeviceInitialize(void);
	// Initialize the device

	int bfsNetworkDeviceUninitialize(void);
	// De-initialze the device

	int changeDeviceState(bfs_device_state_t st);
	// Change the state of the device

	int processCommunications(void);
	// Do the processing for the communications.

	int processClientRequest(bfsNetworkConnection *client,
							 bfsFlexibleBuffer &buf);
	// Process the client request (respond as needed)

	void write_dev_latencies();
	// Log all the latencies to output files

	//
	// Class Data

	bfs_device_state_t devState;
	// The current state of the devices (see enum)

	bfs_device_id_t deviceID;
	// This is the device identifier

	unsigned short commPort;
	// This is the port number on which it is bound.

	char *blockStorage;
	// A pointer to the block storage in memory

	bfsNetworkConnection *serverConn;
	// This is the server socket waiting for incoming connects().

	bfsConnectionMux *serverMux;
	// This is the multiplexer for the server communications.

	bfsSecAssociation *secContext;
	// This is the security association (keys/config)

	bfsDeviceStorage *storage;
	// This is the rate storage interface for the device

	uint32_t nd_send_seq, nd_recv_seq;
};

#endif
