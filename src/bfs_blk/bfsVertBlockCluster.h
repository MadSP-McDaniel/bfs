#ifndef BFS_VERT_BLOCK_CLUSTER_INCLUDED
#define BFS_VERT_BLOCK_CLUSTER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsVertBlockCluster.h
//  Description   : This is the class providing a virtual interface to a cluster
//                  of remote block devices within the BFS file system.
//
//  Author  : Patrick McDaniel
//  Created : Mon 29 Mar 2021 03:49:54 PM EDT
//

// STL-isms
#include <string>
#include <vector>
using namespace std;

// Project Includes
#include "bfs_block.h"
#include <bfsDeviceLayer.h>
#include <bfs_cache.h>

//
// Class definitions

//
// Class types
// Device cluster state
typedef enum {
	BFSBLK_UNINITIALIZED = 0, // Cluster uninitalized
	BFSBLK_READY = 1,		  // Cluster ready and running
	BFSBLK_ERRORED = 2,		  // Cluster in errored state
	BFSBLK_MAXSTATE = 3,	  // Guard state
} bfs_vert_cluster_state_t;

// An ordered list of remote block devices
typedef vector<bfsDevice *> bfs_device_vec_t;

// The block allocation structure
typedef struct {
	bool used;				// Flag indicating that this block is being used
	bfs_device_id_t device; // The device on which the block is placed
	bfs_block_id_t block;	// The block number on the remote device
	uint64_t timestamp;
} blk_alloc_entry;

//
// Class Definition

class bfsVertBlockCluster {

public:
	//
	// Public Interfaces

	// Constructors and destructors

	~bfsVertBlockCluster(void);
	// Destructor

	//
	// Getter and Setter Methods

	// Return the number of blocks in the storage
	bfs_vbid_t getMaxVertBlocNum(void) { return (maxBlockID); }

	// Returns the list of devices in the cluster
	bfs_device_vec_t &get_devices(void) { return devices; }

	const BfsCache &get_blk_cache();
	// Get reference to the block cache object

	//
	// Class Methods

	// int verify_block_freshness(bfs_block_id_t);

	int verify_parent_block();

	int update_parent_block();

	int readBlock_helper(bfs_vbid_t, PBfsBlock **);
	// replaced by blockLayer method
	// int readBlock(VBfsBlock &blk);
	// Read a block of data from the cluster

	int writeBlock_helper(bfs_vbid_t, PBfsBlock *, op_flags_t = _bfs__NONE);
	// replaced by blockLayer method
	// int writeBlock(VBfsBlock &blk, op_flags_t = _bfs__NONE);
	// Write a block to the cluster at a virtual block address

	int readBlocks(bfs_vblock_list_t &blks);
	// Read a set of blocks of data from the cluster

	int writeBlocks(bfs_vblock_list_t &blks);
	// Write a set of blocks to the cluster

	// LIKELY DONT NEED THIS SINCE FS DECIDES ALLOCS (edit: need it for block
	// allocation at blk layer)
	// int deallocBlock(bfs_vbid_t id);
	// Release a previously allocated block

	uint64_t get_block_timestamp(bfs_vbid_t vbid) {
		// return timestamp_tab.at(vbid);
		return blkAllocTable[vbid].timestamp;
	}

	void inc_block_timestamp(bfs_vbid_t vbid) {
		// auto tmp = timestamp_tab[vbid];
		// timestamp_tab[vbid] = tmp + 1;
		auto tmp = blkAllocTable[vbid].timestamp;
		blkAllocTable[vbid].timestamp = tmp + 1;
	}

	// void init_ts_table(bfs_vbid_t num_blks) {
	// 	for (bfs_vbid_t i = 0; i < num_blks; i++)
	// 		timestamp_tab.insert(std::make_pair(i, 0));
	// }

	//
	// Static class methods

	static bfsVertBlockCluster *bfsClusterFactory(void);
	// The factory function for the cluster

private:
	// Private class methods

	bfsVertBlockCluster(void);
	// Default constructor
	// Note: Force all creation to use factory functions

	int bfsVertBlockClusterInitialize(void);
	// Initialize the device

	int bfsVertBlockClusterUninitialize(void);
	// De-initialze the device

	int changeClusterState(bfs_vert_cluster_state_t st);
	// Change the state of the device

	int addBlockDevice(bfsDevice *dev);
	// Add a block device to the cluster

	int getPhyBlockAddr(bfs_vbid_t addr, bfsDevice *&dev, bfs_block_id_t &blk);
	// Get the physical address associated with a virtual block address

	void flush_blk(PBfsBlock *, bfs_vbid_t);
	// Cleanup callback for dirty blocks

	//
	// Class Data

	bfs_vert_cluster_state_t clusterState;
	// The current state of the cluster (see enum)

	bfs_vbid_t maxBlockID;
	// The number of virtual blocks in this cluster

	bfs_device_vec_t devices;
	// The list of available block devices on which to rest the cluster

	blk_alloc_entry *blkAllocTable;
	// The allocation of blocks in the cluster

	BfsCache blk_cache; /* block cache */
};

#endif
