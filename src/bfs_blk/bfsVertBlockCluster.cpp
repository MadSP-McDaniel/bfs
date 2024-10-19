/**
 *
 * @file   bfsVertBlockCluster.cpp
 * @brief  This is the class providing a virtual interface to a cluster
 *         of remote block devices within the BFS file system.
 * @date   Mon 29 Mar 2021 03:49:54 PM EDT
 *
 */

/* Include files  */
#include <string.h>
#ifdef __BFS_NONENCLAVE_MODE
#include <sys/mman.h>
#endif

/* Project include files */
#include "bfsBlockLayer.h"
#include <bfsBlockError.h>
#include <bfsVertBlockCluster.h>
#include <bfs_log.h>
#include <chrono>

/* Macros */

/* Globals  */

//
// Class Data

//
// Class Functions

/**
 * @brief The destructor for the class
 *
 * @param none
 * @return none
 */

bfsVertBlockCluster::~bfsVertBlockCluster(void) {

	// De initialize the cluser object
	bfsVertBlockClusterUninitialize();

	// Return, no return code
	return;
}

/**
 * @brief Helper method to read a block of data from the cluster
 *
 * @param blk - the virtual block object to fill
 * @return the block or NULL if failure
 */
int bfsVertBlockCluster::readBlock_helper(bfs_vbid_t vbid, PBfsBlock **pblk) {

	// Local variables
	bfsDevice *dev;
	bfs_block_id_t pbid;
	bool cache_hit = false;

	// #ifndef __BFS_ENCLAVE_MODE
	// 	double vbc_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test())
	// 		vbc_start_time =
	// 			(double)std::chrono::time_point_cast<std::chrono::microseconds>(
	// 				std::chrono::high_resolution_clock::now())
	// 				.time_since_epoch()
	// 				.count();
	// #endif

	// Get the physical address of the block on the devices
	if (getPhyBlockAddr(vbid, dev, pbid)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Bad virtual block address in read block [%lu]", vbid);
		return (BFS_FAILURE);
	}

	////////////////////////////////////////////////////////////

	// Check the block cache for the block first, otherwise get the block from
	// the device and copy to the vblock object. Note that here we do the
	// untrusted->trusted memory copy from the pblock into the vblock
	PBfsBlock *cached_blk = NULL;
	CacheableObject *obj;

	// #ifndef __BFS_ENCLAVE_MODE
	// 	double vbc_buf_end_time = 0.0;

	// 	if (bfsUtilLayer::perf_test()) {
	// 		vbc_buf_end_time =
	// 			(double)std::chrono::time_point_cast<std::chrono::microseconds>(
	// 				std::chrono::high_resolution_clock::now())
	// 				.time_since_epoch()
	// 				.count();
	// 		logMessage(BLOCK_VRBLOG_LEVEL,
	// 				   "===== Time in vbc readblock buf init: %.3f us",
	// 				   vbc_buf_end_time - vbc_start_time);
	// 	}

	// 	double vbc_read_start_time = vbc_buf_end_time;
	// #endif

	// cache on vbids now and get the pbid object
	if (!bfsUtilLayer::cache_enabled() ||
		(bfsUtilLayer::cache_enabled() &&
		 !(obj = blk_cache.checkCache(intCacheKey(vbid), 1, false)))) {
		// pblk = new PBfsBlock(NULL, BLK_SZ, 0, 0, pbid, dev);

		// if it wasnt cached, do the getblock normally and then (potentially)
		// cache it
		// if (dev->getBlock(*pblk)) {
		// 	logMessage(
		// 		LOG_ERROR_LEVEL,
		// 		"Failed getting virtual block [%lu] from physical [%lu/%d]",
		// 		vbid, pblk->get_pbid(), dev->getDeviceIdenfier());
		// 	return (BFS_FAILURE);
		// }

		// init a new block object and retrieve block from storage
		*pblk = new PBfsBlock(NULL, BLK_SZ, 0, 0, 0, 0);

		// set pbid appropriately
		(*pblk)->set_pbid(pbid);

		// set back pointer so block can be flushed appropriately
		(*pblk)->set_rd(dev);

		if (dev->getBlock(pbid, (*pblk)->getBuffer())) {
			logMessage(
				LOG_ERROR_LEVEL,
				"Failed getting virtual block [%lu] from physical [%lu/%d]",
				vbid, pbid, dev->getDeviceIdenfier());
			return (BFS_FAILURE);
		}

		if (bfsUtilLayer::cache_enabled() &&
			(obj = blk_cache.insertCache(intCacheKey(vbid), 1, *pblk))) {
			// sanity check
			if (!(cached_blk = dynamic_cast<PBfsBlock *>(obj))) {
				logMessage(LOG_ERROR_LEVEL, "Failed cast for block ptr\n");
				return BFS_FAILURE;
			}

			if (cached_blk != *pblk) {
				flush_blk(cached_blk, vbid);
				delete cached_blk;
			}
		}
	} else {
		// otherwise, the block was cached
		// sanity check
		if (bfsUtilLayer::cache_enabled())
			cache_hit = true;
		logMessage(BLOCK_VRBLOG_LEVEL, "cache hit on block [vbid=%lu]\n", vbid);
		if (!(*pblk = dynamic_cast<PBfsBlock *>(obj))) {
			logMessage(LOG_ERROR_LEVEL, "Failed cast for block ptr\n");
			return BFS_FAILURE;
		}

		// memcpy((*pblk)->getBuffer(), cached_blk->getBuffer(),
		// 	   cached_blk->getLength());
	}

	// #ifndef __BFS_ENCLAVE_MODE
	// 	double vbc_read_end_time = 0.0;

	// 	if (bfsUtilLayer::perf_test()) {
	// 		vbc_read_end_time =
	// 			(double)std::chrono::time_point_cast<std::chrono::microseconds>(
	// 				std::chrono::high_resolution_clock::now())
	// 				.time_since_epoch()
	// 				.count();

	// 		logMessage(BLOCK_VRBLOG_LEVEL,
	// 				   "===== Time in vbc readblock getblock: %.3f us",
	// 				   vbc_read_end_time - vbc_read_start_time);
	// 	}
	// #endif

	////////////////////////////////////////////////////////////

	// if (dev->getBlock(pbid, pblk_buf)) {
	// 	logMessage(LOG_ERROR_LEVEL,
	// 			   "Failed getting virtual block [%lu] from physical [%lu/%d]",
	// 			   vbid, pbid, dev->getDeviceIdenfier());
	// 	return (BFS_FAILURE);
	// }

	// copy the data over to the pblock buffer staging area that the TEE knows
	// about (later need to optimize out most of these copies); then back in the
	// readBlock do the final copy into the vblock buffer for decrypting
	// memcpy(pblk_buf, pblk->getBuffer(), pblk->getLength());
	// vblk.setData(pblk->getBuffer(), pblk->getLength());

	// // Return sucesfully (the buffer containing the data)
	logMessage(BLOCK_VRBLOG_LEVEL,
			   "Succesfully got virtual block [%lu] from physical [%lu/%d]",
			   vbid, (*pblk)->getBuffer(), dev->getDeviceIdenfier());
	logMessage(BLOCK_VRBLOG_LEVEL, "Block cache hit rate: %.2f%%\n",
			   blk_cache.get_hit_rate() * 100.0);

	// Note: since we are only doing single-threading for now, just
	// release lock here; otherwise we should let the fs code release
	// lock when it is actually finished with a transaction
	if ((*pblk) && !(*pblk)->unlock()) {
		logMessage(LOG_ERROR_LEVEL, "Failed unlocking block ptr\n");
		return BFS_FAILURE;
	}

	// if (!bfsUtilLayer::cache_enabled())
	// 	delete pblk;

	// // verify freshness of block
	// if (verify_block_freshness(pbid) != BFS_SUCCESS) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed merkle tree check");
	// 	return BFS_FAILURE;
	// }

	// #ifndef __BFS_ENCLAVE_MODE
	// 	double vbc_end_time = 0.0;

	// 	if (bfsUtilLayer::perf_test()) {
	// 		vbc_end_time =
	// 			(double)std::chrono::time_point_cast<std::chrono::microseconds>(
	// 				std::chrono::high_resolution_clock::now())
	// 				.time_since_epoch()
	// 				.count();
	// 		logMessage(BLOCK_VRBLOG_LEVEL,
	// 				   "===== Time in vbc readblock total: %.3f us",
	// 				   vbc_end_time - vbc_start_time);
	// 	}
	// #endif

	return cache_hit ? BFS_SUCCESS_CACHE_HIT : BFS_SUCCESS;
}

