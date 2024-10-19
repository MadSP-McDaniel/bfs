/**
 * @file   bfsNetworkDevice.cpp
 * @brief  This is the class implementation for the storage device interface for
		   the bfs file system.  This is the "device" (non-client) side of the
		   interface.
 *
 */

#include <bfs_util.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>

#include <bfsConfigLayer.h>
#include <bfsDeviceError.h>
#include <bfsNetworkDevice.h>
#include <bfs_log.h>

/* For performance testing */
static std::vector<double> d_read__lats, d_read__d_lats, d_read__net_send_lats,
	d_write__lats, d_write__d_lats, d_write__net_send_lats;
static int bfs_device_listener_status = 0;

static void device_signal_handler(int);

//
// Class Functions

/**
 * @brief The attribute constructor for the class
 *
 * @param did - the device ID of this device
 * @param noblocks - the number of blocks to allocation
 * @param port - the port to bind to for incoming connections
 * @return int : 0 is success, -1 is failure
 */

bfsNetworkDevice::bfsNetworkDevice(bfs_device_id_t did)
	: devState(BFSDEV_UNINITIALIZED), deviceID(did), commPort(-1),
	  blockStorage(NULL), serverConn(NULL), serverMux(NULL), secContext(NULL),
	  storage(NULL), nd_send_seq(0), nd_recv_seq(0) {

	// Return, no return code
	return;
}

/**
 * @brief The destructor function for the class
 *
 * @param none
 */

bfsNetworkDevice::~bfsNetworkDevice(void) {

	// Clean up the device objects
	delete secContext;

	// Return, no return code
	return;
}

/**
 * @brief Signal handler for gracefully shutting down the device.
 *
 * @param no: the signal number
 */
void device_signal_handler(int) { bfs_device_listener_status = 0; }

