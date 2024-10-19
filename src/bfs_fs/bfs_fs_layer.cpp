/**
 * @file bfs_fs_layer.cpp
 * @brief Definitions for the shared fs layer interface.
 */

#include "bfs_fs_layer.h"
#include "bfsBlockError.h"
#include "bfs_acl.h"
#include "bfs_core.h"
#include "bfs_core_ext4_helpers.h"
#include <bfsConfigLayer.h>
#include <bfsCryptoError.h>
#include <bfs_log.h>
#include <math.h>

bfsSecAssociation *BfsFsLayer::secContext =
	NULL; /* security context between the fs layer and itself */
unsigned long BfsFsLayer::bfs_core_log_level = (unsigned long)0;
unsigned long BfsFsLayer::bfs_vrb_core_log_level = (unsigned long)0;
bool BfsFsLayer::bfsFsLayerInitialized = false;
bool BfsFsLayer::use_lwext4_impl = false;
merkle_tree_t BfsFsLayer::mt;

/**
 * @brief Initializes the fs layer and dependent layers.
 *
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int BfsFsLayer::bfsFsLayerInit(void) {
	bfsCfgItem *config, *sacfg;
	bool corelog, vrblog;

	if (bfsFsLayerInitialized)
		return BFS_SUCCESS;

	if (BfsACLayer::BfsACLayer_init() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed BfsACLayer_init\n");
		return BFS_FAILURE;
	}

	if (bfsBlockLayer::bfsBlockLayerInit() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed bfsBlockLayerInit\n");
		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	// Check to make sure we were able to load the configuration
	bfsCfgItem *subcfg;
	int64_t ret = 0, ocall_status = 0;
	const long log_flag_max_len = 10;
	char log_enabled_flag[log_flag_max_len] = {0},
		 log_verbose_flag[log_flag_max_len] = {0},
		 use_lwext4_impl_flag[log_flag_max_len] = {0};

	if (((ocall_status = ocall_getConfigItem(
			  &ret, BFS_FS_LAYER_CONFIG, strlen(BFS_FS_LAYER_CONFIG) + 1)) !=
		 SGX_SUCCESS) ||
		(ret == (int64_t)NULL)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getConfigItem\n");
		return BFS_FAILURE;
	}
	config = (bfsCfgItem *)ret;

	if (((ocall_status = ocall_bfsCfgItemType(&ret, (int64_t)config)) !=
		 SGX_SUCCESS) ||
		(ret != bfsCfgItem_STRUCT)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Unable to find crypto configuration in system config "
				   "(ocall_bfsCfgItemType) : %s",
				   BFS_FS_LAYER_CONFIG);
		return BFS_FAILURE;
	}

	if (bfs_core_log_level == (unsigned long)0) {
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
		corelog = (std::string(log_enabled_flag) == "true");
		bfs_core_log_level = registerLogLevel("FS_LOG_LEVEL", corelog);
	}

	if (bfs_vrb_core_log_level == (unsigned long)0) {
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
		bfs_vrb_core_log_level = registerLogLevel("FS_VRB_LOG_LEVEL", vrblog);
	}

	if (use_lwext4_impl == (unsigned long)0) {
		subcfg = NULL;
		if (((ocall_status = ocall_getSubItemByName(
				  (int64_t *)&subcfg, (int64_t)config, "use_lwext4_impl",
				  strlen("use_lwext4_impl") + 1)) != SGX_SUCCESS)) {
			logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
			return (-1);
		}
		if (((ocall_status = ocall_bfsCfgItemValue(
				  &ret, (int64_t)subcfg, use_lwext4_impl_flag,
				  log_flag_max_len)) != SGX_SUCCESS) ||
			(ret != BFS_SUCCESS)) {
			logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
			return (-1);
		}
		use_lwext4_impl = (std::string(use_lwext4_impl_flag) == "true");
	}

	// Now get the security context (keys etc.)
	sacfg = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&sacfg, (int64_t)config, "fs_sa",
			  strlen("fs_sa") + 1)) != SGX_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
		return BFS_FAILURE;
	}
#else
	config = bfsConfigLayer::getConfigItem(BFS_FS_LAYER_CONFIG);

	if (config->bfsCfgItemType() != bfsCfgItem_STRUCT) {
		logMessage(LOG_ERROR_LEVEL,
				   "Unable to find crypto configuration in system config : %s",
				   BFS_FS_LAYER_CONFIG);
		return BFS_FAILURE;
	}

	use_lwext4_impl =
		(config->getSubItemByName("use_lwext4_impl")->bfsCfgItemValue() ==
		 "true");
	corelog =
		(config->getSubItemByName("log_enabled")->bfsCfgItemValue() == "true");
	bfs_core_log_level = registerLogLevel("FS_LOG_LEVEL", corelog);
	vrblog =
		(config->getSubItemByName("log_verbose")->bfsCfgItemValue() == "true");
	bfs_vrb_core_log_level = registerLogLevel("FS_VRB_LOG_LEVEL", vrblog);

	// Now get the security context (keys etc.)
	sacfg = config->getSubItemByName("fs_sa");
#endif

	secContext = new bfsSecAssociation(sacfg, true);

	bfsFsLayerInitialized = true;

	logMessage(FS_LOG_LEVEL, "BfsFsLayer initialized. ");

	return BFS_SUCCESS;
}

/**
 * @brief Check if the fs layer is initialized or not.
 *
 * @return bool: true if initialized, false if not
 */