/**
 * @brief Read a block of data from the cluster
 *
 * @param blk - the virtual block object to fill
 * @return the block or NULL if failure
 */

// int bfsVertBlockCluster::readBlock(VBfsBlock &vblk) {
// 	long ret = -1;

// 	if (vblk.getBuffer() == NULL) {
// 		return (BFS_FAILURE);
// 	}

// 	PBfsBlock *pblk = new PBfsBlock(NULL, BLK_SZ, 0, 0, 0, 0);

// #ifdef __BFS_ENCLAVE_MODE
// 	// give untrusted routine the raw char* to fill in with pblk instead of
// 	// passing the pblock object; much more efficient; expected for the
// 	// buffer to be exactly 4KB
// 	long ocall_ret = 0;
// 	if ((ocall_readBlock(&ocall_ret, vblk.get_vbid(),
// 						 (void *)pblk->getBuffer()) != SGX_SUCCESS))
// 		return BFS_FAILURE;
// 	ret = ocall_ret;
// #else
// 	// just call helper directly for debug mode
// 	ret = readBlock_helper(vblk.get_vbid(), pblk->getBuffer());

// #endif

// 	if (ret == BFS_SUCCESS)
// 		vblk.setData(pblk->getBuffer(), pblk->getLength());

// 	return ret;
// }

