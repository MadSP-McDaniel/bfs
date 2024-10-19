/**
 *
 * @file   bfsBlockLayer.cpp
 * @brief  This is the class implementing the block layer interface
 *         for the bfs file system.  Note that this is static class
 *          in which there are no objects.
 * @date   Tue 23 Mar 2021 02:45:47 PM EDT
 *
 */

/* Include files  */
#include <algorithm>

/* Project include files */
#include <bfsBlockError.h>
#include <bfsBlockLayer.h>
#include <bfsConfigLayer.h>
#include <bfsDeviceLayer.h>
#include <bfsVertBlockCluster.h>
#include <bfs_log.h>
#include <bfs_util.h>

//
// Class Data

// Create the static class data
bool bfsBlockLayer::bfsBlockLayerInitialized = false;
bfs_vert_cluster_alloc_t bfsBlockLayer::bfsBlockAllocAlgorithm =
	BFSBLK_MAX_ALLOC;
unsigned long bfsBlockLayer::bfsBlockLogLevel = (unsigned long)0;
unsigned long bfsBlockLayer::bfsVerboseBlockLogLevel = (unsigned long)0;

#if defined(__BFS_ENCLAVE_MODE) || defined(__BFS_DEBUG_NO_ENCLAVE)
bfsVertBlockCluster *bfsBlockLayer::vbc = NULL;
#endif
bfs_vbid_t bfsBlockLayer::num_blocks = 0;

// Strings identifying the state/types for the layer
const char *bfsBlockLayer::bfs_cluster_state_strings[BFSBLK_MAXSTATE] = {
	"BFSBLK_UNINITIALIZED", "BFSBLK_READY", "BFSBLK_ERRORED"};

const char *bfsBlockLayer::bfs_vert_cluster_alloc_strings[BFSBLK_MAX_ALLOC] = {
	"linear", "interleave"};

//
// Class Functions

