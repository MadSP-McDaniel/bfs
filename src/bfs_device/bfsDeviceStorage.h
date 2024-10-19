#ifndef BFS_DEVICE_STORAGE_INCLUDED
#define BFS_DEVICE_STORAGE_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsDeviceStorage.h
//  Description   : This is the class describing raw storage interface for
//                  the bfs file system.  This is used by all remote and local
//                  storage (device) classes.
//
//  Author  : Patrick McDaniel
//  Created : Wed 21 Jul 2021 04:00:20 PM EDT
//

// STL-isms
#include <string>
using namespace std;

// Project Includes
#include <bfsDeviceLayer.h>
#include <bfs_common.h>

#define BLK_DEV_FILE "/mnt/externalssd/bfs_dev.bin"

//
// Class definitions

//
// Class types

//
// Helper function prototypes
char *__createDiskStorage(bfs_device_id_t, const char *, uint64_t);
void __deleteDiskStorage(char *, uint64_t);

//
// Class Definition
class bfsDeviceStorage {

public:
	//
	// Public Interfaces0

	// Constructors and destructors

	bfsDeviceStorage(bfs_device_id_t did, uint64_t noblocks);
	// Device geometry constructor

	virtual ~bfsDeviceStorage(void);
	// Destructor

	//
	// Getter and Setter Methods

	// Return the device ID
	bfs_device_id_t getDeviceIdenfier(void) { return (deviceID); }

	// Return the number of blocks in the storage
	uint64_t getNumBlocks(void) { return (numBlocks); }

	//
	// Class Methods

	char *getBlock(bfs_block_id_t blkid, char *blk);
	// Get a block from the device (at block id)

	char *putBlock(bfs_block_id_t blkid, char *blk);
	// Put a block into the device

	// Direct access into the block data (use VERY carefully)
	char *directBlockAccess(bfs_block_id_t blkid) {
		return (getBlockAddress(blkid));
	}

private:
	// Private class methods

	bfsDeviceStorage(void);
	// Default constructor
	// Note: Force all creation to use factory functions

	int bfsDeviceStorageInitialize(void);
	// Initialize the device

	int createDiskStorage(void);
	// Create the disk storage region (memory map)

	int bfsDeviceStorageUninitialize(void);
	// De-initialze the device

	char *getBlockAddress(uint64_t blkid);
	// Get the address of a block in the device

	//
	// Class Data

	bfs_device_id_t deviceID;
	// This is the device identifier

	uint64_t numBlocks;
	// The number of blocks in this device

	string storagePath;
	// The path to the storage file (mmap)

	char *blockStorage;
	// A pointer to the block storage in memory
};

#endif