/**
 * @brief Execute the device state machine
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsNetworkDevice::execute(void) {

	// Local variables
	bool shutdown = false;
	int retcode = 0;

	// Register signal for the device
	struct sigaction new_action;
	memset(&new_action, 0x0, sizeof(struct sigaction));
	new_action.sa_handler = device_signal_handler;
	new_action.sa_flags = SA_NODEFER | SA_ONSTACK;
	sigaction(SIGINT, &new_action, NULL);

	// Execute the main device execution loop
	bfs_device_listener_status = 1;
	while (!shutdown) {
		// check if signal was caught and shutdown network device cleanly
		if (!bfs_device_listener_status) {
			shutdown = true;
			continue;
		}

		switch (devState) {
		case BFSDEV_UNINITIALIZED: // Currently uninitialized, set to ready
								   // after init
			if (bfsNetworkDeviceInitialize() == 0) {
				changeDeviceState(BFSDEV_READY);
			} else {
				changeDeviceState(BFSDEV_ERRORED);
			}
			break;

		case BFSDEV_READY: // Connected and receiving commands
			// if shutdown signal caught, just exit OK
			retcode = processCommunications();
			if (retcode == BFS_SHUTDOWN) {
				shutdown = true;
				retcode = BFS_SUCCESS;
			} else if (retcode == BFS_FAILURE) {
				shutdown = true;
			}
			break;

		case BFSDEV_ERRORED: // Devices in error state
			logMessage(LOG_ERROR_LEVEL, "Device in errored state, aborting.");
			shutdown = true;
			retcode = BFS_FAILURE;
			break;

		case BFSDEV_UKNOWN: // Unknown state (likely corrupted)
		default:
			logMessage(LOG_ERROR_LEVEL,
					   "Device in strange state, aborting [%u]", devState);
			shutdown = true;
			retcode = BFS_FAILURE;
			break;
		}
	}

	// Return return code
	return (retcode);
}

//
// Private class functions

/**
 * @brief The constructor for the class (private)
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

bfsNetworkDevice::bfsNetworkDevice(void) {

	// Return, no return code
	return;
}

/**
 * @brief Initialize the device (memory, network)
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsNetworkDevice::bfsNetworkDeviceInitialize(void) {

	// Local variables
	bfsCfgItem *config, *devcfg, *sacfg, *portcfg, *szcfg;
	bfsSecAssociation *sa;
	unsigned short did;
	uint64_t devsz;
	bool found;
	int i;

	// Pull the configurations
	try {

		// Get the device list from the configuration
#ifdef __BFS_ENCLAVE_MODE
		bfsCfgItem *subcfg;
		long _did;
		int64_t ret = 0, ocall_status = 0;

		if (((ocall_status = ocall_getConfigItem(
				  &ret, BFS_DEVLYR_DEVICES_CONFIG,
				  strlen(BFS_DEVLYR_DEVICES_CONFIG) + 1)) != SGX_SUCCESS) ||
			(ret == (int64_t)NULL)) {
			logMessage(LOG_ERROR_LEVEL, "Failed ocall_getConfigItem\n");
			return (-1);
		}
		config = (bfsCfgItem *)ret;

		if (((ocall_status = ocall_bfsCfgItemType(&ret, (int64_t)config)) !=
			 SGX_SUCCESS) ||
			(ret != bfsCfgItem_LIST)) {
			logMessage(LOG_ERROR_LEVEL,
					   "Unable to find device configuration in system config "
					   "(ocall_bfsCfgItemType) : %s",
					   BFS_DEVLYR_DEVICES_CONFIG);
			return (-1);
		}

		// Now walk through the list of devices, find which one is ours
		int cfg_item_num_sub = 0;
		if (((ocall_status = ocall_bfsCfgItemNumSubItems(
				  &cfg_item_num_sub, (int64_t)config)) != SGX_SUCCESS)) {
			logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemNumSubItems");
			return (-1);
		}

		found = false;
		for (i = 0; (i < cfg_item_num_sub) && (!found); i++) {
			devcfg = NULL;
			if (((ocall_status = ocall_getSubItemByIndex((int64_t *)&devcfg,
														 (int64_t)config, i)) !=
				 SGX_SUCCESS)) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failed ocall_bfsCfgItemNumSubItems");
				return (-1);
			}

			subcfg = NULL;
			if (((ocall_status = ocall_getSubItemByName(
					  (int64_t *)&subcfg, (int64_t)devcfg, "did",
					  strlen("did") + 1)) != SGX_SUCCESS)) {
				logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
				return (-1);
			}

			_did = 0;
			if (((ocall_status = ocall_bfsCfgItemValueLong(
					  &_did, (int64_t)subcfg)) != SGX_SUCCESS) ||
				(_did == 0)) {
				logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValueLong");
				return (-1);
			}
			did = (unsigned short)_did;

			if ((bfs_device_id_t)did == deviceID) {
				// Now get and set the security context
				sacfg = NULL;
				if (((ocall_status = ocall_getSubItemByName(
						  (int64_t *)&sacfg, (int64_t)devcfg, "sa",
						  strlen("sa") + 1)) != SGX_SUCCESS)) {
					logMessage(LOG_ERROR_LEVEL,
							   "Failed ocall_getSubItemByName");
					return (-1);
				}

				sa = new bfsSecAssociation(sacfg);
				setSecurityAssociation(sa);
				found = true;
			}
		}
#else
		config = bfsConfigLayer::getConfigItem(BFS_DEVLYR_DEVICES_CONFIG);

		if (config->bfsCfgItemType() != bfsCfgItem_LIST) {
			logMessage(
				LOG_ERROR_LEVEL,
				"Unable to find device configuration in system config : %s",
				BFS_DEVLYR_DEVICES_CONFIG);
			return (-1);
		}

		// Now walk through the list of devices, find which one is ours
		found = false;
		for (i = 0; (i < config->bfsCfgItemNumSubItems()) && (!found); i++) {
			devcfg = config->getSubItemByIndex(i);
			did = (unsigned short)devcfg->getSubItemByName("did")
					  ->bfsCfgItemValueLong();
			if ((bfs_device_id_t)did == deviceID) {

				// Now get and set the security context
				if ((sacfg = devcfg->getSubItemByName("sa")) == NULL) {
					bfsCfgError("Cannot find SA configuration");
				}
				sa = new bfsSecAssociation(sacfg);
				setSecurityAssociation(sa);

				// Now get and set the port
				if ((portcfg = devcfg->getSubItemByName("port")) == NULL) {
					bfsCfgError("Cannot find port configuration");
				}
				commPort = (unsigned short)portcfg->bfsCfgItemValueLong();

				// Now get and set the size (in blocks)
				if ((szcfg = devcfg->getSubItemByName("size")) == NULL) {
					bfsCfgError("Cannot find port configuration");
				}
				devsz = szcfg->bfsCfgItemValueLong();

				// Found it,
				found = true;
			}
		}
#endif

		// If we have not found the configuration, bail out
		if (found == false) {
			throw new bfsDeviceError(
				"Unable to find SA config for device, aborting");
		}

	} catch (bfsCfgError *e) {
		logMessage(LOG_ERROR_LEVEL, "Failure reading system config : %s",
				   e->getMessage().c_str());
		return (-1);
	}

	// Create the storage device
	storage = new bfsDeviceStorage(deviceID, devsz);

	// Now setup the server connection
	serverConn = bfsNetworkConnection::bfsChannelFactory(commPort);
	if (serverConn == NULL) {
		logMessage(LOG_ERROR_LEVEL,
				   "Server connection create failed, aborting.");
		changeDeviceState(BFSDEV_ERRORED);
		return (-1);
	}

	// Connect the server for incoming connections
	if (serverConn->connect()) {
		logMessage(LOG_ERROR_LEVEL,
				   "Server connect for listen failed, aborting.");
		changeDeviceState(BFSDEV_ERRORED);
		return (-1);
	}
	logMessage(DEVICE_LOG_LEVEL,
			   "Device storage server socket connected [did=%lu].", deviceID);

	// Crea the communication MUX
	serverMux = new bfsConnectionMux();
	serverMux->addConnection(serverConn);

	// Return successfully
	logMessage(DEVICE_LOG_LEVEL,
			   "Network device storage initialized [did=%lu].", deviceID);
	return (0);
}

/**
 * @brief De-initialze the device
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsNetworkDevice::bfsNetworkDeviceUninitialize(void) {

	// Cleanup the server socket (close connection, free memory)
	if (serverConn != NULL) {
		serverConn->disconnect();
		delete serverConn;
		serverConn = NULL;
	}

	// Return successfully
	return (0);
}

/**
 * @brief Change the state of the device
 *
 * @param st - the state to change the device into
 * @return int : 0 is success, -1 is failure
 */

