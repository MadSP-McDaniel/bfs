#ifndef BFS_DEVICE_COMMON_INCLUDED
#define BFS_DEVICE_COMMON_INCLUDED

////////////////////////////////////////////////////////////////////////////////
// 1
//  File          : bfs_dev_common.h
//  Description   : This include file defines a number of types for the device
//                  layer of the BFS device layer.
//
//  Author  : Patrick McDaniel
//  Created : Tue 23 Mar 2021 03:04:38 PM EDT
//

// STL-isms
#include <map>
using namespace std;

// Project Includes
#include <bfs_common.h>

//
// Class definitions
#define MAX_DEV_PACKET_LEN                                                     \
	sizeof(bfs_uid_t) + sizeof(bfs_uid_t) + sizeof(char) + sizeof(char) +      \
		sizeof(uint16_t) + sizeof(bfs_block_id_t) + BLK_SZ

//
// Class types

// The list of devices (for topo checking)
class bfsDevice;
typedef map<bfs_device_id_t, bfsDevice *> bfs_device_list_t;

// Device states
typedef enum {
	BFSDEV_UNINITIALIZED = 0, // Currently uninitialized
	BFSDEV_READY = 1,		  // Connected and receiving commands
	BFSDEV_ERRORED = 2,		  // Devices in error state
	BFSDEV_UKNOWN = 3,		  // Unknown state (likely corrupted)
	BFSDEV_MAXSTATE = 4		  // Guard state (unused)
} bfs_device_state_t;

// Message/command types
typedef enum {
	BFS_DEVICE_GET_TOPO = 0, // Get the topography/configuration of the device
	BFS_DEVICE_GET_BLOCK,	 // Get a block from the device
	BFS_DEVICE_PUT_BLOCK,	 // Push a block onto the device
	BFS_DEVICE_GET_BLOCKS,	 // Get a selection of blocks
	BFS_DEVICE_PUT_BLOCKS,	 // Put a set of blocks
	BFS_DEVICE_PUT_BLOCK_TAGGED,
	BFS_DEVICE_GET_BLOCK_TAGGED,
	BFS_DEVICE_MAX_MSG // Guard value
} bfs_device_msg_t;

// A device packet for the disk-worker job queue (note: data also contains some
// of the params for the device to unpack as well)
typedef struct {
	char *req_data;
	bfs_block_id_t pbid;
	size_t len;
	void *remoteMux;
	void *remoteConn;
    int ready; // flag indicating if job is done
    int sync;
} device_req_packet_t;

// The topo information for the device
typedef struct {
	bfs_device_id_t did; // The disk ID
	uint64_t nblks;		 // The number of blocks
} bfs_device_topo_t;

#endif
