////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsDeviceStorageStorage.h
//  Description   : This is the class describing raw storage interface for
//                  the bfs file system.  This is used by all remote and local
//                  storage (device) classes.
//
//  Author  : Patrick McDaniel
//  Created : Wed 21 Jul 2021 04:00:20 PM EDT
//

#ifdef __BFS_ENCLAVE_MODE
#include "bfs_enclave_t.h"
#else
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "bfs_device_ocalls.h"
#endif

#include <bfsConfigLayer.h>
#include <bfsDeviceError.h>
#include <bfsDeviceStorage.h>
#include <bfs_log.h>
#include <bfs_util.h>

//
// Class Functions

/**
 * @brief The attribute constructor for the class
 *
 * @param did - the device ID of this device
 * @param noblocks - the number of blocks to allocation
 * @return int : 0 is success, -1 is failure
 */

bfsDeviceStorage::bfsDeviceStorage(bfs_device_id_t did, uint64_t noblocks)
	: deviceID(did), numBlocks(noblocks), storagePath(""), blockStorage(NULL) {

	// Try to initialize the storage device
	if (bfsDeviceStorageInitialize() != 0) {
		throw new bfsDeviceError("Cannot initialize device storage");
	}

	// Return, no return code
	return;
}

/**
 * @brief The destructor function for the class
 *
 * @param none
 */

bfsDeviceStorage::~bfsDeviceStorage(void) {

	// Return, no return code
	bfsDeviceStorageUninitialize();
	return;
}

/**
 * @brief Get a block from the device
 *
 * @param blkid - the block ID for the block to get (at block id)
 * @param buf - buffer to copy contents into
 * @return int : 0 is success, -1 is failure
 */

char *bfsDeviceStorage::getBlock(bfs_block_id_t blkid, char *blk) {

	// Get address/block, check for error
	char *baddr = getBlockAddress(blkid);
	if (baddr == NULL) {
		logMessage(LOG_ERROR_LEVEL, "Get block failed [%lu]", blkid);
		return (NULL);
	}
	if (blk == NULL) {
		logMessage(LOG_ERROR_LEVEL, "NULL block in get block [%lu]", blkid);
		return (NULL);
	}

	// Log the block get (as needed)
	if (levelEnabled(DEVICE_VRBLOG_LEVEL)) {
		char bstr[129];
		bufToString(blk, BLK_SZ, bstr, 128);
		logMessage(DEVICE_VRBLOG_LEVEL, "Get block: [%d][%s]", blkid, bstr);
	}

	// Copy the data over and return
	memcpy(blk, baddr, BLK_SZ);
	return (blk);
}

/**
 * @brief Put a block into the device
 *
 * @param blkid - the block ID for the block to put (at block id)
 * @param buf - buffer to copy contents into (NULL no copy)
 * @return int : 0 is success, -1 is failure
 */

char *bfsDeviceStorage::putBlock(bfs_block_id_t blkid, char *blk) {

	// Get address/block, check for error
	char *baddr = getBlockAddress(blkid);
	if (baddr == NULL) {
		logMessage(LOG_ERROR_LEVEL, "Put block failed [%lu]", blkid);
		return (NULL);
	}
	if (blk == NULL) {
		logMessage(LOG_ERROR_LEVEL, "NULL block in put block [%lu]", blkid);
		return (NULL);
	}

	// Log the block put (as needed)
	if (levelEnabled(DEVICE_VRBLOG_LEVEL)) {
		char bstr[129];
		bufToString(blk, BLK_SZ, bstr, 128);
		logMessage(DEVICE_VRBLOG_LEVEL, "Put block: [%d][%s]", blkid, bstr);
	}

	// Write the block
	memcpy(baddr, blk, BLK_SZ);
	return (blk);
}

//
// Private class functions