int bfsNetworkDevice::changeDeviceState(bfs_device_state_t st) {

	// Sanity check the new state
	if ((st < BFSDEV_UNINITIALIZED) || (st > BFSDEV_UKNOWN)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Trying change device state to nonsense value [%u]", st);
		st = BFSDEV_ERRORED;
	}

	// Just log and change the state of the device
	logMessage(DEVICE_VRBLOG_LEVEL,
			   "Change device [%lu] state from [%s] to [%s]", deviceID,
			   bfsDeviceLayer::getDeviceStateStr(devState),
			   bfsDeviceLayer::getDeviceStateStr(st));
	devState = st;

	// Return successfully
	return (0);
}

void bfsNetworkDevice::write_dev_latencies() {
	// Log total device read latencies
	std::string __d_read__lats = vec_to_str<double>(d_read__lats);
	std::string __d_read__lats_fname(getenv("BFS_HOME"));
	__d_read__lats_fname += "/benchmarks/micro/output/__d" +
							std::to_string(deviceID) + "_read__lats.csv";
	std::ofstream __d_read__lats_f;
	__d_read__lats_f.open(__d_read__lats_fname.c_str(), std::ios::trunc);
	__d_read__lats_f << __d_read__lats.c_str();
	__d_read__lats_f.close();
	logMessage(DEVICE_LOG_LEVEL,
			   "Read latencies device%d (overall, us, %lu records):\n[%s]\n",
			   deviceID, d_read__lats.size(), __d_read__lats.c_str());

	// Log non-network-related device read latencies
	std::string __d_read__d_lats = vec_to_str<double>(d_read__d_lats);
	std::string __d_read__d_lats_fname(getenv("BFS_HOME"));
	__d_read__d_lats_fname += "/benchmarks/micro/output/__d" +
							  std::to_string(deviceID) + "_read__d_lats.csv";
	std::ofstream __d_read__d_lats_f;
	__d_read__d_lats_f.open(__d_read__d_lats_fname.c_str(), std::ios::trunc);
	__d_read__d_lats_f << __d_read__d_lats.c_str();
	__d_read__d_lats_f.close();
	logMessage(
		DEVICE_LOG_LEVEL,
		"Read latencies device%d (non-network, us, %lu records):\n[%s]\n",
		deviceID, d_read__d_lats.size(), __d_read__d_lats.c_str());

	// Log network-related sends for device reads
	std::string __d_read__net_send_lats =
		vec_to_str<double>(d_read__net_send_lats);
	std::string __d_read__net_send_lats_fname(getenv("BFS_HOME"));
	__d_read__net_send_lats_fname += "/benchmarks/micro/output/__d" +
									 std::to_string(deviceID) +
									 "_read__net_send_lats.csv";
	std::ofstream __d_read__net_send_lats_f;
	__d_read__net_send_lats_f.open(__d_read__net_send_lats_fname.c_str(),
								   std::ios::trunc);
	__d_read__net_send_lats_f << __d_read__net_send_lats.c_str();
	__d_read__net_send_lats_f.close();
	logMessage(
		DEVICE_LOG_LEVEL,
		"Read latencies device%d (network sends, us, %lu records):\n[%s]\n",
		deviceID, d_read__net_send_lats.size(),
		__d_read__net_send_lats.c_str());

	// Log total device write latencies
	std::string __d_write__lats = vec_to_str<double>(d_write__lats);
	std::string __d_write__lats_fname(getenv("BFS_HOME"));
	__d_write__lats_fname += "/benchmarks/micro/output/__d" +
							 std::to_string(deviceID) + "_write__lats.csv";
	std::ofstream __d_write__lats_f;
	__d_write__lats_f.open(__d_write__lats_fname.c_str(), std::ios::trunc);
	__d_write__lats_f << __d_write__lats.c_str();
	__d_write__lats_f.close();
	logMessage(DEVICE_LOG_LEVEL,
			   "Write latencies device%d (overall, us, %lu records):\n[%s]\n",
			   deviceID, d_write__lats.size(), __d_write__lats.c_str());

	// Log non-network-related device write latencies
	std::string __d_write__d_lats = vec_to_str<double>(d_write__d_lats);
	std::string __d_write__d_lats_fname(getenv("BFS_HOME"));
	__d_write__d_lats_fname += "/benchmarks/micro/output/__d" +
							   std::to_string(deviceID) + "_write__d_lats.csv";
	std::ofstream __d_write__d_lats_f;
	__d_write__d_lats_f.open(__d_write__d_lats_fname.c_str(), std::ios::trunc);
	__d_write__d_lats_f << __d_write__d_lats.c_str();
	__d_write__d_lats_f.close();
	logMessage(
		DEVICE_LOG_LEVEL,
		"Write latencies device%d (non-network, us, %lu records):\n[%s]\n",
		deviceID, d_write__d_lats.size(), __d_write__d_lats.c_str());

	// Log network-related sends for device writes
	std::string __d_write__net_send_lats =
		vec_to_str<double>(d_write__net_send_lats);
	std::string __d_write__net_send_lats_fname(getenv("BFS_HOME"));
	__d_write__net_send_lats_fname += "/benchmarks/micro/output/__d" +
									  std::to_string(deviceID) +
									  "_write__net_send_lats.csv";
	std::ofstream __d_write__net_send_lats_f;
	__d_write__net_send_lats_f.open(__d_write__net_send_lats_fname.c_str(),
									std::ios::trunc);
	__d_write__net_send_lats_f << __d_write__net_send_lats.c_str();
	__d_write__net_send_lats_f.close();
	logMessage(
		DEVICE_LOG_LEVEL,
		"Write latencies device%d (network sends, us, %lu records):\n[%s]\n",
		deviceID, d_write__net_send_lats.size(),
		__d_write__net_send_lats.c_str());
}

