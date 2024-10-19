#ifndef BFS_DEVICE_LAYER_INCLUDED
#define BFS_DEVICE_LAYER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsDeviceLayer.h
//  Description   : This is the class describing storage device layer interface
//                  for the bfs file system.  Note that this is static class
//                  in which there are no objects.
//
//  Author  : Patrick McDaniel
//  Created : Wed 17 Mar 2021 03:14:31 PM EDT
//

// STL-isms
#include <map>
#include <string>
using namespace std;

// Project Includes
#include <bfsDevice.h>
#include <bfsSecAssociation.h>
#include <bfs_dev_common.h>

/* For performance testing (in enclave only) */
#ifdef __BFS_ENCLAVE_MODE
extern int num_reads;
extern int num_writes;
extern bool collect_core_lats;
extern std::vector<long> s_read__net_d_send_lats, s_write__net_d_send_lats,
	s_other__net_d_send_lats;
extern int op;
extern double f_start_time, f_end_time;
#endif

//
// Class definitions
#define DEVICE_LOG_LEVEL bfsDeviceLayer::getDeviceLayerLogLevel()
#define DEVICE_VRBLOG_LEVEL bfsDeviceLayer::getVerboseDeviceLayerLogLevel()
#define BFS_DEVLYR_CONFIG "bfsDeviceLayer"
#define BFS_DEVLYR_DEVICES_CONFIG "bfsDeviceLayer.devices"

//
// Class Definition

class bfsDeviceLayer {

public:
	//
	// Static methods

	static int
	loadDeviceLayerConfiguration(char *filename); // NOT USED YET
												  // Load a device configuration

	static int getDeviceManifest(bfs_device_list_t &devs);
	// Get the list of devices from the manifest

	static int marshalBfsDevicePacket(bfs_uid_t usr, bfs_device_id_t did,
									  bfs_device_msg_t cmd, bool ack,
									  bfsSecAssociation *sa, uint32_t send_seq,
									  bfsFlexibleBuffer &buf);
	// Marshal the data into the device communication packet

	static int unmarshalBfsDevicePacket(bfs_uid_t &usr, bfs_device_id_t &did,
										bfs_device_msg_t &cmd, bool &ack,
										bfsSecAssociation *sa, uint32_t recv_seq,
										bfsFlexibleBuffer &buf);
	// Unmarshal the data into the device communication packet

	static int bfsDeviceLayerInit(void);
	// Initialize the device layer state

	//
	// Static Class Variables

	// Layer log level
	static unsigned long getDeviceLayerLogLevel(void) {
		return (bfsDeviceLogLevel);
	}

	// Verbose log level
	static unsigned long getVerboseDeviceLayerLogLevel(void) {
		return (bfsVerboseDeviceLogLevel);
	}

	// Return a descriptive string for the state
	static const char *getDeviceStateStr(bfs_device_state_t st) {
		if ((st < 0) || (st >= BFSDEV_MAXSTATE)) {
			return ("<*BAD STATE*>");
		}
		return (bfs_device_state_strings[st]);
	}

	// Return a descriptive string for the message type
	static const char *getDeviceMsgStr(bfs_device_msg_t st) {
		if ((st < 0) || (st >= BFS_DEVICE_MAX_MSG)) {
			return ("<*BAD MESSGGE TYPE*>");
		}
		return (bfs_device_message_strings[st]);
	}

private:
	//
	// Private class methods

	bfsDeviceLayer(void) {}
	// Default constructor (prevents creation of any instance)

	//
	// Static Class Variables

	static unsigned long bfsDeviceLogLevel;
	// The log level for all of the device information

	static unsigned long bfsVerboseDeviceLogLevel;
	// The log level for all of the device information

	static bool bfsDeviceLayerInitialized;
	// Flag indicating whether the device layer has been initialized

	static const char *bfs_device_state_strings[BFSDEV_MAXSTATE];
	// Strings identifying the state of the device

	static const char *bfs_device_message_strings[BFS_DEVICE_MAX_MSG];
	// Strings identifying the message types

	static bfs_device_list_t bfsMasterDeviceList;
	// This is the master list of devices known to the device layer
};

#endif