bool BfsFsLayer::initialized(void) { return bfsFsLayerInitialized; }

/**
 * @brief Get the security context for the fs layer (used to encrypt fs blocks).
 *
 * @return bfsSecAssociation*: the pointer to the context object
 */
bfsSecAssociation *BfsFsLayer::get_SA() { return secContext; }

const merkle_tree_t &BfsFsLayer::get_mt() { return mt; }

/**
 * @brief Get the fs layer standard log level.
 *
 * @return unsigned long: the log level
 */
unsigned long BfsFsLayer::getFsLayerLogLevel(void) {
	return bfs_core_log_level;
}

/**
 * @brief Get the fs layer verbose log level.
 *
 * @return unsigned long: the verbose log level
 */
unsigned long BfsFsLayer::getVerboseFsLayerLogLevel(void) {
	return bfs_vrb_core_log_level;
}

bool BfsFsLayer::use_lwext4(void) { return use_lwext4_impl; }

/**
 * @brief Init merkle tree for the vbc (need key to compute the hashes). The vbc
 * holds integrity context over _virtual blocks_ (OK for direct-mapped
 * translations). This expects that during mkfs, every block in the cluster is
 * hashed. Then on mount, we have all hashes available to construct the merkle
 * tree. Alternatively, may have a sparse tree (?)
 *
 * @param initial flag indicating whether or not this initialization process is
 * reading an initial state of the merkle tree or a non-initial state; if
 * initial, we should compare against a known, fixed hash (for simplicity an
 * empty string here); otherwise we should read the reserved MT block and check
 * that hash with what we compute at runtime with the in-mem tree.
 * @return int BFS_SUCCESS on success, BFS_FAILURE on failure
 */
