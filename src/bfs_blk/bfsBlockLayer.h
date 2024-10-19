#ifndef BFS_BLOCK_LAYER_INCLUDED
#define BFS_BLOCK_LAYER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
// 1
//  File          : bfsBlockLayer.h
//  Description   : This is the class implementing the block layer interface
//                  for the bfs file system.  Note that this is static class
//                  in which there are no objects.
//
//  Author  : Patrick McDaniel
//  Created : Mon 29 Mar 2021 02:43:16 PM EDT
//

// Project Includes
#ifdef __BFS_ENCLAVE_MODE
#include "bfs_enclave_t.h" /* For ocalls */
#elif defined(__BFS_DEBUG_NO_ENCLAVE)
#include "bfs_block_ocalls.h"
#endif
#include <bfsVertBlockCluster.h>
#include <bfs_block.h>
#include <cassert>
#include <chrono>
#include <unordered_map>

//
// Class definitions
#define BLOCK_LOG_LEVEL bfsBlockLayer::getBlockLayerLogLevel()
#define BLOCK_VRBLOG_LEVEL bfsBlockLayer::getVerboseBlockLayerLogLevel()
#define BFS_BLKLYR_CONFIG "bfsBlockLayer"
#define BFS_BLKLYR_ALLOC_DSP "allocation_discipline"
#define BFS_DEV_UNIT_TEST_SLOTS 256
#define BFS_DEV_UNIT_TEST_ITERATIONS 1024
#define BFS_UTEST_UNUSED (bfs_vbid_t) - 1

//
// Class types

// Block allocation strategy
typedef enum {
	BFSBLK_LINEAR_ALLOC = 0,	 // Linear block allocation
	BFSBLK_INTERLEAVE_ALLOC = 1, // Disk interleaving block allocation
	BFSBLK_MAX_ALLOC = 2,		 // Guard value
} bfs_vert_cluster_alloc_t;

//
// Class Definition

class bfsBlockLayer {

public:
	//
	// Static methods

	static int
	loadBlockLayerConfiguration(char *filename); // NOT USED YET
												 // Load a block configuration

	static int bfsBlockLayerInit(void);
	// Initialize the block layer state

#ifdef __BFS_DEBUG_NO_ENCLAVE
	static int bfsBlockLayerUtest(void);
	// Perform a unit test on the block layer implementation
#endif

	//
	// Static Class Variables

	// Layer log level
	static unsigned long getBlockLayerLogLevel(void) {
		return (bfsBlockLogLevel);
	}

	// Verbose log level
	static unsigned long getVerboseBlockLayerLogLevel(void) {
		return (bfsVerboseBlockLogLevel);
	}

	// Allocation algorithm
	static bfs_vert_cluster_alloc_t getAllocationAlgorithm(void) {
		return (bfsBlockAllocAlgorithm);
	}

	// Return a descriptive string for the state
	static const char *getClusterStateStr(bfs_vert_cluster_state_t st) {
		if ((st < 0) || (st >= BFSBLK_MAXSTATE)) {
			return ("<*BAD STATE*>");
		}
		return (bfs_cluster_state_strings[st]);
	}

	// #ifdef __BFS_ENCLAVE_MODE
	/**
	 * @brief Gets the pointer to the virtual block cluster object.
	 *
	 * @return bfsVertBlockCluster*: the pointer to the cluster
	 */
	static bfsVertBlockCluster *get_vbc() { return vbc; }

	/**
	 * @brief Set the virtual block cluster pointer of the fs layer
	 *
	 * @param v: pointer to set as
	 * @return int: 0 if successful, -1 if failure
	 */
	static int set_vbc(bfsVertBlockCluster *v) {
		if (!v)
			return BFS_FAILURE;

		vbc = v;

		set_num_blocks(vbc->getMaxVertBlocNum());

		return BFS_SUCCESS;
	}
	// #endif

	static void set_num_blocks(bfs_vbid_t v) { num_blocks = v; }

	static bfs_vbid_t get_num_blocks() { return num_blocks; }

	static bool initialized(void) { return bfsBlockLayerInitialized; }

