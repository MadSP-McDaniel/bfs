#ifndef BFS_DEVICE_INCLUDED
#define BFS_DEVICE_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsDevice.h
//  Description   : This is the class describing storage device interface for
//                  the bfs file system.  This is the server side, where each
//                  object represents a device on the network/in a process.
//
//  Author  : Patrick McDaniel
//  Created : Wed 21 Jul 2021 06:47:43 PM EDT
//

// STL-isms
#include <algorithm>
#include <map>
#include <string>
using namespace std;

// Project Includes
#include <bfsSecAssociation.h>
#include <bfs_block.h>
#include <bfs_dev_common.h>

//
// Class definitions

//
// Class types

// Device states

//
// Class Definition

class bfsDevice {

public:
	//
	// Public Interfaces

	// Constructors and destructors

	virtual ~bfsDevice(void) {}
	// Destructor

	//
	// Getter and Setter Methods

	virtual bfs_device_id_t getDeviceIdenfier(void) = 0;
	// Return the device ID

	virtual uint64_t getNumBlocks(void) = 0;
	// Return the number of blocks in the storage

	virtual void setSecurityAssociation(bfsSecAssociation *sa) = 0;
	// Set the security association

	//
	// Class Methods

	virtual int bfsDeviceInitialize(void) = 0;
	// Initialize the device

	virtual int bfsDeviceUninitialize(void) = 0;
	// De-initialze the device

	virtual int getBlock(PBfsBlock &) = 0;
	virtual int getBlock(bfs_block_id_t, char *) = 0;
	// Get a block from the device (at block id)

	virtual int putBlock(PBfsBlock &) = 0;
	// virtual int putBlock(bfs_block_id_t, char *) = 0;
	// Put a block into the device

	virtual int getBlocks(bfs_block_list_t &blks) = 0;
	// Get the blocks associated with the IDS

	virtual int putBlocks(bfs_block_list_t &blks) = 0;
	// Put the blocks associated with the following IDs

	//
	// Static class methods

	//
	// Private class methods

	//
	// Class Data
};

#endif