int BfsFsLayer::init_merkle_tree(bool initial) {
	// init mt struct based on the number of blocks; if no mkfs first, might
	// need to properly init the mt struct here (on mount)
	if (mt.status == 0) {
		if (use_lwext4())
			mt.n = BFS_LWEXT4_NUM_BLKS;
		else
			mt.n = bfsBlockLayer::get_vbc()->getMaxVertBlocNum();
		// Note: requires power of 2 number of blocks
		mt.height = (bfs_vbid_t)log2((double)mt.n);
		mt.num_nodes = (1 << (mt.height + 1)) - 1; // Ex. 8.6GB for 1TB FS
		mt.nodes = (merkle_tree_node_t *)calloc(mt.num_nodes,
												sizeof(merkle_tree_node_t));
		mt.status = 1;
	}

	// secContext

	// start hashing nodes at level=height-1
	// for (bfs_vbid_t v = 0; v < NUM_BLOCKS; v++) { // pull from cfg
	// (maxBlockID is correct)

	// // compute hashes for the leaves
	// for (bfs_block_id_t l = (1 << mt.height) - 1; l < mt.n; l++) {
	// 	mt.nodes[l].hash = NULL;
	// 	if (hash_node(l) != BFS_SUCCESS)
	// 		return BFS_FAILURE;
	// }

	// // iterate backwards and compute hashes for each internal node up to root
	// for (bfs_block_id_t l = (1 << mt.height) - 1 - 1; l > 0; l--) {
	// 	mt.nodes[l].hash = NULL;
	// 	if (hash_node(l) != BFS_SUCCESS)
	// 		return BFS_FAILURE;
	// }

	// iterate backwards and compute hashes for leaves then for each internal
	// node up to root (using the memoized child hashes); use cached in-mem
	// hashes if mkfs (ie flush) was called prior
	int hash_sz = 0;
	for (bfs_vbid_t i = mt.num_nodes - 1;; i--) {
		// if node hash isn't already in-mem, just compute the hash
		if (i >= (bfs_vbid_t)((1 << mt.height) - 1)) {
			// is a leaf node (ie a block, with 16B hash)
			hash_sz = secContext->getKey()->getMACsize();
		} else {
			// is an internal node (with 32B hash)
			hash_sz = secContext->getKey()->getHMACsize();
		}

		if (!mt.nodes[i].hash) {
			mt.nodes[i].hash = (uint8_t *)calloc(hash_sz, sizeof(uint8_t));
			if (!initial && (hash_node(i, mt.nodes[i].hash) != BFS_SUCCESS))
				return BFS_FAILURE;
		}

		if (i == 0)
			break;
	}

	if (initial) {
		logMessage(LOG_ERROR_LEVEL, "Initial init");
		return BFS_SUCCESS;
	}

	// Lastly, compare the computed root hash with that stored in the root mt
	// block. TODO: or retrieve from a TTP
	if (!get_SA()) {
		logMessage(LOG_ERROR_LEVEL, "No SA in save_root_hash\n");
		return BFS_FAILURE;
	}

	// only the mt root is stored in this block
	uint8_t *hmac_copy =
		(uint8_t *)calloc(secContext->getKey()->getHMACsize(), sizeof(uint8_t));
	if (read_blk_meta(BFS_LWEXT_MT_ROOT_BLK_NUM, NULL, &hmac_copy, true) !=
		BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed reading security metadata");
		return BFS_FAILURE;
	}

	// VBfsBlock blk(NULL, BLK_SZ, 0, 0, MT_REL_START_BLK_NUM);
	// if (read_block_helper(blk) != BFS_SUCCESS) {
	// 	logMessage(LOG_ERROR_LEVEL,
	// 			   "Failed read_block_helper in init_merkle_tree(block %lu)\n",
	// 			   MT_REL_START_BLK_NUM);
	// 	return BFS_FAILURE;
	// }

	if (memcmp(mt.nodes[0].hash, hmac_copy,
			   secContext->getKey()->getHMACsize()) != 0) {
		logMessage(LOG_ERROR_LEVEL, "Invalid root hash");
		return BFS_FAILURE;
	}

	free(hmac_copy);

	return BFS_SUCCESS;
}

/**
 * @brief Flush in-mem mt contents to disk (ie compute+save root hash).
 *
 * @return int BFS_SUCCESS on success, BFS_FAILURE on failure
 */