/**
 * @brief Helper method to write a block of data to the cluster
 *
 * @param blk - the virtual block object to write
 * @return the block or NULL if failure
 */
int bfsVertBlockCluster::writeBlock_helper(bfs_vbid_t vbid, PBfsBlock *pblk,
										   op_flags_t flags) {

	// Local variables
	bfsDevice *dev;
	bfs_block_id_t pbid;
	CacheableObject *obj;
	bool cache_hit = false;

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double vbc_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&vbc_start_time) != SGX_SUCCESS) ||
	// 			(vbc_start_time == -1))
	// 			return BFS_FAILURE;
	// 	}
	// #endif

	// Get the physical address of the block on the devices
	if (getPhyBlockAddr(vbid, dev, pbid)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Bad virtual block address in write block [%lu]", vbid);
		return (-1);
	}

	// set pbid appropriately
	pblk->set_pbid(pbid);

	// set back pointer so block can be flushed appropriately
	pblk->set_rd(dev);

	// Put the block in the cache, and if write-through flag included then also
	// synch with the device. For now we just copy the virtual block data into a
	// physical block object; however, later we will insert new schemes for
	// block management here.
	// create new pblock object from the raw buffer created by the TEE (just
	// copy over for now)
	// TODO: revisit to manage memory better (currently creates a new pblock
	// from the buffer of the underlying pblock)
	PBfsBlock *cached_blk = NULL;

	// mark block as dirty now (so flushes will only occur if block is dirty)
	pblk->set_dirty(true);

	// if (bfsUtilLayer::cache_enabled() &&
	// 	(obj = blk_cache.insertCache(intCacheKey(pbid), pblk))) {
	if (bfsUtilLayer::cache_enabled() &&
		(obj = blk_cache.insertCache(intCacheKey(vbid), 1, pblk))) {
		// sanity check
		if (!(cached_blk = dynamic_cast<PBfsBlock *>(obj))) {
			logMessage(LOG_ERROR_LEVEL, "Failed cast for block ptr\n");
			return BFS_FAILURE;
		}

		if (cached_blk != pblk) {
			flush_blk(cached_blk, vbid);
			delete cached_blk;
		}
	} else {
		// either it was in cache (and overwrote), or it wasnt in cache but
		// there was space to insert; in either case, defer merkle update unless
		// its a synchronous write (which for now they all are; see below)
		if (bfsUtilLayer::cache_enabled())
			cache_hit = true;
		logMessage(BLOCK_VRBLOG_LEVEL, "cache hit on block [vbid=%lu]\n", vbid);
	}

	// if no cache or it's a synchronous write, just do the write to device
	if (!bfsUtilLayer::cache_enabled() || (flags & _bfs__O_SYNC)) {
		// #ifdef __BFS_ENCLAVE_MODE
		// 		double vbc_buf_end_time = 0.0;

		// 		if (bfsUtilLayer::perf_test() && collect_core_lats) {
		// 			if ((ocall_get_time2(&vbc_buf_end_time) != SGX_SUCCESS) ||
		// 				(vbc_buf_end_time == -1))
		// 				return BFS_FAILURE;

		// 			logMessage(BLOCK_VRBLOG_LEVEL,
		// 					   "===== Time in vbc writeblock buf init: %.3f us",
		// 					   vbc_buf_end_time - vbc_start_time);
		// 		}

		// 		double vbc_read_start_time = vbc_buf_end_time;
		// #endif

		if (dev->putBlock(*pblk)) {
			logMessage(
				LOG_ERROR_LEVEL,
				"Failed putting virtual block [%lu] from physical [%lu/%d]",
				vbid, pblk->get_pbid(), dev->getDeviceIdenfier());
			return (-1);
		}

		// #ifdef __BFS_ENCLAVE_MODE
		// 		double vbc_read_end_time = 0.0;

		// 		if (bfsUtilLayer::perf_test() && collect_core_lats) {
		// 			if ((ocall_get_time2(&vbc_read_end_time) != SGX_SUCCESS) ||
		// 				(vbc_read_end_time == -1))
		// 				return BFS_FAILURE;

		// 			logMessage(BLOCK_VRBLOG_LEVEL,
		// 					   "===== Time in vbc writeblock putblock: %.3f us",
		// 					   vbc_read_end_time - vbc_read_start_time);
		// 		}
		// #endif
	}

	// Return sucesfully (the buffer containing the data)
	logMessage(BLOCK_VRBLOG_LEVEL,
			   "Succesfully put virtual block [%lu] from physical [%lu/%d]",
			   vbid, pblk->get_pbid(), dev->getDeviceIdenfier());

	if (pblk && !pblk->unlock()) {
		logMessage(LOG_ERROR_LEVEL, "Failed unlocking block ptr\n");
		return BFS_FAILURE;
	}

	// if (!bfsUtilLayer::cache_enabled())
	// 	delete pblk;

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double vbc_end_time = 0.0;

	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&vbc_end_time) != SGX_SUCCESS) ||
	// 			(vbc_end_time == -1))
	// 			return BFS_FAILURE;

	// 		logMessage(BLOCK_VRBLOG_LEVEL,
	// 				   "===== Time in vbc writeblock total: %.3f us",
	// 				   vbc_end_time - vbc_start_time);
	// 	}
	// #endif

	return cache_hit ? BFS_SUCCESS_CACHE_HIT : BFS_SUCCESS;
}