/**
 * @brief Do the processing for the communications via the MUX.
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsNetworkDevice::processCommunications(void) {

	// Local variables
	bfsNetworkConnection *client;
	bfsConnectionList ready;
	bfsConnectionList::iterator it;
	bfsFlexibleBuffer buf;

	// Wait for incoming data
	if (serverMux->waitConnections(ready, 0)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Mux wait failed, aborting device processing.");

		// otherwise it was a signal caught indicating to shutdown, so
		// just log and write the perf results to a file
		if (bfsUtilLayer::perf_test())
			write_dev_latencies();

		logMessage(DEVICE_LOG_LEVEL, "Device shutting down.");

		return BFS_SHUTDOWN;
	}

	// Walk the list of sockets (which have data/processing)
	for (it = ready.begin(); it != ready.end(); it++) {
		if (it->second->getType() == SCH_SERVER) {

			// Accept the connection, add to the mux
			if ((client = it->second->accept()) != NULL) {
				serverMux->addConnection(client);
				logMessage(DEVICE_LOG_LEVEL,
						   "Accepted new client connection [%d]",
						   client->getSocket());
			} else {
				logMessage(LOG_ERROR_LEVEL, "Server accept failed, aborting.");
				return (-1);
			}

		} else if (it->second->getType() == SCH_CLIENT) {

			// Receive the incoming data
			client = it->second;
			if (client->recvPacketizedBuffer(buf) < 0) {
				logMessage(LOG_ERROR_LEVEL,
						   "Client request recv failed, abort.");
				return (-1);
			}

			// Check to see if the client closed
			if (buf.getLength() == 0) {
				// Socket closed, cleanup
				logMessage(DEVICE_LOG_LEVEL,
						   "Connection [%d] closed, cleaning up.", it->first);
				serverMux->removeConnection(client);
				delete client;
				return (0);
			}

			// Process the packet (this is the actual protocol layer)
			if (processClientRequest(client, buf) == -1) {
				return (-1);
			}
		} else {
			// Super weird case where the connection is corrupted/uninitialized
			// (shoud be UNREACHABLE)
			logMessage(LOG_ERROR_LEVEL, "Weird socket in test, aborting");
			return (-1);
		}
	}

	// Return successfully
	return (0);
}

/**
 * @brief Process the client request (respond as needed)
 *
 * @param client - the client we are responding to
 * @param buf - the received packet/buffer
 * @return int : 0 is success, -1 is failure
 */