int BfsFsLayer::flush_merkle_tree(void) {
	// if trying to mkfs first, need to properly init the mt struct
	if (mt.status == 0) {
		if (use_lwext4())
			mt.n = BFS_LWEXT4_NUM_BLKS;
		else
			mt.n = bfsBlockLayer::get_vbc()->getMaxVertBlocNum();
		// Note: requires power of 2 number of blocks
		mt.height = (bfs_vbid_t)log2((double)mt.n);
		mt.num_nodes = (1 << (mt.height + 1)) - 1; // Ex. 8.6GB for 1TB FS
		mt.nodes = (merkle_tree_node_t *)calloc(mt.num_nodes,
												sizeof(merkle_tree_node_t));
		mt.status = 1;
	}

	// iterate backwards and compute hashes for leaves then for each internal
	// node up to root (using the memoized child hashes)
	int hash_sz = 0; // size of _current_ hash
	for (bfs_vbid_t i = mt.num_nodes - 1;; i--) {
		// if node hash isn't already in-mem, just compute the hash
		if (i >= (bfs_vbid_t)((1 << mt.height) - 1)) {
			// is a leaf node (ie a block, with 16B hash)
			hash_sz = secContext->getKey()->getMACsize();
		} else {
			// is an internal node (with 32B hash)
			hash_sz = secContext->getKey()->getHMACsize();
		}

		if (!mt.nodes[i].hash) {
			mt.nodes[i].hash = (uint8_t *)calloc(hash_sz, sizeof(uint8_t));
			if (hash_node(i, mt.nodes[i].hash) != BFS_SUCCESS)
				return BFS_FAILURE;
		}

		if (i == 0)
			break;
	}

	// root hash is at index 0
	if (save_root_hash() != BFS_SUCCESS)
		return BFS_FAILURE;

	return BFS_SUCCESS;
}

/**
 * @brief Compute hash of the specified virtual block and internal mt nodes.
 * Derived from BfsHandle::read_blk; extracts the MAC from the decrypt call.
 * Note that the block hashes are GMACs while the internal nodes are HMACs of
 * the latter
 *
 * @param i
 * @return int
 */
int BfsFsLayer::hash_node(bfs_vbid_t i, uint8_t *out) {
	// now add the MAC/hash to the merkle tree
	if (i >= (bfs_vbid_t)((1 << mt.height) - 1)) {
		// if leaf (read block and get MAC tag)
		bfs_vbid_t baddr =
			i - ((1 << mt.height) - 1); // subtract block-node start addr
		return read_blk_meta(baddr, NULL, &out);
	}

	// } else if (i >= ((1 << mt.height) - 1)) {
	// otherwise if internal node (compute the rest of MACs from mt
	// structure)
	// long blk_idx = ((1 << mt.height) - 1) + i; // index in merkle tree
	// array
	uint8_t *left_child = mt.nodes[2 * i + 1].hash;
	uint8_t *right_child = mt.nodes[2 * i + 2].hash;
	// bfsSecureFlexibleBuffer cat(
	// 	NULL, secContext->getKey()->getHMACsize() * 2, 0, 0);
	// memcpy(cat.getBuffer(), left_child,
	// secContext->getKey()->getHMACsize()); memcpy(cat.getBuffer() +
	// secContext->getKey()->getHMACsize(), left_child,
	// 	   secContext->getKey()->getHMACsize());
	// bfsSecureFlexibleBuffer *aad2 = new bfsSecureFlexibleBuffer();

	int hash_sz = 0; // size of _child_ hash(es)
	if (i >= (bfs_vbid_t)((1 << (mt.height - 1)) - 1)) {
		// child nodes are leaves (ie blocks, with 16B hashes)
		hash_sz = secContext->getKey()->getMACsize();
	} else {
		// child nodes are internal nodes (with 32B hashes)
		hash_sz = secContext->getKey()->getHMACsize();
	}

	// uint8_t *cat = (uint8_t *)calloc(hash_sz * 2, sizeof(uint8_t));
	// memcpy(cat, left_child, hash_sz);
	// memcpy(cat + hash_sz, right_child, hash_sz);

	return secContext->hmacData(out, left_child, right_child, hash_sz);
}

// int BfsFsLayer::verify_block_freshness(bfs_vbid_t p) {
// 	// TODO
// 	if (p == 0)
// 		return BFS_SUCCESS;

