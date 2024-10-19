/**
 *
 * @file   bfsDeviceLayer.cpp
 * @brief  This is the class implementation for the storage device interface
 *         layer for the bfs file system.  This is really the static methods
 *         and data for the static class.
 *
 */

/* Include files  */
#include <string.h>
#include <vector>

/* Project include files */
#include <bfsConfigLayer.h>
#include <bfsCryptoLayer.h>
#include <bfsDeviceError.h>
#include <bfsDeviceLayer.h>
#include <bfsLocalDevice.h>
#include <bfsRemoteDevice.h>
#include <bfs_log.h>

/* Macros */

/* Globals  */

//
// Class Data

// Create the static class data
unsigned long bfsDeviceLayer::bfsDeviceLogLevel = (unsigned long)0;
unsigned long bfsDeviceLayer::bfsVerboseDeviceLogLevel = (unsigned long)0;
bfs_device_list_t bfsDeviceLayer::bfsMasterDeviceList;

// Strings identifying the state/types for the layer
const char *bfsDeviceLayer::bfs_device_state_strings[BFSDEV_MAXSTATE] = {
	"BFSDEV_UNINITIALIZED",
	"BFSDEV_READY",
	"BFSDEV_ERRORED",
	"BFSDEV_UNKNOWN",
};
const char *bfsDeviceLayer::bfs_device_message_strings[BFS_DEVICE_MAX_MSG] = {
	"BFS_DEVICE_GET_TOPO", "BFS_DEVICE_GET_BLOCK", "BFS_DEVICE_PUT_BLOCK",
	"BFS_DEVICE_GET_BLOCKS", "BFS_DEVICE_PUT_BLOCKS"};

// Static initializer, make sure this is idenpendent of other layers
bool bfsDeviceLayer::bfsDeviceLayerInitialized = false;

//
// Class Functions

/**
 * @brief Get the list of devices from the manifest
 *
 * @param devs - the list of devices to get
 * @return int : 0 is success, -1 is failure
 */