/**
 * @brief The constructor for the class (private)
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

bfsDeviceStorage::bfsDeviceStorage(void) {

	// Return, no return code
	return;
}

/**
 * @brief Initialize the device (memory, network)
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsDeviceStorage::bfsDeviceStorageInitialize(void) {

	// Local variables
	bfsCfgItem *config, *devcfg;
	unsigned short did;
	bool found;
	int i;

	// Verbose log the storage device initialization.
	logMessage(DEVICE_LOG_LEVEL, "Initializing device storage [did=%lu]",
			   deviceID);

	// Pull the configurations
	try {

		// Get the device list from the configuration
#ifdef __BFS_ENCLAVE_MODE
		bfsCfgItem *pathcfg;
		const long path_max_len = 256;
		char storage_path_str[path_max_len] = {0};
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
				pathcfg = NULL;
				if (((ocall_status = ocall_getSubItemByName(
						  (int64_t *)&pathcfg, (int64_t)devcfg, "path",
						  strlen("path") + 1)) != SGX_SUCCESS)) {
					logMessage(LOG_ERROR_LEVEL,
							   "Failed ocall_getSubItemByName");
					return (-1);
				}
				if (((ocall_status = ocall_bfsCfgItemValue(
						  &ret, (int64_t)pathcfg, storage_path_str,
						  path_max_len)) != SGX_SUCCESS) ||
					(ret != BFS_SUCCESS)) {
					logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
					return (-1);
				}
				storagePath = std::string(storage_path_str);
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
				storagePath =
					devcfg->getSubItemByName("path")->bfsCfgItemValue();
				found = true;
			}
		}
#endif

		// If we have not found the configuration, bail out
		if (found == false) {
			throw new bfsDeviceError(
				"Unable to find config for device, aborting");
		}

	} catch (bfsCfgError *e) {
		logMessage(LOG_ERROR_LEVEL, "Failure reading system config : %s",
				   e->getMessage().c_str());
		return (-1);
	}

	// Create the disk storage and move on
	if (createDiskStorage() != 0) {
		logMessage(LOG_ERROR_LEVEL,
				   "Device storage initialized failed [did=%lu].", deviceID);
		return (-1);
	}

	// Return successfully
	logMessage(DEVICE_LOG_LEVEL, "Device storage initialized [did=%lu].",
			   deviceID);
	return (0);
}

/**
 * @brief Create the disk storage region (memory map). This is a wrapper for the
 * actual method that allows us to reuse for enclave and nonenclave code.
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsDeviceStorage::createDiskStorage(void) {
#ifdef __BFS_ENCLAVE_MODE
	if ((ocall_createDiskStorage((uint64_t *)&blockStorage, deviceID,
								 (uint32_t)storagePath.size() + 1,
								 storagePath.c_str(), numBlocks)) !=
		SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_createDiskStorage");
		return BFS_FAILURE;
	}

	if (sgx_is_outside_enclave(blockStorage, numBlocks * BLK_SZ) != 1)
		throw new bfsUtilError("Failed ocall_createDiskStorage: addr range is "
							   "not entirely outside enclave "
							   "(may be corrupt source ptr)");
#else
	blockStorage =
		__createDiskStorage(deviceID, storagePath.c_str(), numBlocks);
#endif

	if (!blockStorage)
		return BFS_FAILURE;

	return BFS_SUCCESS;
}

/**
 * @brief Initializes and creates the memory map for the disk storage.
 *
 * @param deviceID: the device id
 * @param storagePath: path name to the backing file
 * @param numBlocks: the number of disk blocks on the device
 * @return char*: pointer to the base address of the disk storage memory map
 */