// 	if (p > NUM_BLOCKS)
// 		return BFS_FAILURE;

// 	// loop through parent and neighbor
// 	int right_child_flag = p % 2;
// 	for (int i = p; p > 0; p = (p - 1) / 2) {
// 		// block is a right child
// 		if (p % 2 == 0)
// 			;
// 	}
// }

/**
 * @brief Save root hash of merkle tree to the reserved MT block. Note that we
 * need monotonic counter or other method to prevent replay of the root hash (ie
 * the entire MT itself).
 *
 * @return int
 */
int BfsFsLayer::save_root_hash() {
	if (!get_SA()) {
		logMessage(LOG_ERROR_LEVEL, "No SA in save_root_hash\n");
		return BFS_FAILURE;
	}

	// uint8_t *hmac_copy =
	// 	(uint8_t *)calloc(secContext->getKey()->getHMACsize(), sizeof(uint8_t));
	if (write_blk_meta(BFS_LWEXT_MT_ROOT_BLK_NUM, NULL, &(mt.nodes[0].hash),
					   true) != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed writing security metadata");
		return BFS_FAILURE;
	}

	// // encrypt and add MAC tag
	// // The buffer should contain the IV (12 bytes) + data (4064) + MAC (16
	// // bytes) and should fit within the device block size.
	// bfs_vbid_t vbid = MT_REL_START_BLK_NUM;
	// VBfsBlock blk(NULL, BLK_SZ, 0, 0, vbid);
	// try {
	// 	// Just copy the HMAC in, dont need to encrypt the block.
	// 	// Need to make sure write_blk does not interere with this in any way,
	// 	// should be OK to allow the meta blocks to track the MT root block.
	// 	memcpy(blk.getBuffer(), (const uint8_t *)mt.nodes[0].hash,
	// 		   secContext->getKey()->getHMACsize());

	// 	// bfsBlockLayer::get_vbc()->inc_block_timestamp(
	// 	// 	vbid); // on writes, update virtual timestamp (ie block version)
	// 	// 		   // relative to this block, then do write
	// 	// uint64_t blk_ts =
	// 	// bfsBlockLayer::get_vbc()->get_block_timestamp(vbid);
	// 	// bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer();
	// 	// aad->resizeAllocation(0, sizeof(vbid), 0);
	// 	// *aad << vbid; // << blk_ts;
	// 	// get_SA()->encryptData(blk, aad, true);
	// 	// delete aad;
	// } catch (bfsCryptoError *err) {
	// 	logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
	// 			   err->getMessage().c_str());
	// 	delete err;
	// 	return BFS_FAILURE;
	// }

	// // validate that correct amount of data written
	// // assert(blk.getLength() == (BLK_SZ - 4));

	// // Pad the encrypted block to the physical block size (should always
	// submit
	// // appropriate sized buffers to the block layer), then write the
	// encrypted
	// // block to the bdev.
	// // int32_t z = 0;
	// // blk.addTrailer((char *)&z, sizeof(z));

	// try {
	// 	if (use_lwext4()) {
	// 		// write directly to disk, bypassing the mt (since this is the root)
	// 		int r = file_dev_bwrite(NULL, blk.getBuffer(), blk.get_vbid(), 1);
	// 		if (r != BFS_SUCCESS) {
	// 			logMessage(LOG_ERROR_LEVEL, "Failed to write to file device\n");
	// 			return r;
	// 		}
	// 		// if ((ret = __do_file_dev_bwrite(&blk)) == BFS_FAILURE) {
	// 		// 	logMessage(LOG_ERROR_LEVEL,
	// 		// 			   "Failed __do_file_dev_bwrite in save_root_hash\n");
	// 		// 	return BFS_FAILURE;
	// 		// }
	// 	} else {
	// 		if (bfsBlockLayer::writeBlock(blk, _bfs__O_SYNC) == BFS_FAILURE) {
	// 			logMessage(LOG_ERROR_LEVEL, "Failed writing block");
	// 			return BFS_FAILURE;
	// 		}
	// 	}
	// } catch (bfsBlockError *err) {
	// 	logMessage(LOG_ERROR_LEVEL, "%s", err->getMessage().c_str());
	// 	delete err;
	// 	return BFS_FAILURE;
	// }

	// logMessage(FS_VRB_LOG_LEVEL, "write_blk [%lu] success\n",
	// blk.get_vbid());

	return BFS_SUCCESS;
}