int bfsNetworkDevice::processClientRequest(bfsNetworkConnection *client,
										   bfsFlexibleBuffer &buf) {

	// Create the request for the remote device information
	bfs_blockid_list_t manifest;
	bfs_blockid_list_t::iterator mit;
	bfs_uid_t usr;
	bfs_device_id_t did;
	bfs_device_msg_t cmd;
	bfs_block_id_t blkid;
	bfsConnectionList ready;
	bfs_device_topo_t topo;
	string blist, msg;
	char bbuf[128];
	size_t i, sz;
	bool ack;
	double d_start_time = 0.0, d_end_time = 0.0, net_send_start_time = 0.0,
		   net_send_end_time = 0.0;

	if (bfsUtilLayer::perf_test())
		d_start_time =
			(double)std::chrono::time_point_cast<std::chrono::microseconds>(
				std::chrono::high_resolution_clock::now())
				.time_since_epoch()
				.count();

	// Unmarshal the data, sanity check it, log it
	if ((bfsDeviceLayer::unmarshalBfsDevicePacket(
			 usr, did, cmd, ack, secContext, nd_recv_seq, buf) == -1) ||
		(usr != 1) || (ack != 0)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Device unmarshal network data failed, abort.");
		return (-1);
	}
	nd_recv_seq++;
	logMessage(DEVICE_VRBLOG_LEVEL, "Message [%s] received from user [%lu]",
			   bfsDeviceLayer::getDeviceMsgStr(cmd), usr);

	// Sanity check the data
	if (ack != 0) {
		logMessage(LOG_ERROR_LEVEL,
				   "Device request ack flag bad, aborting request.");
		return (-1);
	}

	// Now switch on the type of request being sent by the remote requestor
	switch (cmd) {

	case BFS_DEVICE_GET_TOPO: // Get the device topology information
		if (buf.getLength() != 0) {
			logMessage(LOG_ERROR_LEVEL, "Bad length in topology request. [%u]",
					   buf.getLength());
			return (-1);
		}
		memset(&topo, 0x0, sizeof(bfs_device_topo_t));
		did = topo.did = deviceID;
		topo.nblks = storage->getNumBlocks();
		buf.setData((char *)&topo, sizeof(bfs_device_topo_t));
		break;

	case BFS_DEVICE_GET_BLOCK: // Get a block from the device
		if (buf.getLength() != sizeof(bfs_block_id_t)) {
			logMessage(LOG_ERROR_LEVEL, "Bad length in device get block. [%u]",
					   buf.getLength());
			return (-1);
		}
		if (did != deviceID) {
			logMessage(LOG_ERROR_LEVEL,
					   "Device ID mismatch in device get block. [%lu!=%lu]",
					   did, deviceID);
			return (-1);
		}
		blkid = *(bfs_block_id_t *)buf.getBuffer();
		buf.resetWithAlloc(BLK_SZ);
		getBlock(blkid, buf.getBuffer());
		buf << blkid;
		break;

	case BFS_DEVICE_PUT_BLOCK: // Put a block into the device
		if (buf.getLength() != sizeof(bfs_block_id_t) + BLK_SZ) {
			logMessage(LOG_ERROR_LEVEL, "Bad length in device put block. [%u]",
					   buf.getLength());
			return (-1);
		}
		if (did != deviceID) {
			logMessage(LOG_ERROR_LEVEL,
					   "Device ID mismatch in device get block. [%lu!=%lu]",
					   did, deviceID);
			return (-1);
		}
		buf >> blkid;
		putBlock(blkid, buf.getBuffer());
		buf.setData((char *)&blkid, sizeof(bfs_block_id_t));
		break;

	case BFS_DEVICE_GET_BLOCKS: // Get a set of blocks from the device
		// Walk and get the block identifiers
		buf >> sz;
		for (i = 0; i < sz; i++) {
			buf >> blkid;
			manifest.push_back(blkid);
		}

		// Now setup the response
		buf.burn();
		buf << sz;
		for (mit = manifest.begin(); mit != manifest.end(); mit++) {
			buf.addTrailer(*mit);
			buf.addTrailer(storage->directBlockAccess(*mit), BLK_SZ);
		}

		// Log, possibly list blocks
		if (levelEnabled(DEVICE_VRBLOG_LEVEL)) {
			msg = "";
			for (mit = manifest.begin(); mit != manifest.end(); mit++) {
				bufToString(storage->directBlockAccess(*mit), 2, bbuf, 128);
				msg += " : " + to_string(*mit) + " (" + bbuf + ")";
			}
		}
		logMessage(DEVICE_LOG_LEVEL,
				   "Server requesting (get blocsk) %u blocks server%s ", sz,
				   msg.c_str());
		break;

	case BFS_DEVICE_PUT_BLOCKS: // Put a set of blocks into the device
		// Walk and put the blocks
		buf >> sz;
		for (i = 0; i < sz; i++) {
			buf >> blkid;
			buf.removeHeader(storage->directBlockAccess(blkid), BLK_SZ);
			manifest.push_back(blkid);
		}

		// Now setup the response
		buf.burn();
		buf << sz;
		for (mit = manifest.begin(); mit != manifest.end(); mit++) {
			buf.addTrailer(*mit);
		}

		// Log, possibly list blocks
		if (levelEnabled(DEVICE_VRBLOG_LEVEL)) {
			msg = "";
			for (mit = manifest.begin(); mit != manifest.end(); mit++) {
				bufToString(storage->directBlockAccess(*mit), 2, bbuf, 128);
				msg += " : " + to_string(*mit) + " (" + bbuf + ")";
			}
		}
		logMessage(DEVICE_LOG_LEVEL, "Server requesting (put blocks) %u%s", sz,
				   msg.c_str());
		break;

	default: // Unknown command
		logMessage(LOG_ERROR_LEVEL,
				   "Unknown command received from remote user [%d], error.",
				   cmd);
		return (-1);
	}

	if (bfsUtilLayer::perf_test())
		net_send_start_time =
			(double)std::chrono::time_point_cast<std::chrono::microseconds>(
				std::chrono::high_resolution_clock::now())
				.time_since_epoch()
				.count();

	// Send the packet
	if ((bfsDeviceLayer::marshalBfsDevicePacket(usr, did, cmd, 1, secContext,
												nd_send_seq, buf) == -1) ||
		((size_t)client->sendPacketizedBuffer(buf) != buf.getLength())) {
		logMessage(LOG_ERROR_LEVEL,
				   "Device response failed to marshal/send, abort.");
		return (-1);
	}
	nd_send_seq++;

	if (bfsUtilLayer::perf_test()) {
		net_send_end_time =
			(double)std::chrono::time_point_cast<std::chrono::microseconds>(
				std::chrono::high_resolution_clock::now())
				.time_since_epoch()
				.count();
		d_read__net_send_lats.push_back(net_send_end_time -
										net_send_start_time);
	}

	if (bfsUtilLayer::perf_test()) {
		d_end_time =
			(double)std::chrono::time_point_cast<std::chrono::microseconds>(
				std::chrono::high_resolution_clock::now())
				.time_since_epoch()
				.count();

		switch (cmd) {
		case BFS_DEVICE_GET_BLOCK:
			d_read__net_send_lats.push_back(net_send_end_time -
											net_send_start_time);
			d_read__d_lats.push_back((d_end_time - d_start_time) -
									 (net_send_end_time - net_send_start_time));
			d_read__lats.push_back(d_end_time - d_start_time);
			break;
		case BFS_DEVICE_PUT_BLOCK:
			d_write__net_send_lats.push_back(net_send_end_time -
											 net_send_start_time);
			d_write__d_lats.push_back(
				(d_end_time - d_start_time) -
				(net_send_end_time - net_send_start_time));
			d_write__lats.push_back(d_end_time - d_start_time);
			break;
		default:
			break;
		}
	}

	return (0);
}
