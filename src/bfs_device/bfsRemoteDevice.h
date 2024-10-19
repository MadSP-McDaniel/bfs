#ifndef BFS_REM_DEVICE_INCLUDED
#define BFS_REM_DEVICE_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsRemoteDevice.h.h
//  Description   : This is the class describing storage device interface for
//                  the bfs file system.  This is the server side, where each
//                  object represents a device on the network/in a process.
//
//  Author  : Patrick McDaniel
//  Created : Wed 17 Mar 2021 03:14:31 PM EDT
//

// STL-isms
#include <algorithm>
#include <map>
#include <string>
using namespace std;

// Project Includes
#include <bfsBlockLayer.h>
#include <bfsConnectionMux.h>
#include <bfsDevice.h>
#include <bfsSecAssociation.h>
#include <bfs_dev_common.h>

//
// Class definitions

//
// Class types
class bfsRemoteDevice;
typedef map<bfs_device_id_t, bfsRemoteDevice *> bfsRemoteDeviceList;

// Device states

//
// Class Definition

class bfsRemoteDevice : public bfsDevice {

public:
	//
	// Public Interfaces

	// Constructors and destructors

	bfsRemoteDevice(string address, unsigned short port);
	// Device address constructor

	virtual ~bfsRemoteDevice(void);
	// Destructor

	//
	// Getter and Setter Methods

	// Return the device ID
	virtual bfs_device_id_t getDeviceIdenfier(void) { return (deviceID); }

	// Return the number of blocks in the storage
	virtual uint64_t getNumBlocks(void) { return (numBlocks); }

	// Set the security association
	virtual void setSecurityAssociation(bfsSecAssociation *sa) {
		secContext = sa;
	}

	//
	// Class Methods

	virtual int bfsDeviceInitialize(void);
	// Initialize the device

	virtual int bfsDeviceUninitialize(void);
	// De-initialze the device

	int submit_io(PBfsBlock &, bool = true);
	// Submit an io request to the disk worker

	virtual int getBlock(PBfsBlock &);
	virtual int getBlock(bfs_block_id_t, char *);
	// Get a block from the device (at block id)

	virtual int putBlock(PBfsBlock &);
	// Put a block into the device

	virtual int getBlocks(bfs_block_list_t &blks);
	// Get the blocks associated with the IDS

	virtual int putBlocks(bfs_block_list_t &blks);
	// Put the blocks associated with the following IDs

	//
	// Static class methods

private:
	// Private class methods

	bfsRemoteDevice(void);
	// Default constructor

	int changeDeviceState(bfs_device_state_t st);
	// Change the state of the device

	//
	// Class Data

	bfs_device_state_t devState;
	// The current state of the devices (see enum)

	bfs_device_id_t deviceID;
	// This is the device identifier

	uint64_t numBlocks;
	// The number of blocks in this device

	string commAddress;
	// This is communications address.

	unsigned short commPort;
	// This is the port number on which it is bound.

	bfsNetworkConnection *remoteConn;
	// This is the server socket waiting for incoming connects().

	bfsConnectionMux *remoteMux;
	// This is a multiplexer for the communications

	bfsSecAssociation *secContext;
	// This is the security association (keys/config)

	uint32_t rd_send_seq, rd_recv_seq;
};

#endif