/*
 * @brief Initialize the block layer state
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsBlockLayer::bfsBlockLayerInit(void) {

	// Local variables
	bfsCfgItem *config, *subcfg;
	bool blklog, vrblog;
	string message;
	int i;

	if (bfsBlockLayerInitialized)
		return BFS_SUCCESS;

	// Only try to init devices if we are initializing in a non-TEE context
	// #ifndef __BFS_ENCLAVE_MODE
	if (bfsDeviceLayer::bfsDeviceLayerInit() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed bfsDeviceLayerInit\n");
		return BFS_FAILURE;
	}
	// #endif

#ifdef __BFS_ENCLAVE_MODE
	const long log_flag_max_len = 10, dsp_max_len = 10;
	char log_enabled_flag[log_flag_max_len] = {0},
		 log_verbose_flag[log_flag_max_len] = {0},
		 _alloc_disc[dsp_max_len] = {0};
	int64_t ret = 0, ocall_status = 0;

	if (((ocall_status = ocall_getConfigItem(&ret, BFS_BLKLYR_CONFIG,
											 strlen(BFS_BLKLYR_CONFIG) + 1)) !=
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
				   "Unable to find block configuration in system config "
				   "(ocall_bfsCfgItemType) : %s",
				   BFS_BLKLYR_CONFIG);
		return (-1);
	}

	// Read in the num_blocks in the config (for lwext4 stuff), this will be
	// explicitly overwritten by the BFS code when a virtual block cluster is
	// created, and the cluster will then have the correct parameters configured
	// under each device.
	if (num_blocks == 0) {
		subcfg = NULL;
		if (((ocall_status = ocall_getSubItemByName(
				  (int64_t *)&subcfg, (int64_t)config, "num_blocks",
				  strlen("num_blocks") + 1)) != SGX_SUCCESS)) {
			logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
			return (-1);
		}
		bfs_vbid_t _num_blocks = 0;
		if (((ocall_status = ocall_bfsCfgItemValueLong(
				  (int64_t *)&_num_blocks, (int64_t)subcfg)) != SGX_SUCCESS) ||
			(_num_blocks == 0)) {
			logMessage(LOG_ERROR_LEVEL,
					   "Failed ocall_bfsCfgItemValueLong _num_blocks");
			return (-1);
		}
		set_num_blocks(_num_blocks);
	}

	if (bfsBlockLogLevel == (unsigned long)0) {
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
		blklog = (std::string(log_enabled_flag) == "true");
		bfsBlockLogLevel = registerLogLevel("BLOCK_LOG_LEVEL", blklog);
	}

	if (bfsVerboseBlockLogLevel == (unsigned long)0) {
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
		bfsVerboseBlockLogLevel =
			registerLogLevel("BLOCK_VRBLOG_LEVEL", vrblog);
	}

	// Get the allocation strategy for the block layer
	subcfg = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&subcfg, (int64_t)config, BFS_BLKLYR_ALLOC_DSP,
			  strlen(BFS_BLKLYR_ALLOC_DSP) + 1)) != SGX_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
		return (-1);
	}

	i = 0;
	while ((bfsBlockAllocAlgorithm == BFSBLK_MAX_ALLOC) &&
		   (i < BFSBLK_MAX_ALLOC)) {
		if (((ocall_status = ocall_bfsCfgItemValue(&ret, (int64_t)subcfg,
												   _alloc_disc, dsp_max_len)) !=
			 SGX_SUCCESS) ||
			(ret != BFS_SUCCESS)) {
			logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
			return (-1);
		}
		if (std::string(_alloc_disc) == bfs_vert_cluster_alloc_strings[i]) {
			bfsBlockAllocAlgorithm = (bfs_vert_cluster_alloc_t)i;
			// break; // should prob break here
		}
		i++;
	}
#else
	// Get the layer configuration
	config = bfsConfigLayer::getConfigItem(BFS_BLKLYR_CONFIG);
	if (config->bfsCfgItemType() != bfsCfgItem_STRUCT) {
		logMessage(LOG_ERROR_LEVEL,
				   "Unable to find block layer configuration : %s",
				   BFS_BLKLYR_CONFIG);
		return (-1);
	}

	// Read in the num_blocks in the config (for lwext4 stuff), this will be
	// explicitly overwritten by the BFS code when a virtual block cluster is
	// created, and the cluster will then have the correct parameters configured
	// under each device.
	if (num_blocks == 0) {
		bfs_vbid_t _num_blocks =
			config->getSubItemByName("num_blocks")->bfsCfgItemValueLong();
		set_num_blocks(_num_blocks);
	}

	// Create the class log level
	if (bfsBlockLogLevel == (unsigned long)0) {
		blklog = (config->getSubItemByName("log_enabled")->bfsCfgItemValue() ==
				  "true");
		bfsBlockLogLevel = registerLogLevel("BLOCK_LOG_LEVEL", blklog);
	}

	// Create the class (verbose) log level
	if (bfsVerboseBlockLogLevel == (unsigned long)0) {
		vrblog = (config->getSubItemByName("log_verbose")->bfsCfgItemValue() ==
				  "true");
		bfsVerboseBlockLogLevel =
			registerLogLevel("BLOCK_VRBLOG_LEVEL", vrblog);
	}

	// Get the allocation strategy for the block layer
	subcfg = config->getSubItemByName(BFS_BLKLYR_ALLOC_DSP);
	i = 0;
	while ((bfsBlockAllocAlgorithm == BFSBLK_MAX_ALLOC) &&
		   (i < BFSBLK_MAX_ALLOC)) {
		if (subcfg->bfsCfgItemValue() == bfs_vert_cluster_alloc_strings[i]) {
			bfsBlockAllocAlgorithm = (bfs_vert_cluster_alloc_t)i;
		}
		i++;
	}
#endif

	// If no algorithm configured, bail out
	if (i >= BFSBLK_MAX_ALLOC) {
		message = "Unknown block allocation algorithm in config : " +
				  subcfg->bfsCfgItemValue();
		throw new bfsBlockError(message);
	}

	// Log the block layer being initialized, return successfully
	logMessage(BLOCK_LOG_LEVEL, "bfsBlockLayer initialized. ");

	bfsBlockLayerInitialized = true;

	return (0);
}

#ifdef __BFS_DEBUG_NO_ENCLAVE
// #if 0
////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsBlockLayerUnitTest
// Description  : Perform a unit test on the block layer implementation
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int bfsBlockLayer::bfsBlockLayerUtest(void) {

	// Local variables
	// bfsVertBlockCluster *cluster;
	bfs_vblock_list_t blist;
	bfs_vblock_list_t::iterator it;
	int slot, start, idx;
	size_t noblks, j;
	uint16_t *blockUsed;
	bfs_vbid_t vaddr;
	vector<int> slots;
	char utstr[129];

	if (bfsBlockLayerInit() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed bfsBlockLayerInit in bfsBlockLayerUtest\n");
		return BFS_FAILURE;
	}

	if (set_vbc(bfsVertBlockCluster::bfsClusterFactory()) != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed to initalize virtual block cluster, aborting.");
		return BFS_FAILURE;
	}

	if (!bfsConfigLayer::systemConfigLoaded()) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed to load system configuration, aborting.\n");
		return BFS_FAILURE;
	}

	// Setup a place to hold the unit test data
	typedef struct {
		bfs_vbid_t blk;			 // The virtual block number used
		char block[BLK_SZ];		 // The block of data to hold
		char checkBlock[BLK_SZ]; // Location of read value for check
	} unit_test_blocks_t;

	// These are the major structures holding unit test state
	unit_test_blocks_t utblks[BFS_DEV_UNIT_TEST_SLOTS];

	// // Call the factory to get the cluster
	// cluster = bfsVertBlockCluster::bfsClusterFactory();
	// if (cluster == NULL) {
	// 	logMessage(LOG_ERROR_LEVEL, "Cluster failed to initalize, aborting.");
	// 	return (-1);
	// }

	// Setup the test structures
	blockUsed =
		(uint16_t *)malloc(sizeof(uint16_t) * get_vbc()->getMaxVertBlocNum());
	for (bfs_vbid_t i = 0; i < get_vbc()->getMaxVertBlocNum(); i++) {
		blockUsed[i] = (uint16_t)-1;
	}
	memset(utblks, 0x0, sizeof(unit_test_blocks_t));
	for (bfs_vbid_t i = 0; i < BFS_DEV_UNIT_TEST_SLOTS; i++) {
		utblks[i].blk = BFS_UTEST_UNUSED;
	}

	// Clear unit test data, cycle through a number of iterations
	for (bfs_vbid_t i = 0; i < BFS_DEV_UNIT_TEST_ITERATIONS; i++) {

		// Read or write blocks
		if (get_random_value(0, 1)) {

			// Writing one or more blocks
			blist.clear();
			slots.clear();
			// noblks = get_random_value(1, 10);
			noblks = 1;
			slot = get_random_value(0, BFS_DEV_UNIT_TEST_SLOTS - 1);

			// Find unique slots, add to the list
			for (j = 0; j < noblks; j++) {

				// Find the unique slot, add to list
				do {
					slot = get_random_value(0, BFS_DEV_UNIT_TEST_SLOTS - 1);
				} while (find(slots.begin(), slots.end(), slot) != slots.end());
				slots.push_back(slot);

				// If previously written, mark virtual block as unused
				if (utblks[slot].blk != BFS_UTEST_UNUSED) {
					blockUsed[utblks[slot].blk] = (uint16_t)-1;
				}

				// Now pick a virtual address (erase previous use of it as
				// needed)
				vaddr = get_random_value(0, get_vbc()->getMaxVertBlocNum() - 1);
				if (blockUsed[vaddr] != (uint16_t)-1) {
					utblks[blockUsed[vaddr]].blk = BFS_UTEST_UNUSED;
				}
				blockUsed[vaddr] = (uint16_t)slot;
				utblks[slot].blk = vaddr;
				get_random_data(utblks[slot].block, BLK_SZ);

				// Add to list of blockls
				blist[vaddr] =
					new VBfsBlock(utblks[slot].block, BLK_SZ, 0, 0, vaddr);
			}

			// Send command based on how many blocks are being sent
			if (blist.size() == 1) {
				if (writeBlock(*(blist.begin()->second)) == BFS_FAILURE) {
					logMessage(LOG_ERROR_LEVEL,
							   "Error writing block to cluster, aborting.");
					return (-1);
				}
			} else {
				// if (cluster->writeBlocks(blist)) {
				// 	logMessage(LOG_ERROR_LEVEL,
				// 			   "Error writing blocks to cluster, aborting.");
				// 	return (-1);
				// }
			}

			// Log the unit test thing
			logMessage(BLOCK_LOG_LEVEL, "Successful writing %u block(s)",
					   blist.size());

		} else {

			// Reading one or more blocks
			blist.clear();
			slots.clear();
			// noblks = get_random_value(1, 10);
			noblks = 1;
			start = slot = get_random_value(0, BFS_DEV_UNIT_TEST_SLOTS - 1);

			// Find used slots, add to blist
			while (blist.size() < noblks) {
				if (utblks[slot].blk != BFS_UTEST_UNUSED) {
					slots.push_back(slot);
					blist[utblks[slot].blk] =
						new VBfsBlock(NULL, BLK_SZ, 0, 0, utblks[slot].blk);
				}

				// Have we exhausted the list of available blocks?
				slot = (slot == BFS_DEV_UNIT_TEST_SLOTS - 1) ? 0 : slot + 1;
				if (slot == start) {
					noblks = blist.size();
				}
			}

			// Confirm we have something to get
			if (blist.size() > 0) {
				// Send command based on how many blocks are being sent
				if (blist.size() == 1) {
					slot = blockUsed[blist.begin()->first];
					if ((readBlock(*(blist.begin()->second)) == BFS_FAILURE)) {
						logMessage(
							LOG_ERROR_LEVEL,
							"Error reading block from cluster, aborting.");
						return (-1);
					}
					// memcpy(utblks[slot].checkBlock,
					// 	   blist.begin()->second->getBuffer(),
					// 	   BLK_SZ);
				} else {
					// if ((cluster->readBlocks(blist))) {
					// 	logMessage(
					// 		LOG_ERROR_LEVEL,
					// 		"Error reading blocks from cluster, aborting.");
					// 	return (-1);
					// }
				}

				// Validate the block(s) we submitted was that which was
				// returned
				idx = 0;
				for (it = blist.begin(); it != blist.end(); it++) {
					slot = blockUsed[it->first];
					// if ( memcmp(utblks[slot].block, utblks[slot].checkBlock,
					// BLK_SZ) != 0 ) {
					if (memcmp(utblks[slot].block,
							   blist[utblks[slot].blk]->getBuffer(),
							   BLK_SZ) != 0) {
						logMessage(
							LOG_ERROR_LEVEL,
							"Retrieved block [%lu] failed match validation.",
							utblks[slot].blk);
						bufToString(utblks[slot].block, BLK_SZ, utstr, 128);
						logMessage(LOG_ERROR_LEVEL, "Failed stored  : [%s]",
								   utstr);
						bufToString(utblks[slot].checkBlock, BLK_SZ, utstr,
									128);
						logMessage(LOG_ERROR_LEVEL, "Failed recevied: [%s]",
								   utstr);
						return (-1);
					}
					idx++;
				}

				// Log the unit test thing
				logMessage(BLOCK_LOG_LEVEL,
						   "Successful get and validated %u block(s).",
						   blist.size());
			}
		}
	}

	// When we have a shutdown method, we will add it here
	// TODO: add layer shutdowm method

	// Log saluation, return succesfully
	logMessage(BLOCK_LOG_LEVEL,
			   "\033[93mBfs block unit test completed successfully.\033[0m\n");
	return (0);
}
#endif