	// Wrappers for the fs code to jump to the methods on the cluster through
	// the non-TEE BlockLayer
	static int readBlock(VBfsBlock &vblk) {
		int ret = -1;

		// #ifdef __BFS_ENCLAVE_MODE
		// 		double bl_read_start_time = 0.0;
		// 		if (bfsUtilLayer::perf_test()) {
		// 			if ((ocall_get_time2(&bl_read_start_time) != SGX_SUCCESS) ||
		// 				(bl_read_start_time == -1))
		// 				return BFS_FAILURE;
		// 			// bl_read_start_time =
		// 			// 	std::chrono::time_point_cast<std::chrono::microseconds>(
		// 			// 		std::chrono::high_resolution_clock::now())
		// 			// 		.time_since_epoch()
		// 			// 		.count();
		// 		}
		// #endif

		if (vblk.getBuffer() == NULL) {
			return (BFS_FAILURE);
		}

		// PBfsBlock *pblk = new PBfsBlock(NULL, BLK_SZ, 0, 0, 0, 0);
		PBfsBlock *pblk = NULL;

		// #ifdef __BFS_ENCLAVE_MODE
		// 		// give untrusted routine the raw char* to fill in with pblk
		// instead
		// 		// of passing the pblock object; much more efficient; expected
		// for
		// 		// the buffer to be exactly 4KB
		// 		int ocall_ret = 0;
		// 		if ((ocall_readBlock(&ocall_ret, vblk.get_vbid(),
		// 							 (void *)pblk->getBuffer()) != SGX_SUCCESS))
		// 			return BFS_FAILURE;
		// 		ret = ocall_ret;
		// #else
		// 		// just call helper directly for debug mode
		// 		ret = vbc->readBlock_helper(vblk.get_vbid(), pblk->getBuffer());
		// #endif

		// Note: depending on whether block layer is inside/outside of TEE, we
		// may want to pass either the PBfsBlock object (if inside) or just the
		// raw pointer to its buffer (if outside); for now, just use the object.
		// ret = vbc->readBlock_helper(vblk.get_vbid(), pblk->getBuffer());
		ret = vbc->readBlock_helper(vblk.get_vbid(), &pblk);

		if ((ret == BFS_SUCCESS) || (ret == BFS_SUCCESS_CACHE_HIT))
			vblk.setData(pblk->getBuffer(), pblk->getLength());

		// #ifdef __BFS_ENCLAVE_MODE
		// 		double bl_read_end_time = 0.0;

		// 		if (bfsUtilLayer::perf_test()) {
		// 			if ((ocall_get_time2(&bl_read_end_time) != SGX_SUCCESS) ||
		// 				(bl_read_end_time == -1))
		// 				return BFS_FAILURE;
		// 			// bl_read_end_time =
		// 			// 	std::chrono::time_point_cast<std::chrono::microseconds>(
		// 			// 		std::chrono::high_resolution_clock::now())
		// 			// 		.time_since_epoch()
		// 			// 		.count();
		// 			logMessage(BLOCK_VRBLOG_LEVEL,
		// 					   "===== Time in blockLayer readblock total: %.3f
		// us", 					   bl_read_end_time - bl_read_start_time);
		// 		}
		// #endif

		// delete pblock if we dont have a cache
		if (!bfsUtilLayer::cache_enabled())
			delete pblk;

		return ret;
	}
	// Read a block of data from the cluster

	static int writeBlock(VBfsBlock &vblk, op_flags_t flags = _bfs__NONE) {
		int ret = -1;

		if (vblk.getBuffer() == NULL) {
			return (BFS_FAILURE);
		}

		PBfsBlock *pblk =
			new PBfsBlock(vblk.getBuffer(), vblk.getLength(), 0, 0, 0, 0);
		assert(pblk->getLength() == BLK_SZ); // sanity check

		// #ifdef __BFS_ENCLAVE_MODE
		// 		// give untrusted routine the raw char* to fill in with pblk
		// instead
		// 		// of passing the pblock object; much more efficient; expected
		// for
		// 		// the buffer to be exactly 4KB
		// 		int ocall_ret = 0;
		// 		if ((ocall_writeBlock(&ocall_ret, vblk.get_vbid(),
		// 							  (void *)pblk.getBuffer(), flags) !=
		// SGX_SUCCESS)) 			return BFS_FAILURE; 		ret = ocall_ret;
		// #else
		// 		// just call helper directly for debug mode
		// 		ret = vbc->writeBlock_helper(vblk.get_vbid(), pblk.getBuffer(),
		// flags); #endif
		ret = vbc->writeBlock_helper(vblk.get_vbid(), pblk, flags);

		// delete pblock if we dont have a cache(*pblk)
		if (!bfsUtilLayer::cache_enabled())
			delete pblk;

		return ret;
	}
	// Write a block to the cluster at a virtual block address

	/**
	 * @brief Release a previously allocated block
	 *
	 * @param blkid - the virtual block ID to deallocate
	 * @return the block or NULL if failure
	 */
	static int deallocBlock(bfs_vbid_t id) {

		(void)id;
		// FOR NOW, NOTHING TO DO
		// TODO: call into vbc::deallocBlock

		// Return successfully
		return BFS_SUCCESS;
	}

private:
	//
	// Private class methods

	bfsBlockLayer(void) {}
	// Default constructor (prevents creation of any instance)

	//
	// Static Class Variables

	static bool bfsBlockLayerInitialized;
	// Flag indicating whether the block layer has been initialized

	static bfs_vert_cluster_alloc_t bfsBlockAllocAlgorithm;
	// The allocation algorithm used for assigning blocks

	static unsigned long bfsBlockLogLevel;
	// The log level for all of the block information

	static unsigned long bfsVerboseBlockLogLevel;
	// The verbose log level for all of the block information

	//
	// Stat constant data

	static const char *bfs_cluster_state_strings[BFSBLK_MAXSTATE];
	// Strings identifying the state of the device

	static const char *bfs_vert_cluster_alloc_strings[BFSBLK_MAX_ALLOC];
	// Strings identifying the different allocation strategies

	// #ifdef __BFS_ENCLAVE_MODE
	/* Pointer to a connected block cluster. Only exposes the cluster to
	 * non-TEE code; therefore all the vertBlockCluster methods run in
	 * non-TEE mode */
	static bfsVertBlockCluster *vbc;
	// #endif

	static bfs_vbid_t num_blocks;
};

#endif