/**
 * @brief Extracts the security metadata (IV+MAC) from the reserved disk area
 * (only for individual blocks -- the mt MACs themselves are computed in memory
 * for now).
 */
int BfsFsLayer::read_blk_meta(bfs_vbid_t b, uint8_t **iv, uint8_t **mac_copy,
							  bool root) {
	if ((root && !*mac_copy) || (!root && !(*iv || *mac_copy))) {
		logMessage(LOG_ERROR_LEVEL, "read_blk_meta: bad iv or mac_copy");
		return BFS_FAILURE;
	}

	// First compute MAC location.
	// These are generally always in plaintext and not redundantly
	// hashed/checksummed.
	// Want to reuse the macro for bfs/lwext4 so dont embed the
	// METADATA_REL_START_BLK_NUM into it explicitly and just compute it here.
	bfs_vbid_t meta_blk = 0;
	if (use_lwext4()) {
		meta_blk = BFS_LWEXT_META_START_BLK_NUM + BLK_META_BLK_LOC(b);
		assert(BLK_META_BLK_LOC(b) <= BFS_LWEXT4_META_SPC); // make sure it fits
	} else
		meta_blk = METADATA_REL_START_BLK_NUM + BLK_META_BLK_LOC(b);
	int meta_blk_idx_loc =
		BLK_META_BLK_IDX_LOC(b) *
		(secContext->getKey()->getIVlen() + secContext->getKey()->getMACsize());

	// fetch the associated MAC block
	VBfsBlock blk(NULL, BLK_SZ, 0, 0, meta_blk);
	if (read_block_helper(blk) != BFS_SUCCESS) {
		logMessage(
			LOG_ERROR_LEVEL,
			"Failed read_block_helper in extract_block_mac (block %lu)\n",
			meta_blk);
		return BFS_FAILURE;
	}

	// fetch the MAC and IV from the meta block
	if (!root) {
		if (*iv)
			memcpy(*iv, blk.getBuffer() + meta_blk_idx_loc,
				   secContext->getKey()->getIVlen());
		if (*mac_copy)
			memcpy(*mac_copy,
				   blk.getBuffer() + meta_blk_idx_loc +
					   secContext->getKey()->getIVlen(),
				   secContext->getKey()->getMACsize());
	} else {
		memcpy(*mac_copy, blk.getBuffer(), secContext->getKey()->getHMACsize());
	}

	return BFS_SUCCESS;
}