// /**
//  * @brief Write a block of data to the cluster
//  *
//  * @param blk - the virtual block object to write
//  * @return the block or NULL if failure
//  */
// int bfsVertBlockCluster::writeBlock(VBfsBlock &vblk, op_flags_t flags) {
// 	long ret = -1;

// 	if (vblk.getBuffer() == NULL) {
// 		return (BFS_FAILURE);
// 	}

// 	PBfsBlock pblk(vblk.getBuffer(), vblk.getLength(), 0, 0, 0, 0);
// 	assert(pblk.getLength() == BLK_SZ); // sanity check

// #ifdef __BFS_ENCLAVE_MODE
// 	// give untrusted routine the raw char* to fill in with pblk instead of
// 	// passing the pblock object; much more efficient; expected for the
// 	// buffer to be exactly 4KB
// 	long ocall_ret = 0;
// 	if ((ocall_writeBlock(&ocall_ret, vblk.get_vbid(), (void *)pblk.getBuffer(),
// 						  flags) != SGX_SUCCESS))
// 		return BFS_FAILURE;
// 	ret = ocall_ret;
// #else
// 	// just call helper directly for debug mode
// 	ret = writeBlock_helper(vblk.get_vbid(), pblk.getBuffer(), flags);

// #endif

// 	return ret;
// }

/**
 * @brief Read a set of blocks of data from the cluster
 *
 * @param blks - the set of blocks to read (and places to put data)
 * @return the block or NULL if failure
 */

int bfsVertBlockCluster::readBlocks(bfs_vblock_list_t &blks) {

	// Local variables
	bfs_vblock_list_t::iterator it;
	bfs_block_id_t block;
	bfsDevice *dev;
	map<bfsDevice *, bfs_block_list_t> dev_blocks;
	map<bfsDevice *, bfs_block_list_t>::iterator bit;
	map<bfs_vbid_t, std::pair<bfsDevice *, bfs_block_id_t>> virt_phys_map;

	// Walk the block list and find the devices and blocks they have. Also map
	// the virtual->physical block so that we can trace later when allocating
	// the virtual block objects
	for (it = blks.begin(); it != blks.end(); it++) {
		getPhyBlockAddr(it->first, dev, block);
		dev_blocks[dev][block] = new PBfsBlock(NULL, BLK_SZ, 0, 0, block, dev);
		virt_phys_map[it->first] = std::make_pair(dev, block);
	}

	// Now walk the list of devices to send requests to
	for (bit = dev_blocks.begin(); bit != dev_blocks.end(); bit++) {
		bit->first->getBlocks(bit->second);
	}

	// Now copy the physical blocks into virtual blocks for the fs layer
	for (auto vit = blks.begin(); vit != blks.end(); vit++) {
		if (virt_phys_map.find(vit->first) != virt_phys_map.end())
			blks[vit->first]->setData(
				dev_blocks[virt_phys_map[vit->first].first]
						  [virt_phys_map[vit->first].second]
							  ->getBuffer(),
				BLK_SZ);
	}

	// Log and return successfully
	logMessage(BLOCK_LOG_LEVEL, "Successfully read %d blocks", blks.size());
	return (0);
}