int bfsDeviceLayer::getDeviceManifest(bfs_device_list_t &devs) {

	// Local variables
	bfsDevice *device;
	bfsCfgItem *config, *devcfg, *sacfg;
	bfsSecAssociation *sa;
	bfs_device_id_t did;
	string ipaddr, devtype, path;
	unsigned short port;
	uint64_t devsize;
	int i;

	// If the layer has been initialized, just return master
	if (bfsMasterDeviceList.size() != 0) {
		devs = bfsMasterDeviceList;
		return (0);
	}

	// Pull the configurations
	try {

		// Get the device list from the configuration
#ifdef __BFS_ENCLAVE_MODE
		// Check to make sure we were able to load the configuration
		bfsCfgItem *subcfg;
		const long flag_max_len = 50;
		long _port = 0, _devsize = 0;
		uint64_t _did = 0;
		char _ipaddr[flag_max_len] = {0}, _dev_type[flag_max_len] = {0},
			 _path[flag_max_len] = {0};
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

		// Now walk through the list of devices
		int cfg_item_num_sub = 0;
		if (((ocall_status = ocall_bfsCfgItemNumSubItems(
				  &cfg_item_num_sub, (int64_t)config)) != SGX_SUCCESS)) {
			logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemNumSubItems");
			return (-1);
		}

		for (i = 0; i < cfg_item_num_sub; i++) {

			// Get the device particulars
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
					  (int64_t *)&subcfg, (int64_t)devcfg, "type",
					  strlen("type") + 1)) != SGX_SUCCESS)) {
				logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
				return (-1);
			}
			if (((ocall_status =
					  ocall_bfsCfgItemValue(&ret, (int64_t)subcfg, _dev_type,
											flag_max_len)) != SGX_SUCCESS) ||
				(ret != BFS_SUCCESS)) {
				logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
				return (-1);
			}
			devtype = std::string(_dev_type);
			if (devtype == "local") {
				// Local disk file storage
				subcfg = NULL;
				if (((ocall_status = ocall_getSubItemByName(
						  (int64_t *)&subcfg, (int64_t)devcfg, "did",
						  strlen("did") + 1)) != SGX_SUCCESS)) {
					logMessage(LOG_ERROR_LEVEL,
							   "Failed ocall_getSubItemByName");
					return (-1);
				}
				_did = 0;
				if (((ocall_status = ocall_bfsCfgItemValueUnsigned(
						  &_did, (int64_t)subcfg)) != SGX_SUCCESS) ||
					(_did == 0)) {
					logMessage(LOG_ERROR_LEVEL,
							   "Failed ocall_bfsCfgItemValueUnsigned");
					return (-1);
				}
				did = (uint32_t)_did;

				subcfg = NULL;
				if (((ocall_status = ocall_getSubItemByName(
						  (int64_t *)&subcfg, (int64_t)devcfg, "size",
						  strlen("size") + 1)) != SGX_SUCCESS)) {
					logMessage(LOG_ERROR_LEVEL,
							   "Failed ocall_getSubItemByName");
					return (-1);
				}
				_devsize = 0;
				if (((ocall_status = ocall_bfsCfgItemValueLong(
						  &_devsize, (int64_t)subcfg)) != SGX_SUCCESS) ||
					(_devsize == 0)) {
					logMessage(LOG_ERROR_LEVEL,
							   "Failed ocall_bfsCfgItemValueLong");
					return (-1);
				}
				devsize = _devsize;

				subcfg = NULL;
				if (((ocall_status = ocall_getSubItemByName(
						  (int64_t *)&subcfg, (int64_t)devcfg, "path",
						  strlen("path") + 1)) != SGX_SUCCESS)) {
					logMessage(LOG_ERROR_LEVEL,
							   "Failed ocall_getSubItemByName");
					return (-1);
				}
				if (((ocall_status = ocall_bfsCfgItemValue(
						  &ret, (int64_t)subcfg, _path, flag_max_len)) !=
					 SGX_SUCCESS) ||
					(ret != BFS_SUCCESS)) {
					logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
					return (-1);
				}
				path = std::string(_path);

				device = new bfsLocalDevice(did, path, devsize);
			} else if (devtype == "remote") {
				// Remote device storage
				subcfg = NULL;
				if (((ocall_status = ocall_getSubItemByName(
						  (int64_t *)&subcfg, (int64_t)devcfg, "ip",
						  strlen("ip") + 1)) != SGX_SUCCESS)) {
					logMessage(LOG_ERROR_LEVEL,
							   "Failed ocall_getSubItemByName");
					return (-1);
				}
				if (((ocall_status = ocall_bfsCfgItemValue(
						  &ret, (int64_t)subcfg, _ipaddr, flag_max_len)) !=
					 SGX_SUCCESS) ||
					(ret != BFS_SUCCESS)) {
					logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
					return (-1);
				}
				ipaddr = std::string(_ipaddr);

				subcfg = NULL;
				if (((ocall_status = ocall_getSubItemByName(
						  (int64_t *)&subcfg, (int64_t)devcfg, "port",
						  strlen("port") + 1)) != SGX_SUCCESS)) {
					logMessage(LOG_ERROR_LEVEL,
							   "Failed ocall_getSubItemByName");
					return (-1);
				}
				_port = 0;
				if (((ocall_status = ocall_bfsCfgItemValueLong(
						  &_port, (int64_t)subcfg)) != SGX_SUCCESS) ||
					(_port == 0)) {
					logMessage(LOG_ERROR_LEVEL,
							   "Failed ocall_bfsCfgItemValueLong");
					return (-1);
				}
				port = (unsigned short)_port;

				device = new bfsRemoteDevice(ipaddr, port);
			}

			// Now get the security context (keys etc.)
			sacfg = NULL;
			if (((ocall_status = ocall_getSubItemByName(
					  (int64_t *)&sacfg, (int64_t)devcfg, "sa",
					  strlen("sa") + 1)) != SGX_SUCCESS)) {
				logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
				return (-1);
			}
			sa = new bfsSecAssociation(sacfg);
			device->setSecurityAssociation(sa);
			if (device->bfsDeviceInitialize() != BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failure during bfsDeviceInitialize");
				return BFS_FAILURE;
			}