#ifndef __BFS_ENCLAVE_MODE
char *__createDiskStorage(bfs_device_id_t deviceID, const char *storagePath,
						  uint64_t numBlocks) {
	char *blockStorage = NULL;

	// Local variables
	uint64_t mapsz = numBlocks * BLK_SZ;
	int mapprot = PROT_READ | PROT_WRITE, mapflgs = MAP_SHARED, mapfh = -1,
		mapoffset = 0, i;
	char zeroblk[BLK_SZ];
	struct stat st;
	bool create = false;

	// See if the file exists
	if (stat(storagePath, &st) == -1) {
		// Does not exist create it
		logMessage(DEVICE_LOG_LEVEL, "Storage file [%s] does not exist",
				   storagePath);
		create = true;
	} else if (st.st_size != (off_t)mapsz) {
		// Wrong file, remove file
		logMessage(DEVICE_LOG_LEVEL,
				   "Storage file [%s] incorrect size, resetting", storagePath);
		unlink(storagePath);
		create = true;
	}

	// If we are creating the file
	if (create == true) {

		// Create the file indicated in the path config item
		logMessage(
			DEVICE_LOG_LEVEL,
			"Creating memory map for device storage [did=%lu, sz=%lu bytes]",
			deviceID, mapsz);
		if ((mapfh = open(storagePath, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG)) ==
			-1) {
			logMessage(
				LOG_ERROR_LEVEL,
				"Device memory map, file open failed, error [%s], path [%s]",
				strerror(errno), storagePath);
			return (NULL);
		}

		// Create the data
		for (i = 0; i < (int)numBlocks; i++) {
			if (write(mapfh, zeroblk, BLK_SZ) != BLK_SZ) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failed writing memory map file content [%s]",
						   strerror(errno));
				return (NULL);
			}
		}

		// Fluch and close the map file
		fsync(mapfh);
		close(mapfh);
		mapfh = -1;
	}

	// Create the file indicated in the path config item
	logMessage(DEVICE_LOG_LEVEL,
			   "Mounting memory map for device storage [did=%lu, sz=%lu bytes]",
			   deviceID, mapsz);
	if ((mapfh = open(storagePath, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG)) ==
		-1) {
		logMessage(LOG_ERROR_LEVEL,
				   "Device memory map, file open failed, error [%s], path [%s]",
				   strerror(errno), storagePath);
		return (NULL);
	}

	// Now do the the memory map, checking result
	blockStorage =
		(char *)mmap(NULL, mapsz, mapprot, mapflgs, mapfh, mapoffset);
	if (blockStorage == MAP_FAILED) {
		logMessage(LOG_ERROR_LEVEL, "Device memory map failed, error [%s]",
				   strerror(errno));
		return (NULL);
	}
	close(mapfh); // Note that this has no impact on the ongoing memory map

	// Return successfully
	logMessage(DEVICE_LOG_LEVEL, "Device storage mounted [%s]", storagePath);
	return (blockStorage);
}
#endif

/**
 * @brief De-initialze the device
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsDeviceStorage::bfsDeviceStorageUninitialize(void) {
#ifdef __BFS_ENCLAVE_MODE
	if ((ocall_deleteDiskStorage((uint64_t)blockStorage, numBlocks)) !=
		SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_createDiskStorage");
		return BFS_FAILURE;
	}
#else
	__deleteDiskStorage(blockStorage, numBlocks);
#endif

	return BFS_SUCCESS;
}

/**
 * @brief Unmaps the memory map for the storage device.
 *
 * @param blockStorage: pointer to unmap
 * @param numBlocks: size of the map (in blocks)
 */
#ifndef __BFS_ENCLAVE_MODE
void __deleteDiskStorage(char *blockStorage, uint64_t numBlocks) {
	// Now just unmap the device memory file
	munmap(blockStorage, numBlocks * BLK_SZ);
	blockStorage = NULL;
}
#endif

/**
 * @brief Get the address of a block in the device
 *
 * @param blkid - the block ID for the block to get
 * @return int : 0 is success, -1 is failure
 */

char *bfsDeviceStorage::getBlockAddress(uint64_t blkid) {

	// Check device memory available
	if (blockStorage == NULL) {
		logMessage(LOG_ERROR_LEVEL,
				   "Getting block address on NULL device memory");
		return (NULL);
	}

	if (blkid >= numBlocks) {
		logMessage(LOG_ERROR_LEVEL,
				   "Getting block address with bad block ID [%lu]", blkid);
		return (NULL);
	}

	// Compute address and return it
	return (blockStorage + (blkid * BLK_SZ));
}
