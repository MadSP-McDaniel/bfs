#ifndef BFS_LOCAL_DEVICE_INCLUDED
#define BFS_LOCAL_DEVICE_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsLocalDevice.h.h
//  Description   : This is the class describing storage device interface for
//                  the bfs file system.  This is the local deviceD.
//
//  Author  : Patrick McDaniel
//  Created : Wed 17 Mar 2021 03:14:31 PM EDT
//

/*
// STL-isms
#include <algorithm>
#include <map>
#include <string>
using namespace std;

// Project Includes
#include <bfsBlockLayer.h>
#include <bfsConnectionMux.h>
#include <bfsNetworkConnection.h>
#include <bfsSecAssociation.h>
#include <bfs_dev_common.h>
*/

#include <bfsDevice.h>
#include <bfsDeviceError.h>
#include <bfsDeviceStorage.h>
#include <bfs_block.h>

//
// Class definitions

//
// Class types

// Device states

//
// Class Definition

class bfsLocalDevice : public bfsDevice {

public:
	//
	// Public Interfaces

	// Constructors and destructors

	bfsLocalDevice(bfs_device_id_t did, string path, uint64_t blks);
	// Device address constructor

	virtual ~bfsLocalDevice(void);
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

	// Get a block from the device (at block id)
	virtual int getBlock(PBfsBlock &blk) {
		if (storage == NULL) {
			throw new bfsDeviceError("Using NULL storage device, failed");
		}
		return (storage->getBlock(blk.get_pbid(), blk.getBuffer()) == NULL);
	}

	virtual int getBlock(bfs_block_id_t pbid, char *blk) {
		if (storage == NULL) {
			throw new bfsDeviceError("Using NULL storage device, failed");
		}
		return (storage->getBlock(pbid, blk) == NULL);
	}

	virtual int getBlocks(bfs_block_id_t *pbids, uint32_t nblks, char **blks) {
		if (storage == NULL) {
			throw new bfsDeviceError("Using NULL storage device, failed");
		}

		for (uint32_t i = 0; i < nblks; i++) {
			if (storage->getBlock(pbids[i], blks[i]) == NULL) {
				return -1;
			}
		}
		return 0;
	}

	virtual int getBlocks(bfs_block_list_t &blks);
	// Get the blocks associated with the IDS

	// Put a block into the device
	virtual int putBlock(PBfsBlock &blk) {
		if (storage == NULL) {
			throw new bfsDeviceError("Using NULL storage device, failed");
		}
		return (storage->putBlock(blk.get_pbid(), blk.getBuffer()) == NULL);
	}

	virtual int putBlock(bfs_block_id_t pbid, char *blk) {
		if (storage == NULL) {
			throw new bfsDeviceError("Using NULL storage device, failed");
		}
		return (storage->putBlock(pbid, blk) == NULL);
	}

	virtual int putBlocks(bfs_block_id_t *pbids, uint32_t nblks, char **blks) {
		if (storage == NULL) {
			throw new bfsDeviceError("Using NULL storage device, failed");
		}

		for (uint32_t i = 0; i < nblks; i++) {
			if (storage->putBlock(pbids[i], blks[i]) == NULL) {
				return -1;
			}
		}
		return 0;
	}

	virtual int putBlocks(bfs_block_list_t &blks);
	// Put the blocks associated with the following IDs

	//
	// Static class methods

private:
	// Private class methods

	// Default constructor
	bfsLocalDevice(void) {}

	//
	// Class Data

	bfs_device_id_t deviceID;
	// This is the device identifier

	string storagePath;
	// This is the path to the storage information

	uint64_t numBlocks;
	// The number of blocks in the device

	bfsSecAssociation *secContext;
	// This is the security association (keys/config)

	bfsDeviceStorage *storage;
	// This is the rate storage interface for the device
};

#endif