#else
		// Get the bas configuration
		config = bfsConfigLayer::getConfigItem(BFS_DEVLYR_DEVICES_CONFIG);
		if (config->bfsCfgItemType() != bfsCfgItem_LIST) {
			logMessage(
				LOG_ERROR_LEVEL,
				"Unable to find device configuration in system config : %s",
				BFS_DEVLYR_DEVICES_CONFIG);
			return (-1);
		}

		// Now walk through the list of devices
		for (i = 0; i < config->bfsCfgItemNumSubItems(); i++) {

			// Get the device, then type, type specific configs
			devcfg = config->getSubItemByIndex(i);
			devtype = devcfg->getSubItemByName("type")->bfsCfgItemValue();
			if (devtype == "local") {
				// Local disk file storage
				did = (bfs_device_id_t)devcfg->getSubItemByName("did")
						  ->bfsCfgItemValueUnsigned();
				path = devcfg->getSubItemByName("path")->bfsCfgItemValue();
				devsize =
					devcfg->getSubItemByName("size")->bfsCfgItemValueLong();
				device = new bfsLocalDevice(did, path, devsize);
			} else if (devtype == "remote") {
				// Remote device storage
				ipaddr = devcfg->getSubItemByName("ip")->bfsCfgItemValue();
				port = (unsigned short)devcfg->getSubItemByName("port")
						   ->bfsCfgItemValueLong();
				device = new bfsRemoteDevice(ipaddr, port);
			}

			// Now get the security context (keys etc.), initialize the device
			sacfg = devcfg->getSubItemByName("sa");
			sa = new bfsSecAssociation(sacfg);
			device->setSecurityAssociation(sa);
			if (device->bfsDeviceInitialize() != BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failure during bfsDeviceInitialize");
				return BFS_FAILURE;
			}
#endif

			// Add to the master and device lists
			bfsMasterDeviceList[device->getDeviceIdenfier()] = device;
			devs[device->getDeviceIdenfier()] = device;
		}

	} catch (bfsCfgError *e) {
		logMessage(LOG_ERROR_LEVEL, "Failure reading device system config : %s",
				   e->getMessage().c_str());
		return (-1);
	}

	// Log/set to initialized, return successfully
	logMessage(DEVICE_LOG_LEVEL,
			   "bfsDeviceLayer device list initialized with %d devices.",
			   bfsMasterDeviceList.size());
	bfsDeviceLayerInitialized = true;
	return (0);
}

/**
 * @brief Marshal the data into the device communication packet
 *
 * @param usr - the user ID
 * @param did - the device identifier
 * @param cmd - the device protocol
 * @param ack - ack flag
 * @param buf - the data packet structure
 * @return int : 0 is success, -1 is failure
 */