int BfsFsLayer::write_blk_meta(bfs_vbid_t b, uint8_t **iv, uint8_t **mac_copy,
							   bool root) {
	/* If saving root hash, mac_copy should be given, and otherwise at least one
	 * of the others should. This should catch most bugs. */
	if ((root && !*mac_copy) || (!root && !(*iv || *mac_copy))) {
		logMessage(LOG_ERROR_LEVEL, "write_blk_meta: bad iv or mac_copy");
		return BFS_FAILURE;
	}

	// First compute MAC location.
	// As a temp workaround for lwext4 (instead of changing the block allocation
	// code), just use the extra allocated space at the end of bfs_blk_dev for
	// the meta blocks. The FS layer will know about it, but the lwext4 code
	// won't since we init the ext4 block device only with BFS_LWEXT4_NUM_BLKS.
	bfs_vbid_t meta_blk = 0;
	if (use_lwext4()) {
		meta_blk = BFS_LWEXT_META_START_BLK_NUM + BLK_META_BLK_LOC(b);
		auto x = BLK_META_BLK_LOC(b);
		auto y = BFS_LWEXT4_META_SPC;
		assert(x < y); // make sure it fits
	} else
		meta_blk = METADATA_REL_START_BLK_NUM + BLK_META_BLK_LOC(b);
	int meta_blk_idx_loc =
		BLK_META_BLK_IDX_LOC(b) *
		(secContext->getKey()->getIVlen() + secContext->getKey()->getMACsize());

	VBfsBlock blk(NULL, BLK_SZ, 0, 0, meta_blk);

	// copy the MAC and IV over to init the associated meta block
	if (!root) {
		// read current contents (if not root block)
		if (read_block_helper(blk) != BFS_SUCCESS) {
			logMessage(
				LOG_ERROR_LEVEL,
				"Failed read_block_helper in extract_block_mac (block %lu)\n",
				meta_blk);
			return BFS_FAILURE;
		}

		if (*iv)
			memcpy(blk.getBuffer() + meta_blk_idx_loc, *iv,
				   secContext->getKey()->getIVlen());
		if (*mac_copy)
			memcpy(blk.getBuffer() + meta_blk_idx_loc +
					   secContext->getKey()->getIVlen(),
				   *mac_copy, secContext->getKey()->getMACsize());
	} else {
		memcpy(blk.getBuffer(), *mac_copy, secContext->getKey()->getHMACsize());
	}

	// write the meta block back to disk
	if (write_block_helper(blk) != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed write_block_helper in write_blk_meta (block %lu)\n",
				   meta_blk);
		return BFS_FAILURE;
	}

	return BFS_SUCCESS;
}

int BfsFsLayer::write_block_helper(VBfsBlock &blk) {
	int ret = BFS_FAILURE;
	try {
		if (use_lwext4()) {
			if ((ret = __do_put_block(blk.get_vbid(), blk.getBuffer())) ==
				BFS_FAILURE) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failed __do_put_block in write_block_helper\n");
				return BFS_FAILURE;
			}
		} else {
			if ((ret = bfsBlockLayer::writeBlock(blk, _bfs__O_SYNC)) ==
				BFS_FAILURE) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failed writeBlock in write_block_helper\n");
				return BFS_FAILURE;
			}
		}
	} catch (bfsBlockError *err) {
		logMessage(LOG_ERROR_LEVEL, "%s", err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	}

	return BFS_SUCCESS;
}

/**
 * @brief Read a block from disk (similar to bfs_core read_blk). Note that when
 * flush_mt/init calls this (eg from initial mkfs state), it is assumed that the
 * initial block states dont matter (besides the metadata related mkfs blocks,
 * which will be cached as long as the cache is large enough). We have to deal
 * with this separately otherwise we run into a circular dependency (ie
 * flush/init calls hash_node, which calls read_block, which calls hash_node,
 * etc.).
 *
 * @param blk
 * @param mac_copy
 * @return int
 */