/**
 * @brief Write a set of blocks to the cluster
 *
 * @param blks - the list of blocks to read
 * @return the block or NULL if failure
 */

int bfsVertBlockCluster::writeBlocks(bfs_vblock_list_t &blks) {

	// Local variables
	bfs_vblock_list_t::iterator it;
	bfs_block_id_t block;
	bfsDevice *dev;
	map<bfsDevice *, bfs_block_list_t> dev_blocks;
	map<bfsDevice *, bfs_block_list_t>::iterator bit;

	// Walk the block list and find the devices and blocks they have
	for (it = blks.begin(); it != blks.end(); it++) {
		getPhyBlockAddr(it->first, dev, block);
		dev_blocks[dev][block] =
			new PBfsBlock(it->second->getBuffer(), BLK_SZ, 0, 0, block, dev);
	}

	// Now walk the list of devices to send requests to
	for (bit = dev_blocks.begin(); bit != dev_blocks.end(); bit++) {
		bit->first->putBlocks(bit->second);
	}

	// Log and return successfully
	logMessage(BLOCK_LOG_LEVEL, "Successfully put %d blocks", blks.size());
	return (0);
}

// /**
//  * @brief Release a previously allocated block
//  *
//  * @param blkid - the virtual block ID to deallocate
//  * @return the block or NULL if failure
//  */

// int bfsVertBlockCluster::deallocBlock(bfs_vbid_t id) {

// 	// FOR NOW, NOTHING TO DO
// 	// TODO

// 	// Return successfully
// 	return BFS_SUCCESS;
// }

/**
 * @brief The factory function for the cluster
 *
 * @param none
 * @return pointer to cluster object or NULL on failure
 */

bfsVertBlockCluster *bfsVertBlockCluster::bfsClusterFactory(void) {

	// Create the cluster, initalize it
	bfsVertBlockCluster *cluster = new bfsVertBlockCluster();
	if (cluster->bfsVertBlockClusterInitialize()) {
		logMessage(LOG_ERROR_LEVEL,
				   "Virtual Block Cluster failed to initialize");
		return (NULL);
	}

	// Return the new cluster to the caller
	return (cluster);
}

//
// Private class functions

/**
 * @brief The constructor for the class (private)
 *
 * @param none
 * @return none
 */

bfsVertBlockCluster::bfsVertBlockCluster(void)
	: clusterState(BFSBLK_UNINITIALIZED), maxBlockID(0), blkAllocTable(NULL) {

	// De initialize the cluser object
	bfsVertBlockClusterUninitialize();

	// ensure the cache is sized correctly
	blk_cache.set_max_sz(bfsUtilLayer::getUtilLayerCacheSizeLimit());

	// Return, no return code
	return;
}

/**
 * @brief Initialize the cluster
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsVertBlockCluster::bfsVertBlockClusterInitialize(void) {

	// Local variavbles
	bfs_device_list_t manifest;
	bfs_device_list_t::iterator it;

	// Check the state of the device
	if (clusterState != BFSBLK_UNINITIALIZED) {
		logMessage(LOG_ERROR_LEVEL,
				   "Trying to initialize in bad state, aborting");
		return (-1);
	}

	// Call the layer manifest
	if (bfsDeviceLayer::getDeviceManifest(manifest)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Unable to get device manifest data, aborting");
		return (-1);
	}

	// Walk the device list printing out detail on the device geometry, saving
	// IDs
	for (it = manifest.begin(); it != manifest.end(); it++) {
		addBlockDevice(it->second);
	}

	changeClusterState(BFSBLK_READY);

	// Return successfully
	logMessage(BLOCK_LOG_LEVEL, "Virtual block cluster initialized.");
	return (0);
}

/**
 * @brief De-initialze the cluster
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsVertBlockCluster::bfsVertBlockClusterUninitialize(void) {

	// Cleanup the devices
	free(blkAllocTable);
	blkAllocTable = NULL;
	devices.clear();
	maxBlockID = 0;

	// Change state, return successfully
	changeClusterState(BFSBLK_UNINITIALIZED);
	return (0);
}

/**
 * @brief Change the state of the clusterr
 *
 * @param st - the state to change the cluster into
 * @return int : 0 is success, -1 is failure
 */