int bfsDeviceLayer::marshalBfsDevicePacket(bfs_uid_t usr, bfs_device_id_t did,
										   bfs_device_msg_t cmd, bool ack,
										   bfsSecAssociation *sa,
										   uint32_t dev_send_seq,
										   bfsFlexibleBuffer &buf) {

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double dl_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&dl_start_time) != SGX_SUCCESS) ||
	// 			(dl_start_time == -1))
	// 			return BFS_FAILURE;
	// 	}
	// #endif

	// Local variables
	char bstr[129], ccmd;
	bfs_size_t dlen;

	// Sanity check security association
	if (sa == NULL) {
		throw new bfsDeviceError("Cannot marshal with NULL security context");
	}

	// Log, sanity check the values
	if (levelEnabled(DEVICE_VRBLOG_LEVEL)) {
		logMessage(DEVICE_VRBLOG_LEVEL,
				   "Marshaling data (for send) [usr=%lu, did=%lu, cmd=%s, "
				   "ack=%d, len=%d",
				   usr, did, bfsDeviceLayer::getDeviceMsgStr(cmd), ack,
				   buf.getLength());
		if (buf.getLength() > 0) {
			bufToString(buf.getBuffer(), buf.getLength(), bstr, 128);
			logMessage(DEVICE_VRBLOG_LEVEL, "Data marshaled: [%s]", bstr);
		}
	}

	// Add the headers in reverse order, then encrypt/MAC using the SA
	ccmd = (char)cmd;
	dlen = buf.getLength();

	buf << dlen << ack << ccmd << did << usr;

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double dl_end_time = 0.0;
	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&dl_end_time) != SGX_SUCCESS) ||
	// 			(dl_end_time == -1))
	// 			return BFS_FAILURE;

	// 		logMessage(DEVICE_VRBLOG_LEVEL,
	// 				   "===== Time in deviceLayer buf pack: %.3f us",
	// 				   dl_end_time - dl_start_time);

	// 		dl_start_time = dl_end_time; // set to measure encrypt
	// 	}
	// #endif

	bfsFlexibleBuffer *aad =
		new bfsFlexibleBuffer((char *)&dev_send_seq, sizeof(uint32_t));
	sa->encryptData(buf, aad, true);
	delete aad;

	// #ifdef __BFS_ENCLAVE_MODE
	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&dl_end_time) != SGX_SUCCESS) ||
	// 			(dl_end_time == -1))
	// 			return BFS_FAILURE;

	// 		logMessage(DEVICE_VRBLOG_LEVEL,
	// 				   "===== Time in deviceLayer encrypt: %.3f us",
	// 				   dl_end_time - dl_start_time);
	// 	}
	// #endif

	// Return succesfully
	return (0);
}

/**
 * @brief Unmarshal the data from the device communication packet
 *
 * @param usr - the user ID
 * @param did - the device identifier
 * @param cmd - the device protocol
 * @param ack - ack flag
 * @param len - the length of the buffer
 * @param buf - data contents
 * @param pkt - the data packet structure
 * @return int : 0 is success, -1 is failure
 */

int bfsDeviceLayer::unmarshalBfsDevicePacket(
	bfs_uid_t &usr, bfs_device_id_t &did, bfs_device_msg_t &cmd, bool &ack,
	bfsSecAssociation *sa, uint32_t dev_recv_seq, bfsFlexibleBuffer &buf) {

	// Local variables
	char bstr[129], ccmd;
	bfs_size_t dlen;

	// First decrypt/verify MAC
	if (sa == NULL) {
		throw new bfsDeviceError("Cannot unmarshal with NULL security context");
	}

	bfsFlexibleBuffer *aad =
		new bfsFlexibleBuffer((char *)&dev_recv_seq, sizeof(uint32_t));
	sa->decryptData(buf, aad, true);
	delete aad;

	// Pull off the headers, sanity check length
	buf >> usr >> did >> ccmd >> ack >> dlen;
	cmd = (bfs_device_msg_t)ccmd;
	if (buf.getLength() != dlen) {
		logMessage(LOG_ERROR_LEVEL,
				   "Unmarshal buffer length mismatch, failure.");
		return (-1);
	}

	// Log what we received
	if (levelEnabled(DEVICE_VRBLOG_LEVEL)) {
		logMessage(DEVICE_VRBLOG_LEVEL,
				   "Unmarshaling data (for recv) [usr=%lu, did=%lu, cmd=%s, "
				   "ack=%d, len=%d",
				   usr, did, bfsDeviceLayer::getDeviceMsgStr(cmd), ack,
				   buf.getLength());
		if (buf.getLength() > 0) {
			bufToString(buf.getBuffer(), buf.getLength(), bstr, 128);
			logMessage(DEVICE_VRBLOG_LEVEL, "Data unmarshaled: [%s]", bstr);
		}
	}

	// Return succesfully
	return (0);
}