int BfsFsLayer::read_block_helper(VBfsBlock &blk) {
	int ret = BFS_FAILURE;
	try {
		if (use_lwext4()) {
			if ((ret = __do_get_block(blk.get_vbid(), blk.getBuffer())) ==
				BFS_FAILURE) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failed __do_get_block in read_block_helper\n");
				return BFS_FAILURE;
			}
		} else {
			if ((ret = bfsBlockLayer::readBlock(blk)) == BFS_FAILURE) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failed readBlock in read_block_helper\n");
				return BFS_FAILURE;
			}
		}
	} catch (bfsBlockError *err) {
		logMessage(LOG_ERROR_LEVEL, "%s", err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	}

	// // pop the padding that fills the physical block
	// int32_t z = -1;
	// blk.removeTrailer((char *)&z, sizeof(z));

	// if (!get_SA()) {
	// 	logMessage(LOG_ERROR_LEVEL, "No SA in read_block_helper\n");
	// 	return BFS_FAILURE;
	// }

	// // decrypt and verify MAC
	// bfs_vbid_t vbid = blk.get_vbid();
	// try {
	// 	// uint64_t blk_ts =
	// 	// bfsBlockLayer::get_vbc()->get_block_timestamp(vbid);
	// 	bfsSecureFlexibleBuffer *aad1 = new bfsSecureFlexibleBuffer();
	// 	aad1->resizeAllocation(0, sizeof(vbid), 0);
	// 	*aad1 << vbid;
	// 	// 	  << blk_ts; // validate using current timestamp, but dont update
	// 	get_SA()->decryptData(blk, aad1, false, NULL);
	// 	delete aad1;
	// } catch (bfsCryptoError *err) {
	// 	logMessage(LOG_ERROR_LEVEL,
	// 			   "Exception caught from decrypt in read_block_helper: %s\n",
	// 			   err->getMessage().c_str());
	// 	delete err;
	// 	return BFS_FAILURE;
	// }

	// // validate that correct amount of data read, then copy over
	// // assert(blk.getLength() == EFF_BLK_SZ);
	// assert(blk.getLength() == BLK_SZ);

	// // if cached (in secure memory), skip MT verification
	// // if (bfsUtilLayer::use_mt() && (ret != BFS_SUCCESS_CACHE_HIT)) {
	// // 	// iterate backwards and compute hashes for appropriate nodes up to
	// // 	// root; compare the result with mt.nodes[0]
	// // 	uint8_t *ih = NULL, *curr_root = NULL;

	// // 	if (!mt.nodes[0].hash) {
	// // 		logMessage(LOG_ERROR_LEVEL, "NULL root hash in read_blk", NULL,
	// // 				   NULL);
	// // 		return BFS_FAILURE;
	// // 	}

	// // 	curr_root = mt.nodes[0].hash;

	// // 	// for (bfs_vbid_t i = mt.num_nodes - 1;; i--) {
	// // 	bfs_vbid_t i = vbid + ((1 << mt.height) - 1);

	// // 	if (!mt.nodes[i].hash) {
	// // 		logMessage(LOG_ERROR_LEVEL,
	// // 				   "Hash doesnt exist but should in read_blk", NULL,
	// NULL);
	// // 		return BFS_FAILURE;
	// // 	}

	// // 	ih = mt.nodes[i].hash;
	// // 	mt.nodes[i].hash = mac_copy;

	// // 	// Go to parent (based on if i is a left- or right-child);
	// // 	// ensures log2 overhead for mt verification
	// // 	if (i % 2 == 0)
	// // 		i = (i - 2) / 2;
	// // 	else
	// // 		i = (i - 1) / 2;

	// // 	while (1) {
	// // 		if (i == 0)
	// // 			break;

	// // 		// these may be dependent on the new vbid so just always
	// // 		// recompute them (TODO: optimize later)
	// // 		if (hash_node(i, mt.nodes[i].hash) != BFS_SUCCESS) {
	// // 			logMessage(LOG_ERROR_LEVEL, "Failed hash_node in read_blk",
	// // 					   NULL, NULL);
	// // 			return BFS_FAILURE;
	// // 		}

	// // 		// Go to parent (based on if i is a left- or right-child);
	// // 		// ensures log2 overhead for mt verification
	// // 		if (i % 2 == 0)
	// // 			i = (i - 2) / 2;
	// // 		else
	// // 			i = (i - 1) / 2;
	// // 	}

	// // 	// check the new computed root against the current root
	// // 	if (memcmp(mt.nodes[0].hash, curr_root,
	// // 			   get_SA()->getKey()->getHMACsize()) != 0) {
	// // 		mt.nodes[0].hash = curr_root;
	// // 		mt.nodes[(vbid + (1 << mt.height) - 1)].hash = ih;
	// // 		logMessage(LOG_ERROR_LEVEL,
	// // 				   "Invalid root hash comparison in read_blk", NULL,
	// NULL);
	// // 		return BFS_FAILURE;
	// // 	}
	// // }

	return BFS_SUCCESS;
}