int bfsVertBlockCluster::changeClusterState(bfs_vert_cluster_state_t st) {

	// Sanity check the new state
	if ((st < BFSBLK_UNINITIALIZED) || (st > BFSBLK_ERRORED)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Trying change cluster state to nonsense value [%u]", st);
		st = BFSBLK_ERRORED;
	}

	// Just log and change the state of the device
	logMessage(BLOCK_LOG_LEVEL,
			   "Change virtual cluster state from [%s] to [%s]",
			   bfsBlockLayer::getClusterStateStr(clusterState),
			   bfsBlockLayer::getClusterStateStr(st));
	clusterState = st;

	// Return successfully
	return (0);
}

/**
 * @brief Add a block device to the cluster
 *
 * @param dev - the new device to add to the cluster
 * @return int : 0 is success, -1 is failure
 */

int bfsVertBlockCluster::addBlockDevice(bfsDevice *dev) {

	// Local variables
	bfs_vbid_t i, curBlocks;

	// Add a new device to the cluster
	devices.push_back(dev);
	curBlocks = maxBlockID;
	maxBlockID += dev->getNumBlocks();

	// Resize and initlize the block allocation tables
	blkAllocTable = (blk_alloc_entry *)realloc(
		blkAllocTable, maxBlockID * sizeof(blk_alloc_entry));
	// blkAllocTable =
	// 	(blk_alloc_entry *)malloc(maxBlockID * sizeof(blk_alloc_entry));
	for (i = curBlocks; i < maxBlockID; i++) {
		blkAllocTable[i].used = false;
		blkAllocTable[i].device = 0;
		blkAllocTable[i].block = 0;
		blkAllocTable[i].timestamp = 0;
	}

	// Log and return succesfully
	logMessage(LOG_INFO_LEVEL,
			   "Cluster added discovered block device: did=%lu, blocks=%lu",
			   dev->getDeviceIdenfier(), dev->getNumBlocks());
	return (0);
}

/**
 * @brief Get the physical address associated with a virtual block address
 *
 * @param dev - the new device to add to the cluster
 * @return int : 0 is success, -1 is failure
 */

int bfsVertBlockCluster::getPhyBlockAddr(bfs_vbid_t addr, bfsDevice *&dev,
										 bfs_block_id_t &blk) {

	// Local variables
	bfs_vbid_t saddr = 0;
	bfs_device_vec_t::iterator it;
	string message;

	// TODO: brute force, do something more clever later
	for (it = devices.begin(); it != devices.end(); it++) {
		if (saddr + (*it)->getNumBlocks() > addr) {
			dev = (*it);
			blk = addr - saddr;
			return (0);
		}
		saddr += (*it)->getNumBlocks();
	}

	// Throw on failure
	message = "Unmappable virtual block address " + to_string(addr);
	throw new bfsBlockError(message);
}

/**
 * @brief Gets a reference to the block cache object.
 *
 * @return const BfsCache&: the reference to the block cache
 */
const BfsCache &bfsVertBlockCluster::get_blk_cache() { return blk_cache; }

/**
 * @brief Cleanup callback for dirty blocks.
 *
 * @param pblk: the blk to flush
 * @return bool: true if cleanup OK, false otherwise
 */

void bfsVertBlockCluster::flush_blk(PBfsBlock *pblk, bfs_vbid_t vbid) {
	logMessage(BLOCK_VRBLOG_LEVEL, "Flushing block [vbid=%lu, pbid=%lu] ", vbid,
			   pblk->get_pbid());

	if (!pblk->is_dirty())
		return;

	if (static_cast<bfsDevice *>(pblk->get_rd())->putBlock(*pblk)) {
		logMessage(
			LOG_ERROR_LEVEL,
			"Failed flushing physical block [blk=%lu / dev=%d]",
			pblk->get_pbid(),
			static_cast<bfsDevice *>(pblk->get_rd())->getDeviceIdenfier());
		abort();
	}
}