/*
 * @brief Initialize the device layer state
 *
 * @param none
 * @return int : 0 is success, throws exception if failure
 */

int bfsDeviceLayer::bfsDeviceLayerInit(void) {

	// Local variables
	bfsCfgItem *config;
	bool devlog, vrblog;

	// Sanity check
	if (bfsDeviceLayerInitialized) {
		return (0);
	}

	// Call the layer below init functions
	if (bfsUtilLayer::bfsUtilLayerInit() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed bfsUtilLayerInit\n");
		return BFS_FAILURE;
	}

	// Get the device list from the configuration
#ifdef __BFS_ENCLAVE_MODE
	const long log_flag_max_len = 10;
	char log_enabled_flag[log_flag_max_len] = {0},
		 log_verbose_flag[log_flag_max_len] = {0};
	bfsCfgItem *subcfg;
	int64_t ret = 0, ocall_status = 0;

	if (((ocall_status = ocall_getConfigItem(&ret, BFS_DEVLYR_CONFIG,
											 strlen(BFS_DEVLYR_CONFIG) + 1)) !=
		 SGX_SUCCESS) ||
		(ret == (int64_t)NULL)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getConfigItem\n");
		return (-1);
	}
	config = (bfsCfgItem *)ret;

	if (((ocall_status = ocall_bfsCfgItemType(&ret, (int64_t)config)) !=
		 SGX_SUCCESS) ||
		(ret != bfsCfgItem_STRUCT)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Unable to find device configuration in system config "
				   "(ocall_bfsCfgItemType) : %s",
				   BFS_DEVLYR_CONFIG);
		return (-1);
	}

	subcfg = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&subcfg, (int64_t)config, "log_enabled",
			  strlen("log_enabled") + 1)) != SGX_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
		return (-1);
	}
	if (((ocall_status =
			  ocall_bfsCfgItemValue(&ret, (int64_t)subcfg, log_enabled_flag,
									log_flag_max_len)) != SGX_SUCCESS) ||
		(ret != BFS_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
		return (-1);
	}
	devlog = (std::string(log_enabled_flag) == "true");
	bfsDeviceLogLevel = registerLogLevel("DEVICE_LOG_LEVEL", devlog);

	subcfg = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&subcfg, (int64_t)config, "log_verbose",
			  strlen("log_verbose") + 1)) != SGX_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
		return (-1);
	}
	if (((ocall_status =
			  ocall_bfsCfgItemValue(&ret, (int64_t)subcfg, log_verbose_flag,
									log_flag_max_len)) != SGX_SUCCESS) ||
		(ret != BFS_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
		return (-1);
	}
	vrblog = (std::string(log_verbose_flag) == "true");
	bfsVerboseDeviceLogLevel = registerLogLevel("DEVICE_VRBLOG_LEVEL", vrblog);
#else
	config = bfsConfigLayer::getConfigItem(BFS_DEVLYR_CONFIG);

	if (config->bfsCfgItemType() != bfsCfgItem_STRUCT) {
		logMessage(LOG_ERROR_LEVEL,
				   "Unable to find device configuration in system config : %s",
				   BFS_DEVLYR_CONFIG);
		return (-1);
	}
	devlog =
		(config->getSubItemByName("log_enabled")->bfsCfgItemValue() == "true");
	bfsDeviceLogLevel = registerLogLevel("DEVICE_LOG_LEVEL", devlog);
	vrblog =
		(config->getSubItemByName("log_verbose")->bfsCfgItemValue() == "true");
	bfsVerboseDeviceLogLevel = registerLogLevel("DEVICE_VRBLOG_LEVEL", vrblog);
#endif

	// Log the device layer being initialized, return successfully
	bfsDeviceLayerInitialized = true;
	logMessage(DEVICE_LOG_LEVEL, "bfsDeviceLayer initialized. ");
	return (0);
}
