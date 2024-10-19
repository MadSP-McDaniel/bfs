/**
 * @file bfs_core.cpp
 * @brief The definitions for core Bfs file operations methods and helpers. This
 * includes definitions for BfsHandle, SuperBlock, IBitMap, Inode, DirEntry,
 * IndirectBlock, OpenFile, and error class types.
 */

#include <cassert>
#include <cstring>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <utility>

#include "bfs_acl.h"
#include "bfs_core.h"
#include "bfs_fs_layer.h"
#include <bfsBlockError.h>
#include <bfsBlockLayer.h>
#include <bfsConfigLayer.h>
#include <bfsCryptoError.h>
#include <bfsVertBlockCluster.h>
#include <bfs_common.h>
#include <bfs_log.h>
#include <bfs_util.h>

#ifdef __BFS_ENCLAVE_MODE
#include "bfs_enclave_t.h" /* For ocalls */
#elif defined(__BFS_DEBUG_NO_ENCLAVE)
#include <bfs_util_ocalls.h>
#endif

/* For performance testing */
// static void write_core_latencies();
// static void write_lats_to_file(std::string, std::string);

static uint8_t *curr_par = NULL;

/**
 * BfsHandle definitions
 */

/**
 * @brief Reads and decrypts a block from the virtual block cluster. Ensures
 * that the decrypted buffer data is in protected memory (enclave memory). This
 * relies on the block being sized appropriately in the calling function to
 * avoid having to continuously resize a reused buffer (might want to consider
 * adding some additional space when we do remote devices to make room for the
 * device layer headers for remote devices).
 *
 * @param vbid: the virtual block id to read from
 * @param blk: buffer containing the block data to read into
 * @return Throws BfsServerError if failure
 */
void BfsHandle::read_blk(VBfsBlock &blk) {
	// Dont ever try to read directly; and MKFS never reads directly
	if ((blk.get_vbid() >= METADATA_REL_START_BLK_NUM) &&
		(blk.get_vbid() < DATA_REL_START_BLK_NUM))
		throw BfsServerError("Trying to read meta block directly", NULL, NULL);

	// bfsSecureFlexibleBuffer buf;
	int ret = BFS_FAILURE;

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double core_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&core_start_time) != SGX_SUCCESS) ||
	// 			(core_start_time == -1))
	// 			return;
	// 	}
	// #else
	// 	double core_start_time = 0.0;

	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((core_start_time = ocall_get_time2()) == -1)
	// 			return;
	// 	}
	// #endif

	// VBfsBlock enc_blk(NULL, BLK_SZ - 4, blk.get_vbid());
	// blk.resizeAllocation(BLK_SZ, 0x0, 0, BLK_SZ - BLK_SZ);
	blk.set_vbid(blk.get_vbid());

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double core_buf_end_time = 0.0;

	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&core_buf_end_time) != SGX_SUCCESS) ||
	// 			(core_buf_end_time == -1))
	// 			return;

	// 		logMessage(FS_VRB_LOG_LEVEL,
	// 				   "===== Time in core readblock buf init: %.3f us",
	// 				   core_buf_end_time - core_start_time);
	// 	}

	// 	double core_read_start_time = core_buf_end_time;
	// #endif

	// read the encrypted block from the bdev
	// TODO: kind of redundant to catch and rethrow, but leave for now
	try {
		if ((ret = bfsBlockLayer::readBlock(blk)) == BFS_FAILURE)
			throw BfsServerError("Failed reading block", NULL, NULL);
	} catch (bfsBlockError *err) {
		logMessage(LOG_ERROR_LEVEL, "%s", err->getMessage().c_str());
		delete err;
		throw BfsServerError("Failed reading block", NULL, NULL);
	}

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double core_read_end_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&core_read_end_time) != SGX_SUCCESS) ||
	// 			(core_read_end_time == -1))
	// 			return;

	// 		logMessage(FS_VRB_LOG_LEVEL, "===== Time in core readblock: %.3f
	// us", 				   core_read_end_time - core_read_start_time);
	// 	}

	// 	// double core_enc_start_time = core_read_end_time;

	// 	// // For performance testing
	// 	// if (bfsUtilLayer::perf_test())
	// 	// 	num_reads++;
	// #endif

	// pop the padding that fills the physical block
	// blk.removeTrailer(NULL, UNUSED_PAD_SZ);

	// last 4 bytes unused for now because of the way pkcs7 padding works
	// buf.setData(enc_blk.getBuffer(), BLK_SZ - 4);

	if (!BfsFsLayer::get_SA())
		throw BfsServerError("Failed decrypting, NULL security context", NULL,
							 NULL);

	if (status < FORMATTED)
		throw BfsServerError("Failed read_blk, filesystem not formatted", NULL,
							 NULL);

	uint8_t *mac_copy, *iv;
	mac_copy = (uint8_t *)calloc(BfsFsLayer::get_SA()->getKey()->getMACsize(),
								 sizeof(uint8_t));
	iv = (uint8_t *)calloc(BfsFsLayer::get_SA()->getKey()->getIVlen(),
						   sizeof(uint8_t));
	if (BfsFsLayer::read_blk_meta(blk.get_vbid(), &iv, &mac_copy) !=
		BFS_SUCCESS) {
		free(mac_copy);
		free(iv);
		throw BfsServerError("Failed reading security metadata MAC", NULL,
							 NULL);
	}

	// decrypt and verify MAC
	bfs_vbid_t vbid = blk.get_vbid();
	try {
		// uint64_t blk_ts =
		// bfsBlockLayer::get_vbc()->get_block_timestamp(vbid);
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer();
		aad->resizeAllocation(0, sizeof(vbid), 0);
		*aad << vbid;
		// 	 << blk_ts; // validate using current timestamp, but dont update
		BfsFsLayer::get_SA()->decryptData2(blk, aad, iv, mac_copy);
		delete aad;
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from decrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		throw BfsServerError("Failed decrypting block", NULL, NULL);
	}

	// validate that correct amount of data read, then copy over
	// assert(blk.getLength() == BLK_SZ);
	assert(blk.getLength() == BLK_SZ);

	// if ((status == MOUNTED) && ((vbid >= DATA_REL_START_BLK_NUM) ||
	// 							(vbid < METADATA_REL_START_BLK_NUM))) {
	// 	if (BfsFsLayer::write_blk_meta(blk.get_vbid(), &iv, &mac_copy) !=
	// 		BFS_SUCCESS) {
	// 		free(mac_copy);
	// 		throw BfsServerError("Failed writing security metadata", NULL,
	// 							 NULL);
	// 	}
	// 	free(iv);
	// }

	// For now we have the mt cached in-mem, so just check the root hash
	// ***Only begin checking blocks once the file system has been mounted by
	// the client(s)***
	// TODO: sync with disk periodically
	// if cached (in secure memory), skip MT verification
	if (bfsUtilLayer::use_mt() && (status == MOUNTED) &&
		(ret != BFS_SUCCESS_CACHE_HIT)) {
		// Note: here we use MAC/GMAC size (16B hash) for the block but check
		// the HMAC (32B hash) of the root below

		// iterate backwards and compute hashes for appropriate nodes up to
		// root; compare the result with mt.nodes[0]
		uint8_t *ih = NULL; //, *curr_root = NULL;

		if (!BfsFsLayer::get_mt().nodes[0].hash)
			throw BfsServerError("NULL root hash in read_blk", NULL, NULL);

		// curr_root = (uint8_t *)calloc(
		// 	BfsFsLayer::get_SA()->getKey()->getHMACsize(), sizeof(uint8_t));
		// memcpy(curr_root, BfsFsLayer::get_mt().nodes[0].hash,
		// 	   BfsFsLayer::get_SA()->getKey()->getHMACsize());

		// for (bfs_vbid_t i = BfsFsLayer::get_mt().num_nodes - 1;; i--) {
		bfs_vbid_t i = vbid + ((1 << BfsFsLayer::get_mt().height) - 1);

		if (!BfsFsLayer::get_mt().nodes[i].hash)
			throw BfsServerError("Hash doesnt exist but should in read_blk",
								 NULL, NULL);

		ih = BfsFsLayer::get_mt().nodes[i].hash;
		BfsFsLayer::get_mt().nodes[i].hash = mac_copy;

		// Note: we assume entire mt is kept in-mem (read into mem on mount()),
		// regardless of the cache size for blocks, and therefore only need to
		// compute hashes for internal nodes (ie non-vblock hashes); this also
		// ensures that calls to hash_node>>extract_block_mac>>read_block_helper
		// don't skip verification (since we dont read any blocks from disk;
		// those methods are generally used only during init/flush phases)
		// i = (1 << (BfsFsLayer::get_mt().height - 1)) - 1;

		if (i % 2 == 0)
			i = (i - 2) / 2;
		else
			i = (i - 1) / 2;

		// int hash_sz = BfsFsLayer::get_SA()->getKey()->getHMACsize();
		// uint8_t *curr_par = (uint8_t *)calloc(hash_sz, sizeof(uint8_t));
		int hash_sz = BfsFsLayer::get_SA()->getKey()->getHMACsize();
		bfs_vbid_t cnt = 0;
		while (1) {
			// Compute the new hash for node i based on the current block
			// data/hash that we just read, then compare to what was there
			// before (the previously trusted version)
			memcpy(curr_par, BfsFsLayer::get_mt().nodes[i].hash, hash_sz);

			// while (1) {
			// 	if (i == 0)
			// 		break;

			// these may be dependent on the new vbid so just always
			// recompute them (TODO: optimize later)
			if (BfsFsLayer::hash_node(i, BfsFsLayer::get_mt().nodes[i].hash) !=
				BFS_SUCCESS)
				throw BfsServerError("Failed hash_node in read_blk", NULL,
									 NULL);

			// check the new computed parent against the current parent to do an
			// early return (caching the MT in memory, whether entirely or
			// sparsely, enables this)
			if (memcmp(BfsFsLayer::get_mt().nodes[i].hash, curr_par, hash_sz) !=
				0) {
				char utstr[129];
				bufToString((const char *)curr_par, hash_sz, utstr, 128);
				logMessage(FS_LOG_LEVEL, "curr par hash: [%s]", utstr);
				bufToString((const char *)BfsFsLayer::get_mt().nodes[i].hash,
							hash_sz, utstr, 128);
				logMessage(FS_LOG_LEVEL, "computed par hash: [%s]", utstr);

				free(BfsFsLayer::get_mt()
						 .nodes[i]
						 .hash); // free the newly computed value
				BfsFsLayer::get_mt().nodes[i].hash =
					curr_par; // swap in the old value

				free(BfsFsLayer::get_mt()
						 .nodes[(vbid + (1 << BfsFsLayer::get_mt().height) - 1)]
						 .hash); // free mac_copy
				BfsFsLayer::get_mt()
					.nodes[(vbid + (1 << BfsFsLayer::get_mt().height) - 1)]
					.hash = ih; // swap in the old value
				throw BfsServerError("Invalid par hash comparison in read_blk",
									 NULL, NULL);
			}

			// For debugging, this does a fast return by only validating that
			// the immediate parent is consistent with what we have in the tree
			// in (secure) memory. This shows a 3X performance difference for
			// reads (note that writes always have to go to parent).
			// break;
			cnt++;
			// if (cnt == (BfsFsLayer::get_mt().height / 15))
			if (cnt == 1)
				break;

			// stop if at root
			if (i == 0)
				break;

			// Go to parent (based on if i is a left- or right-child); ensures
			// log2 overhead for mt verification
			if (i % 2 == 0)
				i = (i - 2) / 2;
			else
				i = (i - 1) / 2;
		}

		// cleanup a bit
		free(ih);
		// free(curr_par);

		// 	// Go to parent (based on if i is a left- or right-child);
		// 	// ensures log2 overhead for mt verification
		// 	if (i % 2 == 0)
		// 		i = (i - 2) / 2;
		// 	else
		// 		i = (i - 1) / 2;
		// }

		// check the new computed root against the current root
		// if (memcmp(BfsFsLayer::get_mt().nodes[0].hash, curr_root,
		// 		   BfsFsLayer::get_SA()->getKey()->getHMACsize()) != 0) {
		// 	free(BfsFsLayer::get_mt().nodes[0].hash);
		// 	BfsFsLayer::get_mt().nodes[0].hash = curr_root;

		// 	free(BfsFsLayer::get_mt()
		// 			 .nodes[(vbid + (1 << BfsFsLayer::get_mt().height) - 1)]
		// 			 .hash);
		// 	BfsFsLayer::get_mt()
		// 		.nodes[(vbid + (1 << BfsFsLayer::get_mt().height) - 1)]
		// 		.hash = ih;
		// 	throw BfsServerError("Invalid root hash comparison in read_blk",
		// 						 NULL, NULL);
		// }
	} else {
		free(mac_copy);
	}

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double core_enc_end_time = 0.0;

	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&core_enc_end_time) != SGX_SUCCESS) ||
	// 			(core_enc_end_time == -1))
	// 			return;

	// 		// logMessage(FS_VRB_LOG_LEVEL,
	// 		// 		   "===== Time in core readblock decrypt: %.3f us",
	// 		// 		   core_enc_end_time - core_enc_start_time);

	// 		double core_end_time = core_enc_end_time;
	// 		logMessage(FS_VRB_LOG_LEVEL,
	// 				   "===== Time in core readblock total: %.3f us",
	// 				   core_end_time - core_start_time);
	// 	}
	// #else
	// 	double core_end_time = 0.0;

	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((core_end_time = ocall_get_time2()) == -1)
	// 			return;
	// 		logMessage(FS_VRB_LOG_LEVEL,
	// 				   "===== Time in core readblock total: %.3f us",
	// 				   core_end_time - core_start_time);
	// 	}
	// #endif

	// edit: already decrypted directly into the buffer in the vblock object
	// memcpy(blk.get_data(), out.getBuffer(), out.getLength());

	logMessage(FS_VRB_LOG_LEVEL, "read_blk [%lu] success\n", blk.get_vbid());
}

/**
 * @brief Encrypts and writes a block to the virtual block cluster. Ensures that
 * the buffer is in nonenclave memory so that it can be accessed and sent by the
 * (nonenclave) comms code.
 *
 * @param vbid: the virtual block id to write to
 * @param blk: buffer containing the block data to write
 * @param flags: unused for now (later should be sync/async flags)
 * @return Throws BfsServerError if failure
 */
void BfsHandle::write_blk(VBfsBlock &blk, op_flags_t flags) {
	// write_blk should never be called on the meta blocks, because we dont want
	// them to go through the normal encryption process (they should only be
	// read/written by read/write_blk_meta). However, MKFS should be allowed to
	// pass through here, and we simply return success, otherwise failure.
	// if ((status != FORMATTING) &&
	// 	(blk.get_vbid() >= METADATA_REL_START_BLK_NUM) &&
	// 	(blk.get_vbid() < DATA_REL_START_BLK_NUM))
	if ((blk.get_vbid() >= METADATA_REL_START_BLK_NUM) &&
		(blk.get_vbid() < DATA_REL_START_BLK_NUM))
		throw BfsServerError("Trying to write to meta block directly", NULL,
							 NULL);

	int ret = BFS_FAILURE;

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double core_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&core_start_time) != SGX_SUCCESS) ||
	// 			(core_start_time == -1))
	// 			return;
	// 	}

	// 	// double core_enc_start_time = core_start_time;
	// #else
	// 	double core_start_time = 0.0;

	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((core_start_time = ocall_get_time2()) == -1)
	// 			return;
	// 	}
	// #endif

	if (!BfsFsLayer::get_SA())
		throw BfsServerError("Failed encrypting, NULL security cnontext", NULL,
							 NULL);

	// ***In contrast to read_blk, always update mt hashes for write_blk,
	// because we need set things up properly during mkfs so that read_blk
	// during mount is correct***
	uint8_t *mac_copy = NULL, *iv = NULL;
	// if (bfsUtilLayer::use_mt() && (status == MOUNTED)) {
	// if (status != CORRUPTED) {
	mac_copy = (uint8_t *)calloc(BfsFsLayer::get_SA()->getKey()->getMACsize(),
								 sizeof(uint8_t));
	iv = (uint8_t *)calloc(BfsFsLayer::get_SA()->getKey()->getIVlen(),
						   sizeof(uint8_t));
	// // copy over before block is prepared to be written to disk/network
	// memcpy(mac_copy,
	// 	   &(blk.getBuffer()[BFS_IV_LEN + BLK_SZ + PKCS_PAD_SZ]),
	// 	   BfsFsLayer::get_SA()->getKey()->getMACsize());

	// if ((status != FORMATTING) &&
	// 	(BfsFsLayer::read_blk_meta(blk.get_vbid(), &iv, &mac_copy) !=
	// 	 BFS_SUCCESS)) {
	// 	free(mac_copy);
	// 	throw BfsServerError("Failed reading security metadata MAC", NULL,
	// 						 NULL);
	// }
	// }

	// encrypt and add MAC tag
	// The buffer should contain the IV (12 bytes) + data (4064) + MAC (16
	// bytes) and should fit within the device block size.
	bfs_vbid_t vbid = blk.get_vbid();
	try {
		// bfsBlockLayer::get_vbc()->inc_block_timestamp(
		// 	vbid); // on writes, update virtual timestamp (ie block version)
		// 		   // relative to this block, then do write
		// uint64_t blk_ts =
		// bfsBlockLayer::get_vbc()->get_block_timestamp(vbid);

		// The if guard above checks this.
		// if ((blk.get_vbid() < METADATA_REL_START_BLK_NUM) ||
		// 	(blk.get_vbid() >= DATA_REL_START_BLK_NUM)) {
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer();
		aad->resizeAllocation(0, sizeof(vbid), 0);
		*aad << vbid;
		BfsFsLayer::get_SA()->encryptData2(blk, aad, &iv, &mac_copy);
		delete aad;
		// }
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		throw BfsServerError("Failed encrypting", NULL, NULL);
	}

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double core_enc_end_time = 0.0;

	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&core_enc_end_time) != SGX_SUCCESS) ||
	// 			(core_enc_end_time == -1))
	// 			return;

	// 		logMessage(FS_VRB_LOG_LEVEL,
	// 				   "===== Time up to core writeblock encrypt: %.3f us",
	// 				   core_enc_end_time - core_start_time);
	// 	}
	// #endif

	// validate that correct amount of data written
	// assert(blk.getLength() == (BLK_SZ - 4));
	assert(blk.getLength() == BLK_SZ);

	// The if guard above prevents writing directly to meta blocks, and we dont
	// really need the corrupted check for now.
	// if ((status != CORRUPTED) && ((vbid >= DATA_REL_START_BLK_NUM) ||
	// 							  (vbid < METADATA_REL_START_BLK_NUM))) {
	// if (status != CORRUPTED) {
	if (BfsFsLayer::write_blk_meta(blk.get_vbid(), &iv, &mac_copy) !=
		BFS_SUCCESS) {
		free(mac_copy);
		free(iv);
		throw BfsServerError("Failed writing security metadata", NULL, NULL);
	}
	free(iv);
	// }

	// Pad the encrypted block to the physical block size (should always submit
	// appropriate sized buffers to the block layer), then write the encrypted
	// block to the bdev.
	// char *z = (char *)calloc(1, UNUSED_PAD_SZ);
	// blk.addTrailer(z, UNUSED_PAD_SZ);
	// free(z);

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double core_write_start_time = 0.0;

	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&core_write_start_time) != SGX_SUCCESS) ||
	// 			(core_write_start_time == -1))
	// 			return;
	// 	}
	// #endif

	// TODO: kind of redundant to catch and rethrow, but leave for now
	try {
		if ((ret = bfsBlockLayer::writeBlock(blk, flags)) == BFS_FAILURE)
			throw BfsServerError("Failed writing block", NULL, NULL);
	} catch (bfsBlockError *err) {
		logMessage(LOG_ERROR_LEVEL, "%s", err->getMessage().c_str());
		delete err;
		throw BfsServerError("Failed writing block", NULL, NULL);
	}

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double core_write_mt_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&core_write_mt_start_time) != SGX_SUCCESS) ||
	// 			(core_write_mt_start_time == -1))
	// 			return;
	// 	}
	// #else
	// 	double core_write_mt_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((core_write_mt_start_time = ocall_get_time2()) == -1)
	// 			return;
	// 	}
	// #endif

	// if cached (in secure memory), skip MT verification
	// TODO: double check this before enabling cache for benchmarks
	// if (bfsUtilLayer::use_mt() && (status == MOUNTED) &&
	// 	(ret != BFS_SUCCESS_CACHE_HIT)) {
	if (bfsUtilLayer::use_mt() && (status != CORRUPTED) &&
		(ret != BFS_SUCCESS_CACHE_HIT)) {
		// uint8_t *ih = NULL, *curr_root = NULL;

		if (!BfsFsLayer::get_mt().nodes[0].hash)
			throw BfsServerError("NULL root hash in write_blk", NULL, NULL);

		// curr_root = BfsFsLayer::get_mt().nodes[0].hash;

		// swap the block's hash with the new one, then iterate backwards and
		// compute the necessary new hashes up to the root
		bfs_vbid_t i = vbid + ((1 << BfsFsLayer::get_mt().height) - 1);

		if (!BfsFsLayer::get_mt().nodes[i].hash)
			throw BfsServerError("Hash doesnt exist but should in write_blk",
								 NULL, NULL);

		// bfs_vbid_t node_idx = vbid + (1 << BfsFsLayer::get_mt().height);
		// ih = BfsFsLayer::get_mt().nodes[i].hash;
		free(BfsFsLayer::get_mt().nodes[i].hash);
		BfsFsLayer::get_mt().nodes[i].hash = mac_copy;

		// iterate through all nodes < current block and recompute hashes up to
		// root
		// TODO: optimize later by only computing parent hashes along the
		// block's subtre
		// for (bfs_vbid_t i = node_idx - 1;; i--) {
		// bfs_vbid_t i = (BfsFsLayer::get_mt().num_nodes - 1) -
		// 			   ((1 << BfsFsLayer::get_mt().height) - 1);

		// Note: same deal as above for read_blk
		// i = (1 << (BfsFsLayer::get_mt().height - 1)) - 1;

		if (i % 2 == 0)
			i = (i - 2) / 2;
		else
			i = (i - 1) / 2;

		while (1) {
			if (BfsFsLayer::hash_node(i, BfsFsLayer::get_mt().nodes[i].hash) !=
				BFS_SUCCESS)
				throw BfsServerError("Failed hash_node in write_blk", NULL,
									 NULL);

			// stop if at root
			if (i == 0)
				break;

			// Go to parent (based on if i is a left- or right-child); ensures
			// log2 overhead for mt verification
			if (i % 2 == 0)
				i = (i - 2) / 2;
			else
				i = (i - 1) / 2;
		}

		// TODO: flush new root periodically
		// if (BfsFsLayer::save_root_hash() != BFS_SUCCESS)
		// 	return BFS_FAILURE;
	} else {
		free(mac_copy);
	}

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double core_write_end_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&core_write_end_time) != SGX_SUCCESS) ||
	// 			(core_write_end_time == -1))
	// 			return;

	// 		// logMessage(FS_VRB_LOG_LEVEL, "===== Time in core writeblock: %.3f
	// 		// us", 		   core_write_end_time - core_write_start_time);

	// 		double core_end_time = core_write_end_time;
	// 		logMessage(FS_VRB_LOG_LEVEL,
	// 				   "===== Time in core writeblock mt checks: %.3f us",
	// 				   core_end_time - core_write_mt_start_time);
	// 		logMessage(FS_VRB_LOG_LEVEL,
	// 				   "===== Time in core writeblock total: %.3f us",
	// 				   core_end_time - core_start_time);
	// 	}

	// 	// // For performance testing
	// 	// if (bfsUtilLayer::perf_test())
	// 	// 	num_writes++;
	// #else
	// 	double core_end_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((core_end_time = ocall_get_time2()) == -1)
	// 			return;
	// 		logMessage(FS_VRB_LOG_LEVEL,
	// 				   "===== Time in core writeblock total: %.3f us",
	// 				   core_end_time - core_start_time);
	// 	}
	// #endif

	logMessage(FS_VRB_LOG_LEVEL, "write_blk [%lu] success\n", blk.get_vbid());
}

/**
 * @brief Allocate a file handle for an open file. Limits the number of files
 * that are allowed to be opened. Doesn't use a bitmap so using a monotone
 * counter for fd allocation is fine (so long as the 64 bit handle isnt
 * exhausted).
 *
 * @return bfs_fh_t: a valid file handle if success, 0 if failure (<START_FD)
 */
bfs_fh_t BfsHandle::alloc_fd() {
	if (open_file_tab.size() == MAX_OPEN_FILES) {
		logMessage(LOG_ERROR_LEVEL, "Too many files open\n");
		return 0;
	}

	return next_fd++;
}

/**
 * @brief Allocate a new inode id for a file/dir/link by tracking the bitmap and
 * inode counter in the superblock.
 *
 * @return bfs_ino_id_t: new inode id if success, NULL_INO if at max number of
 * inodes, and throws error on server failure
 */
bfs_ino_id_t BfsHandle::alloc_ino() {
	bfs_ino_id_t no_free, new_ino;
	IBitMap ibm;
	VBfsBlock *ibm_blk_buf;

	// update number of free inodes
	no_free = sb.get_no_inodes_free();

	// Prevent giving an invalid (too high) inode number which will result in
	// (write_inode) overruning the inode table and clobbering the root inode's
	// dentries; just do bounds check and fail when at max number of inodes
	if (no_free == 0) {
		logMessage(LOG_ERROR_LEVEL,
				   "bfs alloc_ino failure: at max number of inodes\n");
		return NULL_INO;
	}

	// read all the ibm blocks so we can search
	for (uint32_t b = 0; b < NUM_IBITMAP_BLOCKS; b++) {
		ibm_blk_buf = new VBfsBlock(NULL, BLK_SZ, 0, 0, 0);
		ibm_blk_buf->set_vbid(IBM_REL_START_BLK_NUM + b);
		read_blk(*ibm_blk_buf);
		ibm.append_ibm_blk(ibm_blk_buf);
	}

	// allocate the new inode number by searching for empty slot in bitmap
	for (uint32_t ibmb = 0; ibmb < ibm.get_ibm_blks().size(); ibmb++) {
		for (uint32_t b = 0; b < BLK_SZ_BITS; b++) {
			if (!bfs_test_bit(b, ibm.get_ibm_blks()[ibmb]->getBuffer())) {
				// found an empty inode slot; allocate the new inode number and
				// write the results
				new_ino = ibmb * BLK_SZ_BITS + b;
				ibm.set_bit(new_ino);
				write_blk(*(ibm.get_ibm_blks()[ibmb]), _bfs__O_SYNC);
				sb.set_no_inodes_free(no_free - 1);

				logMessage(FS_VRB_LOG_LEVEL, "bfs alloc_ino success\n");

				return new_ino;
			}
		}
	}

	// otherwise we have some inconsistency because the no_free is nonzero yet
	// we checked all the bits across the bitmap blocks and could not find an
	// empty inode slot
	logMessage(LOG_ERROR_LEVEL,
			   "bfs alloc_ino failure: could not find a free inode number\n");

	throw BfsServerError(
		"Failed allocating new inode: inconsistency inode table state", NULL,
		NULL);
}

/**
 * @brief Deallocate an inode number so that it can be used by a new file.
 *
 * @param ino: the inode number to free
 * @return Any exceptions are caught by caller
 */
void BfsHandle::dealloc_ino(Inode *ino_ptr) {
	bfs_ino_id_t no_free, del_ino;
	IBitMap ibm;
	VBfsBlock *ibm_blk_buf = new VBfsBlock(NULL, BLK_SZ, 0, 0,
										   0); // cleaned up when ibm destroyed
	// TODO: need locks around access to the sb (inside sb methods)

	// Don't let root inode be deleted for sanity
	if (ino_ptr->get_i_no() == ROOT_INO)
		throw BfsServerError("Trying to deallocate root inode\n", NULL, NULL);

	// update number of free inodes
	no_free = sb.get_no_inodes_free();

	// read the correct ibitmap block
	ibm_blk_buf->set_vbid(IBM_ABSOLUTE_BLK_LOC(ino_ptr->get_i_no()));
	read_blk(*ibm_blk_buf);
	ibm.append_ibm_blk(ibm_blk_buf);

	// update the bitmap block
	ibm.clear_bit(ino_ptr->get_i_no());

	// then write it back to bdev
	write_blk(*(ibm.get_ibm_blks().at(0)), _bfs__O_SYNC);

	// then deallocate all of the inode's data blocks
	delete_inode_iblks(ino_ptr);

	// invalidate the inode and write it (nothing will ever match it in the
	// cache so it should then be evicted quickly)
	del_ino = ino_ptr->get_i_no();
	ino_ptr->set_i_no(NULL_INO);
	if (write_inode(ino_ptr, _bfs__O_SYNC, del_ino, false) != BFS_SUCCESS)
		throw BfsServerError("Failed to write updated path inode\n", NULL,
							 NULL);

	// if write OK, then update the num free inodes
	sb.set_no_inodes_free(no_free + 1);

	logMessage(FS_VRB_LOG_LEVEL, "bfs deallocate inode success\n");
}

/**
 * @brief Deletes (cleans) the iblocks for an inode. Simply notifies the block
 * layer that the blocks no longer contain live contents. The data is encrypted
 * so we don't need to burn the blocks during deallocation.
 *
 * @param ino_ptr: the inode to clean
 * @return throws BfsServerError on failure
 */
void BfsHandle::delete_inode_iblks(Inode *ino_ptr) {
	// The caller should have verified that the file is either a (1) regular
	// file and therefore we can just delete the iblocks normally, or (2)
	// directory file where the dentries were already deleted (we don't do
	// recursive delete for now).
	for (uint32_t iblk_vbid_idx = 0; iblk_vbid_idx < NUM_DIRECT_BLOCKS;
		 iblk_vbid_idx++) {
		// Proceed if we see a valid vbid, otherwise return and done---this
		// doesn't include reserved data blocks or root inode's data blocks
		// (which include DATA_REL_START_BLK_NUM). All valid data blocks are
		// >DATA_REL_START_BLK_NUM.
		if (ino_ptr->get_i_blks().at(iblk_vbid_idx) <= DATA_REL_START_BLK_NUM) {
			logMessage(FS_VRB_LOG_LEVEL, "Done deallocating direct blocks\n");
			return;
		}

		if (sb.dealloc_blk(ino_ptr->get_i_blks().at(iblk_vbid_idx)) !=
			BFS_SUCCESS)
			throw BfsServerError("Failed to deallocate direct block\n", NULL,
								 NULL);
	}

	// Now read the inode's indirect block and if it is valid then deallocate
	// all of the indirect block IDs, then deallocate the indirect block itself
	if (ino_ptr->get_i_blks().at(NUM_DIRECT_BLOCKS) <= DATA_REL_START_BLK_NUM) {
		logMessage(FS_VRB_LOG_LEVEL, "Done deallocating direct blocks\n");
		return;
	}

	VBfsBlock data_blk_buf(NULL, BLK_SZ, 0, 0, 0),
		indir_data_blk_buf(NULL, BLK_SZ, 0, 0, 0);
	IndirectBlock ib;
	int64_t ib_len = 0;

	data_blk_buf.set_vbid(ino_ptr->get_i_blks().at(NUM_DIRECT_BLOCKS));
	read_blk(data_blk_buf);
	ib_len = ib.deserialize(data_blk_buf, 0);
	assert(ib_len <= BLK_SZ);

	for (auto temp_indir_vbid : ib.get_indirect_locs()) {
		if (temp_indir_vbid <= DATA_REL_START_BLK_NUM) {
			logMessage(FS_VRB_LOG_LEVEL, "Done deallocating indirect blocks\n");
			return;
		}

		if (sb.dealloc_blk(temp_indir_vbid) != BFS_SUCCESS)
			throw BfsServerError("Failed to deallocate indirect block\n", NULL,
								 NULL);
	}

	if (sb.dealloc_blk(ino_ptr->get_i_blks().at(NUM_DIRECT_BLOCKS)) !=
		BFS_SUCCESS)
		throw BfsServerError("Failed to deallocate indirect block\n", NULL,
							 NULL);
}

/**
 * @brief Gets a reference to the directory entry cache object.
 *
 * @return const BfsCache&: the reference to the dentry cache
 */
const BfsCache &BfsHandle::get_dentry_cache() { return dentry_cache; }

/**
 * @brief Gets a reference to the inode cache object.
 *
 * @return const BfsCache&: the reference to the inode cache
 */
const BfsCache &BfsHandle::get_ino_cache() { return ino_cache; }

/**
 * @brief Gets the current status of the file system (uninit, init, mounted, or
 * corrupted).
 *
 * @return int32_t: the current status of the system.
 */
int32_t BfsHandle::get_status() { return status; }

/**
 * @brief Search a directory inode's direct data blocks for a dentry(ies).
 * Supports two types of searching, executed based on the de_handler code:
 * searching for a specific dentry (for a get_de call) and scanning all dentries
 * in the direct data blocks (for a readdir call).
 *
 * @param usr: the user sending the access request
 * @param curr_search_de: dentry to search for
 * @param de: pointer to fill with the dentry if found
 * @param all_dentries_searched: flag indicating if all dentries were tested
 * @param curr_parent_ino: starting inode to search through
 * @param de_tested: pointer to track the number of dentries searched
 * @param de_handler: type code indicating the type of search
 * @param ents: container to store dentries for readdir operation
 * @return bool: For get_de operation, true if the dentry was found and false if
 * not. For readdir operation, always true if no failure
 */
bool BfsHandle::check_direct_blks(BfsUserContext *usr,
								  std::string curr_search_de, DirEntry **de,
								  bool *all_dentries_searched,
								  bfs_ino_id_t *curr_parent_ino,
								  uint32_t *de_tested, int32_t de_handler,
								  std::vector<DirEntry *> *ents) {
	VBfsBlock data_blk_buf(NULL, BLK_SZ, 0, 0, 0);
	uint32_t iblk_vbid_idx = 0;
	Inode *curr_parent_ino_ptr = NULL;

	// read the starting parent inode to search
	curr_parent_ino_ptr = read_inode(*curr_parent_ino);

	// ensure the inode on disk was not corrupted somehow
	if (curr_parent_ino_ptr->get_i_no() != *curr_parent_ino) {
		logMessage(LOG_ERROR_LEVEL,
				   "Given ino [%lu] does not match the read inode [%lu]\n",
				   *curr_parent_ino, curr_parent_ino_ptr->get_i_no());
		abort();
	}

	// check access permissions
	if ((BfsACLayer::is_owner(usr, curr_parent_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, curr_parent_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, curr_parent_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", curr_parent_ino_ptr,
								   NULL);

	// only allow dentry checks to be called on directory files
	if (!BFS__S_ISDIR(curr_parent_ino_ptr->get_mode())) {
		logMessage(LOG_ERROR_LEVEL,
				   "Inode is not a directory file (in check direct blocks, "
				   "called by %s): %lu",
				   de_handler == 1 ? "get_de" : "readdir",
				   curr_parent_ino_ptr->get_i_no());
		throw BfsClientRequestFailedError(
			"Inode is not a directory file (check direct blks)\n",
			curr_parent_ino_ptr, NULL);
	}

	// search each direct data block
	for (iblk_vbid_idx = 0; iblk_vbid_idx < NUM_DIRECT_BLOCKS;
		 iblk_vbid_idx++) {
		/**
		 * Check if we searched all dentries in direct blocks. If caller is
		 * get_de, then we did not find the dentry yet, otherwise if the caller
		 * is readdir then we read all the dentries in. Just return.
		 */
		if (curr_parent_ino_ptr->get_i_blks().at(iblk_vbid_idx) <
			DATA_REL_START_BLK_NUM) {
			*all_dentries_searched = true;

			if (!curr_parent_ino_ptr->unlock())
				throw BfsServerError("Failed releasing inode\n", NULL, NULL);

			if (de_handler == 1)
				return false;

			return true;
		}

		// otherwise read the next direct block and keep searching
		data_blk_buf.set_vbid(
			curr_parent_ino_ptr->get_i_blks().at(iblk_vbid_idx));
		read_blk(data_blk_buf);

		if (check_each_dentry(data_blk_buf, curr_parent_ino_ptr, de,
							  all_dentries_searched, de_tested, de_handler,
							  curr_search_de, ents)) {
			if (de_handler == 1) {
				*curr_parent_ino = (*de)->get_ino();
				if (!curr_parent_ino_ptr->unlock())
					throw BfsServerError("Failed releasing inode\n", NULL,
										 NULL);
				return true;
			}
		}

		if (*all_dentries_searched) {
			if (!curr_parent_ino_ptr->unlock())
				throw BfsServerError("Failed releasing inode\n", NULL, NULL);

			if (de_handler == 1)
				return false;

			return true;
		}
	}

	// At this point, there are still dentries to search in indirect blocks

	assert(*de_tested <= curr_parent_ino_ptr->get_i_links_count());

	if (!curr_parent_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	if (de_handler == 1)
		return false;

	return true;
}

/**
 * @brief Search a directory inode's indirect data blocks for a dentry(ies).
 * Supports two types of searching, executed based on the de_handler code:
 * searching for a specific dentry (for a get_de call) and scanning all dentries
 * in the indirect data blocks (for a readdir call).
 *
 * @param usr: the user sending the access request
 * @param curr_search_de: dentry to search for
 * @param de: pointer to fill with the dentry if found
 * @param all_dentries_searched: flag indicating if all dentries were tested
 * @param curr_parent_ino: starting inode to search through
 * @param de_tested: pointer to track the number of dentries searched
 * @param de_handler: type code indicating the type of search
 * @param ents: container to store dentries for readdir operation
 * @return bool: For get_de operation, true if the dentry was found and false if
 * not. For readdir operation, always true if no failure
 */
bool BfsHandle::check_indirect_blks(BfsUserContext *usr,
									std::string curr_search_de, DirEntry **de,
									bool *all_dentries_searched,
									bfs_ino_id_t *curr_parent_ino,
									uint32_t *de_tested, int32_t de_handler,
									std::vector<DirEntry *> *ents) {
	VBfsBlock data_blk_buf(NULL, BLK_SZ, 0, 0, 0);
	uint64_t ib_len = 0;
	Inode *curr_parent_ino_ptr = NULL;
	IndirectBlock ib;

	// read the starting parent inode to search
	curr_parent_ino_ptr = read_inode(*curr_parent_ino);

	// ensure the inode on disk was not corrupted somehow
	if (curr_parent_ino_ptr->get_i_no() != *curr_parent_ino) {
		logMessage(LOG_ERROR_LEVEL,
				   "Given ino [%lu] does not match the read inode [%lu]\n",
				   *curr_parent_ino, curr_parent_ino_ptr->get_i_no());
		abort();
	}

	// check access permissions
	if ((BfsACLayer::is_owner(usr, curr_parent_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, curr_parent_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, curr_parent_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", curr_parent_ino_ptr,
								   NULL);

	// only allow dentry checks to be called on directory files
	if (!BFS__S_ISDIR(curr_parent_ino_ptr->get_mode())) {
		logMessage(LOG_ERROR_LEVEL, "Inode is not a directory file: %lu",
				   curr_parent_ino_ptr->get_i_no());
		throw BfsClientRequestFailedError(
			"Inode is not a directory file (check indirect blks)\n",
			curr_parent_ino_ptr, NULL);
	}

	// TODO: We were throwing from here on read_blk error, but server errors are
	// fatal so dont care about unlocking the inode at that point.

	// read the inode's indirect block
	data_blk_buf.set_vbid(
		curr_parent_ino_ptr->get_i_blks().at(NUM_DIRECT_BLOCKS));
	read_blk(data_blk_buf);
	ib_len = ib.deserialize(data_blk_buf, 0);
	assert(ib_len <= BLK_SZ);

	// search each indirect data block
	for (auto temp_indir_vbid : ib.get_indirect_locs()) {
		/**
		 * Check if we searched all dentries in indirect data blocks. If caller
		 * is get_de, then we did not find the dentry, otherwise if the caller
		 * is readdir then we read all the dentries in. Just return.
		 */
		if (temp_indir_vbid < DATA_REL_START_BLK_NUM) {
			*all_dentries_searched = true;

			if (!curr_parent_ino_ptr->unlock())
				throw BfsServerError("Failed releasing inode\n", NULL, NULL);

			if (de_handler == 1)
				return false;

			return true;
		}

		// otherwise read the next indirect data block and keep searching
		data_blk_buf.resizeAllocation(0, BLK_SZ, 0); // resize for reading
		data_blk_buf.burn(); // clear contents from old block
		data_blk_buf.set_vbid(temp_indir_vbid);
		read_blk(data_blk_buf);

		if (check_each_dentry(data_blk_buf, curr_parent_ino_ptr, de,
							  all_dentries_searched, de_tested, de_handler,
							  curr_search_de, ents)) {
			if (de_handler == 1) {
				*curr_parent_ino = (*de)->get_ino();
				if (!curr_parent_ino_ptr->unlock())
					throw BfsServerError("Failed releasing inode\n", NULL,
										 NULL);
				return true;
			}
		}

		if (*all_dentries_searched) {
			if (!curr_parent_ino_ptr->unlock())
				throw BfsServerError("Failed releasing inode\n", NULL, NULL);

			if (de_handler == 1)
				return false;

			return true;
		}
	}

	// At this point, there are no more dentries to search in indirect blocks

	assert(*de_tested == curr_parent_ino_ptr->get_i_links_count());

	*all_dentries_searched = true;

	if (!curr_parent_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	if (de_handler == 1)
		return false;

	return true;
}

/**
 * @brief Parses each dentry in the given block and (depending on what type of
 * search operation is being done) checks a specific condition on each. Right
 * now this method is used only by get_de and readdir calls (through
 * check_(in)direct_blks methods), so we have two type of conditions to check
 * and react on. Might change later to use callbacks.
 *
 * @param blk: the block id of the dentry
 * @param data_blk_buf: the block to parse
 * @param ino_ptr: the pointer to the (directory) inode that owns the block
 * @param de: the de to find (for a get_de call)
 * @param all_dentries_searched: flag indicating if all dentries were searched
 * @param de_tested: number of dentries checked
 * @param de_handler: type of search operation
 * @param search_de: de to search for (for a get_de call)
 * @param ents: dentry vector to fill (for a readir call)
 * @return bool: For get_de operation, true if the dentry was found and false if
 * not. For readdir operation, true if all dentries searched (if no failure)
 */
bool BfsHandle::check_each_dentry(VBfsBlock &data_blk_buf, Inode *ino_ptr,
								  DirEntry **de, bool *all_dentries_searched,
								  uint32_t *de_tested, int32_t de_handler,
								  std::string search_de,
								  std::vector<DirEntry *> *ents) {
	DirEntry curr_de, *temp_de = NULL;
	int64_t temp_de_len = 0;

	// stop if all dentries were searched
	if ((de_handler != 3) && (*de_tested == ino_ptr->get_i_links_count())) {
		*all_dentries_searched = true;

		if (de_handler == 1)
			return false;

		return true;
	}

	// search each dentry in the block
	for (uint32_t de_idx = 0; de_idx < NUM_DIRENTS_PER_BLOCK; de_idx++) {
		temp_de_len =
			curr_de.deserialize(data_blk_buf, DENTRY_ABSOLUTE_BLK_OFF(de_idx));
		assert(temp_de_len == DIRENT_SZ);

		// skip only if we are _not_ searching for empty dentries (e.g., for a
		// create/mkdir) and the dentry is invalid (i.e., if the inode was
		// deleted it will be 0 and the dentry will be empty string)
		if ((de_handler != 3) &&
			((curr_de.get_de_name().compare(std::string("")) == 0) ||
			 (curr_de.get_ino() < ROOT_INO)))
			continue;

		// put this temporarily visited dentry in dcache
		temp_de = new DirEntry(curr_de.get_de_name(), curr_de.get_ino(),
							   data_blk_buf.get_vbid(), de_idx);

		if (de_handler !=
			3) // wait to cache when handler==3 so we dont cache empty dentries
			write_dcache(stringCacheKey(curr_de.get_de_name()), temp_de);

		switch (de_handler) {
		case 1: // get_de handler
			if ((curr_de.get_de_name().compare(search_de) == 0) &&
				(curr_de.get_ino() >= ROOT_INO)) {
				*de = temp_de; // record de and keep the lock
				return true;
			}

			// otherwise just release the lock
			if (!temp_de->unlock())
				throw BfsServerError("Failed releasing de\n", NULL, NULL);

			break;
		case 3: // searching for empty dentries for create/mkdir
			if ((curr_de.get_de_name().compare(search_de) == 0)) {
				*de = temp_de; // record de and keep the lock
				return true;
			}

			// otherwise the dentry is not empty, so just cache it and release
			// the lock
			write_dcache(stringCacheKey(curr_de.get_de_name()), temp_de);
			if (!temp_de->unlock())
				throw BfsServerError("Failed releasing de\n", NULL, NULL);

			break;
		case 2: // readdir handler
			if ((curr_de.get_de_name().compare(std::string("")) != 0) &&
				(curr_de.get_ino() >= ROOT_INO)) {
				// edit: loc set in constructor now
				// *de = temp_de; // set so that the caller can set the blk_loc;
				ents->push_back(temp_de);
			}

			// always release the lock after accounting for the de in ents
			if (!temp_de->unlock())
				throw BfsServerError("Failed releasing de\n", NULL, NULL);

			break;
		default:
			break;
		}

		// otherwise check next entry
		*de_tested += 1;

		// Stop if all dentries were searched. This might happen in the middle
		// of a block so we check here instead of just breaking loop and
		// checking after; need to do this check at every iteration, otherwise
		// all_dentries_searched won't be set and will cause get_de to continue
		// searching in an unallocated indirect block
		if ((de_handler != 3) && (*de_tested == ino_ptr->get_i_links_count())) {
			*all_dentries_searched = true;

			if (de_handler == 1)
				return false;

			return true;
		}
	}

	// fail if it reaches here for get_de (couldnt find dentry but didnt finish
	// searching all of the inode's dentries), or when searching for empty
	// dentries for create/mkdir
	if ((de_handler == 1) || (de_handler == 3))
		return false;

	// OK if it reaches here for readdir
	return true;
}

/**
 * @brief Initializes and writes a dentry object to an inode's direct blocks.
 *
 * @param par_ino: the parent inode to add the dentry to
 * @param new_ino: the new inode to add to the parent
 * @param path: name of the dentry
 * @return int32_t: BFS_SUCCESS if it was added, BFS_FAILURE or BfsServerError
 * on failure
 */
int32_t BfsHandle::add_dentry_to_direct_blks(Inode *par_ino_ptr,
											 Inode *new_ino_ptr,
											 std::string path) {
	bfs_vbid_t new_blk;
	DirEntry *de = NULL;
	int64_t de_len = 0;
	uint64_t dir_idx = 0;
	VBfsBlock dir_blk_buf(NULL, BLK_SZ, 0, 0, 0);
	bool skip_read = false; // skip read if newly allocated (MAC not written)

	// unused for the purpose of adding dentry
	bool all_dentries_searched = false;
	uint32_t de_tested = 0;

	// Need to loop through and find an empty slot in all of the direct blocks
	// (later move to hash-based extent style like ext3/4) and if none then we
	// try to allocate a new direct block
	for (dir_idx = 0; dir_idx < NUM_DIRECT_BLOCKS; dir_idx++) {
		/**
		 * If the value at the index in the direct block is 0, need to allocate
		 * a new block, then write the dentry to the newly allocated direct
		 * block.
		 */
		if (par_ino_ptr->get_i_blks().at(dir_idx) < DATA_REL_START_BLK_NUM) {
			if (!(new_blk = sb.alloc_blk())) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failed allocating a new direct block vbid\n");
				return BFS_FAILURE;
			}

			skip_read = true;
			par_ino_ptr->set_i_blk(dir_idx, new_blk);
			if (write_inode(par_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL, "Failed writing new inode\n");
				return BFS_FAILURE;
			}
		}

		// then read the direct block and check the dentries for an empty slot
		dir_blk_buf.set_vbid(par_ino_ptr->get_i_blks().at(dir_idx));
		if (!skip_read) {
			read_blk(dir_blk_buf);
		} else {
			// otherwise just resize the buffer for writing, then search for an
			// empty entry in the empty block buffer (should take the first
			// slot)
			dir_blk_buf.resizeAllocation(0, BLK_SZ, 0);

			// clear contents for so we correctly check not block (ie otherwise
			// the first block's contents will be leftover when a dentry needs
			// to be allocated in the next block; and so we will end up not
			// finding an empty dentry in the first slot of the next block; we
			// will end up only with empty slots for NUM_DIRENTS_PER_BLOCK as
			// the dir_blk_buf accummulates those files in the inode's first
			// data block)
			dir_blk_buf.burn();
		}

		if (check_each_dentry(dir_blk_buf, par_ino_ptr, &de,
							  &all_dentries_searched, &de_tested, 3,
							  std::string(""), NULL)) {
			/**
			 * Found an empty slot. Create the new dentry, map it to the correct
			 * inode, then serialize new dentry into the direct data block at
			 * the correct offset and write it.
			 */
			// de = new DirEntry(path, new_ino_ptr->get_i_no(),
			// par_ino_ptr->get_i_blks().at(dir_idx), empty_dentry_idx);
			de->set_de_name(path);
			de->set_ino(new_ino_ptr->get_i_no());
			de->set_blk_loc(
				par_ino_ptr->get_i_blks().at(dir_idx)); // blk index already set
			de_len = de->serialize(dir_blk_buf,
								   DENTRY_ABSOLUTE_BLK_OFF(de->get_idx_loc()));
			assert(de_len == DIRENT_SZ);
			write_dcache(stringCacheKey(de->get_de_name()), de);
			write_blk(dir_blk_buf, _bfs__O_SYNC);

			// update the parent inode
			par_ino_ptr->set_i_links(par_ino_ptr->get_i_links_count() + 1);
			if (write_inode(par_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL, "Failed updating parent inode\n");
				return BFS_FAILURE;
			}

			if (de && !de->unlock())
				throw BfsServerError("Failed releasing de\n", NULL, NULL);

			return BFS_SUCCESS;
		}

		// otherwise couldnt find an empty slot, just try next block
	}

	// if we checked all direct blocks and we couldn't find an empty slot, then
	// search the indirect data blocks
	logMessage(LOG_ERROR_LEVEL, "Could not find empty slot in direct blocks\n");
	return BFS_FAILURE;
}

/**
 * @brief Initializes and writes a dentry object to an inode's indirect blocks.
 *
 * @param par_ino: the parent inode to add the dentry to
 * @param new_ino: the new inode to add to the parent
 * @param path: name of the dentry
 * @return int32_t: BFS_SUCCESS if it was added, BFS_FAILURE or BfsServerError
 * on failure
 */
int32_t BfsHandle::add_dentry_to_indirect_blks(Inode *par_ino_ptr,
											   Inode *new_ino_ptr,
											   std::string path) {
	bfs_vbid_t new_blk;
	DirEntry *de;
	uint64_t indir_idx = 0;
	int64_t ib_len = 0, de_len = 0;
	VBfsBlock indir_blk_buf(NULL, BLK_SZ, 0, 0, 0),
		indir_data_blk_buf(NULL, BLK_SZ, 0, 0, 0);
	IndirectBlock ib;
	bool skip_read = false; // skip read if newly allocated (MAC not written)

	// unused for the purpose of adding dentry
	bool all_dentries_searched = false;
	uint32_t de_tested = 0;

	/**
	 * If the indirect block is unallocated (assumes that when the inode was
	 * first written, it zeroed the iblks), then allocate it, otherwise read it
	 * from the block device.
	 */
	if (par_ino_ptr->get_i_blks().at(NUM_DIRECT_BLOCKS) <
		DATA_REL_START_BLK_NUM) {
		if (!(new_blk = sb.alloc_blk())) {
			logMessage(LOG_ERROR_LEVEL,
					   "Failed allocating a new indirect block\n");
			return BFS_FAILURE;
		}

		skip_read = true; // redundant, invalid blkid caught below anyway
		par_ino_ptr->set_i_blk(NUM_DIRECT_BLOCKS, new_blk);
		indir_blk_buf.set_vbid(new_blk);

		// we know we are going to write it since it is a newly allocated block,
		// so just resize the buffer for writing (already zeroed)
		indir_blk_buf.resizeAllocation(0, BLK_SZ, 0);
	} else {
		indir_blk_buf.set_vbid(par_ino_ptr->get_i_blks().at(NUM_DIRECT_BLOCKS));
		read_blk(indir_blk_buf);
		ib_len = ib.deserialize(indir_blk_buf, 0);
		assert(ib_len <= BLK_SZ);
	}

	// Need to loop through each block in the ib and find an empty slot (later
	// move to hash-based extent style like ext3/4) and if none then we try to
	// allocate a new data block in the ib
	for (indir_idx = 0; indir_idx < ib.get_indirect_locs().size();
		 indir_idx++) {
		/**
		 * If the value at the index in the block is 0, need to allocate
		 * a new block, then write the dentry to the newly allocated
		 * block.
		 */
		if (ib.get_indirect_locs().at(indir_idx) < DATA_REL_START_BLK_NUM) {
			if (!(new_blk = sb.alloc_blk())) {
				logMessage(
					LOG_ERROR_LEVEL,
					"Failed allocating a new indirect data block vbid\n");
				return BFS_FAILURE;
			}

			skip_read = true; // might be redundant if it was already a new ib
			ib.set_indirect_loc(indir_idx, new_blk);
			ib_len = ib.serialize(indir_blk_buf, 0);
			assert(ib_len <= BLK_SZ);
			write_blk(indir_blk_buf, _bfs__O_SYNC);
		}

		indir_data_blk_buf.set_vbid(ib.get_indirect_locs().at(indir_idx));
		if (!skip_read) {
			read_blk(indir_data_blk_buf);
		} else {
			// otherwise just resize the buffer for writing, then search for an
			// empty entry in the empty block buffer (should take the first
			// slot)
			indir_data_blk_buf.resizeAllocation(0, BLK_SZ, 0);

			// clear contents for so we correctly check not block (ie otherwise
			// the first block's contents will be leftover when a dentry needs
			// to be allocated in the next block; and so we will end up not
			// finding an empty dentry in the first slot of the next block; we
			// will end up only with empty slots for NUM_DIRENTS_PER_BLOCK as
			// the indir_data_blk_buf accummulates those files in the inode's
			// first data block)
			indir_data_blk_buf.burn();
		}

		if (check_each_dentry(indir_data_blk_buf, par_ino_ptr, &de,
							  &all_dentries_searched, &de_tested, 3,
							  std::string(""), NULL)) {
			/**
			 * Found an empty slot. Create the new dentry, map it to the correct
			 * inode, then serialize new dentry into the indirect data block at
			 * the correct offset and write it.
			 */
			de->set_de_name(path);
			de->set_ino(new_ino_ptr->get_i_no());
			de->set_blk_loc(
				ib.get_indirect_locs().at(indir_idx)); // blk index already set
			de_len = de->serialize(indir_data_blk_buf,
								   DENTRY_ABSOLUTE_BLK_OFF(de->get_idx_loc()));
			assert(de_len == DIRENT_SZ);
			write_dcache(stringCacheKey(de->get_de_name()), de);
			write_blk(indir_data_blk_buf, _bfs__O_SYNC);

			// update the parent inode
			par_ino_ptr->set_i_links(par_ino_ptr->get_i_links_count() + 1);
			if (write_inode(par_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL, "Failed updating parent inode\n");
				return BFS_FAILURE;
			}

			if (de && !de->unlock())
				throw BfsServerError("Failed releasing de\n", NULL, NULL);

			return BFS_SUCCESS;
		}

		// otherwise couldnt find an empty slot, just try next block in the ib
	}

	// if we checked all indirect data blocks and we couldn't find an empty
	// slot, then no space left in the directory (>7787 dentries)
	logMessage(LOG_ERROR_LEVEL,
			   "Directory too large, could not find empty dentry slot\n");
	return BFS_FAILURE;
}

/**
 * @brief Cleanup callback for inodes. This is only executed on an insert to
 * cache from read_inode, in which the calling thread currently owns both the
 * inode cache mutex. So no other thread can touch the cache until we are done.
 * However another thread might currently own the lock on the ino_ptr, so we
 * have to try to acquire the lock (waiting for any other threads to finish with
 * it) before cleaning it up. Then the calling thread can finish the
 * cb>>insert and release the inode_mutex so the changes to the cache are
 * reflected to any other threads trying to read from the cache.
 *
 * @param owned_ptr: the inode currently locked by the calling thread
 * @param ptr: the inode for which to acquire the lock and cleanup/flush
 * @return void: throws exception on error
 */
void BfsHandle::flush_inode(Inode *replaced_ptr) {
	/**
	 * At this point we have the cache lock and we need to get the object lock,
	 * flush if dirty, then we can safely unlock and let caller delete the
	 * pointer. No other threads can thereafter acquire the pointer even if they
	 * can call checkCache before the callers can invoke delete, since we ensure
	 * in with remove that the pointer is no longer present in the cache.
	 */

	Inode *replaced_ino_ptr = dynamic_cast<Inode *>(replaced_ptr);

	if (!replaced_ino_ptr)
		throw BfsServerError("replace pointer is bad in inode_cleanup_cb\n",
							 NULL, NULL);

	if (!replaced_ino_ptr->lock())
		throw BfsServerError("Error when acquiring lock in inode_cleanup_cb\n",
							 NULL, NULL);

	if (replaced_ino_ptr->is_dirty() &&
		write_inode(replaced_ino_ptr, _bfs__O_SYNC, 0, false) != BFS_SUCCESS)
		throw BfsServerError("Failed to write updated parent inode\n", NULL,
							 NULL);

	if (!replaced_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode in inode_cleanup_cb\n",
							 NULL, NULL);

	// TODO: might just delete pointer here instead
}

/**
 * @brief Cleanup callback for dentries.
 *
 * @param key: the dentry key to flush
 * @return bool: true if cleanup OK, false otherwise
 */
void BfsHandle::flush_dentry(DirEntry *replaced_ptr) {
	DirEntry *replaced_de = dynamic_cast<DirEntry *>(replaced_ptr);
	int64_t de_len;
	VBfsBlock data_blk_buf(NULL, BLK_SZ, 0, 0, 0);

	if (!replaced_de)
		throw BfsServerError("replace pointer is bad in de_cleanup_cb\n", NULL,
							 NULL);

	if (!replaced_de->lock())
		throw BfsServerError("Error when acquiring lock in de_cleanup_cb\n",
							 NULL, NULL);

	if (replaced_de->is_dirty()) {
		data_blk_buf.set_vbid(replaced_de->get_blk_loc());
		read_blk(data_blk_buf);
		de_len = replaced_de->serialize(
			data_blk_buf, DENTRY_ABSOLUTE_BLK_OFF(replaced_de->get_idx_loc()));
		assert(de_len == DIRENT_SZ);
		write_blk(data_blk_buf, _bfs__O_SYNC);
	}

	if (!replaced_de->unlock())
		throw BfsServerError("Failed releasing inode in de_cleanup_cb\n", NULL,
							 NULL);

	// TODO: might just delete pointer here instead
}

/**
 * @brief Retrieve an entry from the dentry cache by key.
 *
 * @param key: the key to search by
 * @param pop: flag indicating whether to pop if found
 * @return DirEntry*: pointer to the found dentry object
 */
DirEntry *BfsHandle::read_dcache(stringCacheKey key, bool pop) {
	CacheableObject *obj = dentry_cache.checkCache(key, 0, pop);

	if (bfsUtilLayer::cache_enabled() && obj)
		return dynamic_cast<DirEntry *>(obj);

	return NULL;
}

/**
 * @brief Add a new entry to the dentry cache. Requires the caller to own the
 * lock for the key, otherwise there might be some deadlocking issues.
 *
 * @param key: key to add to the cache
 * @param de: pointer to object to add
 * @return Throws BfsServerError if failure
 */
void BfsHandle::write_dcache(stringCacheKey key, DirEntry *de) {
	DirEntry *cached_de;
	CacheableObject *obj;

	if (bfsUtilLayer::cache_enabled() &&
		(obj = dentry_cache.insertCache(key, 0, de))) {
		// sanity check
		if (!(cached_de = dynamic_cast<DirEntry *>(obj)))
			throw BfsServerError("Failed cast dentry ptr\n", NULL, NULL);

		if (cached_de != de) {
			flush_dentry(cached_de);
			delete cached_de;
		}
	}
}

/**
 * @brief Read an on-bdev inode structure into an in-memory structure. Checks
 * the cache first.
 *
 * @param ino: the inode number to read
 * @param pop: flag indicating to pop the inode from the cache during read
 * @return Inode*: pointer to the read inode structure, throws BfsServerError if
 * failure
 */
Inode *BfsHandle::read_inode(bfs_ino_id_t ino, bool pop) {
	VBfsBlock path_ino_blk(NULL, BLK_SZ, 0, 0, 0);
	Inode *path_ino_ptr = NULL, *cached_ip;
	int64_t path_ino_len = 0;
	CacheableObject *obj;

	logMessage(FS_VRB_LOG_LEVEL, "Trying to read inode [%lu]\n", ino);

	// if the inode is not in the cache, then read from disk
	if (!(obj = ino_cache.checkCache(intCacheKey(ino), 1, pop))) {
		path_ino_blk.set_vbid(ITAB_ABSOLUTE_BLK_LOC(ino));
		read_blk(path_ino_blk);

		path_ino_ptr = new Inode(); // locked on create
		path_ino_len =
			path_ino_ptr->deserialize(path_ino_blk, ITAB_ABSOLUTE_BLK_OFF(ino));
		assert(path_ino_len <= INODE_SZ);

		if (bfsUtilLayer::cache_enabled() &&
			(obj = ino_cache.insertCache(intCacheKey(path_ino_ptr->get_i_no()),
										 1, path_ino_ptr))) {
			// sanity check
			if (!(cached_ip = dynamic_cast<Inode *>(obj)))
				throw BfsServerError("Failed cast for inode ptr\n", NULL, NULL);

			if (cached_ip != path_ino_ptr) {
				flush_inode(cached_ip);
				delete cached_ip;
			}
		}
	} else {
		// sanity check
		if (!(path_ino_ptr = dynamic_cast<Inode *>(obj)))
			throw BfsServerError("Failed cast for inode ptr\n", NULL, NULL);
	}

	/**
	 * Sanity check that we didnt read some invalid inode structure; the
	 * serialized inode might be zero if written after an unlink/rmdir, although
	 * the dentry should also be gone and this should never hit (unless
	 * read_inode called directly by something with a particular inode number).
	 */
	if (!path_ino_ptr ||
		(path_ino_ptr && (path_ino_ptr->get_i_no() == NULL_INO)))
		throw BfsServerError("read_inode found bad inode\n", NULL, NULL);

	logMessage(FS_VRB_LOG_LEVEL, "Successfully read inode [%lu]\n", ino);
	logMessage(FS_VRB_LOG_LEVEL, "Inode cache hit rate: %.2f%%\n",
			   ino_cache.get_hit_rate() * 100.0);

	return path_ino_ptr;
}

/**
 * @brief Write/persist an in-memory inode structure to a block device. If
 * synchronous write, then write directly to the block device, otherwise just
 * try to put in cache.
 *
 * @param ino_ptr: the inode to write
 * @param flags: unused for now (later should be sync/async flags)
 * @param del_ino: the deleted inode num (possibly used to get location)
 * @param put_cache: flag indicating whether to write ino to cache
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE or BfsServerError if
 * failure
 */
int32_t BfsHandle::write_inode(Inode *ino_ptr, op_flags_t flags,
							   bfs_ino_id_t del_ino, bool put_cache) {
	VBfsBlock itab_empty_blk_buf(NULL, BLK_SZ, 0, 0, 0);
	int64_t itab_entry_len = 0;
	bfs_ino_id_t ino_loc =
		(!put_cache && (del_ino > 0)) ? del_ino : ino_ptr->get_i_no();
	Inode *cached_ip;
	CacheableObject *obj;

	logMessage(FS_VRB_LOG_LEVEL, "Trying to write inode [%lu]\n",
			   ino_ptr->get_i_no());

	if (flags & _bfs__O_SYNC) {
		/**
		 * Read data in the itab blk to avoid overwriting other entries, then
		 * serialize the inode into the correct location in the block, and
		 * finally write it back to bdev.
		 */
		long b_off = ITAB_ABSOLUTE_BLK_LOC(ino_loc);
		itab_empty_blk_buf.set_vbid(b_off);
		read_blk(itab_empty_blk_buf);
		long c_off = ITAB_ABSOLUTE_BLK_OFF(ino_loc);
		itab_entry_len = ino_ptr->serialize(itab_empty_blk_buf, c_off);
		assert(itab_entry_len <= INODE_SZ);
		write_blk(itab_empty_blk_buf, flags);
	}

	logMessage(FS_VRB_LOG_LEVEL, "Successfully wrote inode [%lu]\n",
			   ino_ptr->get_i_no());

	/**
	 * If write was OK, then if the ino wasnt deleted (i.e., if del_ino == 0),
	 * check if we need to put the inode into the cache.
	 */
	if (!del_ino && put_cache) {
		if (bfsUtilLayer::cache_enabled() &&
			(obj = ino_cache.insertCache(intCacheKey(ino_loc), 1, ino_ptr))) {
			// sanity check
			if (!(cached_ip = dynamic_cast<Inode *>(obj)))
				throw BfsServerError("Failed cast for inode ptr\n", NULL,
									 ino_ptr);

			if (cached_ip != ino_ptr) {
				flush_inode(cached_ip);
				delete cached_ip;
			}
		}
	}

	return BFS_SUCCESS;
}

/**
 * @brief Finds the directory entry associated with the given absolute path by
 * walking the inode table and associated direct/indirect data blocks for the
 * parent directories. Places the visited dentries in the cache.
 *
 * @param de: dentry to fill if found
 * @param path: given absolute path name
 * @return int32_t: BFS_SUCCESS if dentry found, BFS_FAILURE if not
 */
int32_t BfsHandle::get_de(BfsUserContext *usr, DirEntry **de, std::string path,
						  bool pop) {
	std::string path_copy(path);
	std::string path_str(path);

	logMessage(FS_VRB_LOG_LEVEL, "Dentry cache hit rate: %.2f%%\n",
			   dentry_cache.get_hit_rate() * 100.0);

	// only allow absolute paths in the file system for now
	if (path[0] != '/') {
		logMessage(LOG_ERROR_LEVEL, "Path is not absolute: %s\n", path.c_str());
		return BFS_FAILURE;
	}

	// check if the actual dentry (full path) is already cached (also acquires
	// the de lock for the caller)
	if ((*de = read_dcache(stringCacheKey(path), pop)) != NULL) {
		logMessage(FS_VRB_LOG_LEVEL,
				   "Dentry found in cache in get_de [path=%s, ino=%d]\n",
				   (*de)->get_de_name().c_str(), (*de)->get_ino());
		return BFS_SUCCESS;
	}

	// if not cached, then check if root dir, if so then skip below
	if (path_str.compare(std::string("/")) == 0) {
		*de = new DirEntry("/", sb.get_root_ino(), DATA_REL_START_BLK_NUM, 0);
		write_dcache(stringCacheKey((*de)->get_de_name()), *de);
		return BFS_SUCCESS;
	}

	/**
	 * Here we parse the entire absolute path name by tokenizing it (starting
	 * with the root dir as the parent) and searching for the successive child
	 * dentries by performing a linear walk of the itable and the associated
	 * inode iblks. First checks the direct data blocks, then checks the
	 * indirect data blocks (until we move to a btree approach for dentry
	 * searching).
	 */
	bfs_ino_id_t curr_parent_ino = sb.get_root_ino(); // starting parent
	bool all_dentries_searched = false, de_found = false;
	size_t start = 1;
	uint32_t de_tested = 0;
	std::string delim = "/";
	size_t end = 0;
	std::string curr_search_de_str = "";

	while (end != std::string::npos) {
		// find delim starting at particular pos to get next dir in path
		end = path_copy.find(delim, start);

		// take whole path up to the current dir
		curr_search_de_str = path_copy.substr(0, end);

		/**
		 * Always try to unlock de at the beginning of loop. On the last dentry
		 * (the one we search for), this strategy will keep the lock acquired
		 * for the caller, since the loop will exit before it tries to unlock.
		 */
		if (*de && !(*de)->unlock())
			throw BfsServerError("Failed releasing de\n", NULL, NULL);

		// check if the curr_search_de_str  is already cached
		if ((*de = read_dcache(stringCacheKey(curr_search_de_str), pop)) !=
			NULL) {
			// set this as the current parent
			curr_parent_ino = (*de)->get_ino();

			// search for next child
			start = end + delim.length();
			// end = path_copy.find(delim, start);
			// path_copy.substr(1, end); // last token (basename)
			continue;
		}

		// otherwise read the current par inode's dentries to find
		// curr_search_dir
		de_tested = 0; // reset de_tested for the current search dentry's parent
		de_found = check_direct_blks(usr, curr_search_de_str, de,
									 &all_dentries_searched, &curr_parent_ino,
									 &de_tested, 1, NULL);

		// if found then search for next child
		if (de_found) {
			start = end + delim.length();
			// end = path_copy.find(delim, start);
			continue;
		}

		// if it wasnt found and no more dentries then return fail
		if (all_dentries_searched)
			return BFS_FAILURE;

		// otherwise search for next child in indirect blocks
		// edit: otherwise search for the current child in indirect blocks
		// curr_search_de_str = path_copy.substr(start, end - start);
		// start = end + delim.length();
		// end = path_copy.find(delim, start);

		de_found = check_indirect_blks(usr, curr_search_de_str, de,
									   &all_dentries_searched, &curr_parent_ino,
									   &de_tested, 1, NULL);

		// if found then search for next child
		if (de_found) {
			start = end + delim.length();
			// end = path_copy.find(delim, start);
			continue;
		}

		// if it wasnt found and no more dentries then return fail
		if (all_dentries_searched)
			return BFS_FAILURE;

		// very bad - all dentries should be searched by this point
		logMessage(LOG_ERROR_LEVEL,
				   "All dentries not searched but should be.\n");
		abort();
	}

	/**
	 * At this point the parsing should have found each dentry in the absolute
	 * path and exited the loop normally (by reaching the end of the path
	 * string). *de should be the dentry for the path name.
	 */
	if ((!*de) || ((*de)->get_de_name().compare(path) != 0))
		return BFS_FAILURE;

	return BFS_SUCCESS;
}

/**
 * @brief Initializes dependent bfs layers and core data structures structures
 * for the file system, mainly a single (independent) virtual block cluster (the
 * block layer) and the caches (Note: Superblock will be read and initialized
 * when mounting the file system).
 */
BfsHandle::BfsHandle() {
	// ensure that caches are sized correctly (even though util layer may have
	// been initialized previously)
	ino_cache.set_max_sz(bfsUtilLayer::getUtilLayerCacheSizeLimit());
	dentry_cache.set_max_sz(bfsUtilLayer::getUtilLayerCacheSizeLimit());

	next_fd = START_FD;
	status = INITIALIZED;

	logMessage(FS_LOG_LEVEL, "BfsHandle init success\n");
}

/**
 * @brief Cleanup the dyanmically allocated objects in the BfsHandle object.
 */
BfsHandle::~BfsHandle() {
	// TODO: shutdown all the layers

	next_fd = START_FD;
	status = UNINITIALIZED;

	// Log all results before finishing
	// write_core_latencies();

	logMessage(FS_LOG_LEVEL, "BfsHandle destroy success\n");
}

/**
 * @brief Format virtual block device with layout of BFS.
 *
 * Process:
 * 1. write superblock
 * 2. write empty inode bitmap
 * 3. write empty inode table
 * 4. allocate root inode in-memory and on bdev
 */
int32_t BfsHandle::mkfs() {
	logMessage(FS_LOG_LEVEL, "Formatting bdev for BFS ...\n");

	if (status != INITIALIZED)
		throw BfsServerError("Invalid server status in mkfs [!=INITIALIZED]\n",
							 NULL, NULL);
	status = FORMATTING;

	// First we need to setup an initial state for the merkle tree, so that
	// further mutations (during mkfs and after) are reflected properly.
	if (bfsUtilLayer::use_mt()) {
		if (BfsFsLayer::init_merkle_tree(true) != BFS_SUCCESS)
			throw BfsServerError("Failed initializing merkle tree\n", NULL,
								 NULL);
		logMessage(FS_LOG_LEVEL, "Merkle tree initialized");

	} else {
		logMessage(FS_LOG_LEVEL, "Merkle tree disabled, skipping mt init");
	}

	/**
	 * 1. Alloc buffer for superblock, fill it, then write to bdev. Note: need
	 * to read some of these params from device geo (eg no_blks, no_grp). Needed
	 * to add +1 for next_vbid/first_data_blk so alloc for the first file wont
	 * get the the root inode's first data blk (i.e., we reserve it).
	 */
	bfs_vbid_t blk_target = 0;
	VBfsBlock super_buf(NULL, BLK_SZ, 0, 0, 0);
	int64_t super_len = 0;
	SuperBlock super;

	super.set_magic(BFS_SB_MAGIC);
	super.set_sb_params(BLK_SZ, INODE_SZ, NUM_BLOCKS, NUM_DATA_BLOCKS,
						NUM_INODES, NUM_DATA_BLOCKS, NUM_UNRES_INODES,
						DATA_REL_START_BLK_NUM + 1);
	super.set_reserved_inos(ROOT_INO, IBITMAP_INO, ITABLE_INO, JOURNAL_INO,
							FIRST_UNRESERVED_INO);
	super.set_state(FORMATTED);
	super_len = super.serialize(super_buf, 0);
	assert(super_len <= SB_SZ);

	super_buf.set_vbid(SB_REL_START_BLK_NUM);
	logMessage(FS_VRB_LOG_LEVEL, "MKFS: writing block [%d]",
			   SB_REL_START_BLK_NUM);
	write_blk(super_buf, _bfs__O_SYNC);

	// write empty MT block for now
	VBfsBlock empty_buf(NULL, BLK_SZ, 0, 0, 0);
	blk_target += 1;
	empty_buf.set_vbid(blk_target);
	write_blk(empty_buf, _bfs__O_SYNC);

	/**
	 * 2. Alloc and write zero'd out buffer for inode bitmap. Also
	 * reserve the first DATA_REL_START_BLK_NUM blocks (by setting
	 * vbid=DATA_REL_START_BLK_NUM), and the first FIRST_UNRESERVED_INO inodes
	 * in the ibitmap. Note that resv inodes bits should fit in the first ibm
	 * blk, so write it and then lopo through the rest and write empty blocks.
	 */
	IBitMap ibm;
	Inode *ino_ptr, *rt_ptr;
	std::vector<Inode *> res_inos; // for writing into itable after
	// VBfsBlock *ibm_blk_buf =
	// 	new VBfsBlock(NULL, BLK_SZ, 0); // cleaned up when ibm destroyed
	VBfsBlock ibm_blk_buf(NULL, BLK_SZ, 0, 0,
						  0); // cleaned up when ibm destroyed

	ibm.append_ibm_blk(&ibm_blk_buf);

	for (bfs_vbid_t b = 0; b < FIRST_UNRESERVED_INO; b++) {
		ibm.set_bit(b);

		ino_ptr = new Inode();
		ino_ptr->set_i_no(b);

		if (b == ROOT_INO) {
			rt_ptr = ino_ptr;
			ino_ptr->set_uid(0);
			ino_ptr->set_mode(BFS__S_IFDIR | 0777);
			ino_ptr->set_ref_cnt(1); // always keep root dir open
			ino_ptr->set_i_links(2); // "." and ".." subdirectories
			ino_ptr->set_i_blk(0, DATA_REL_START_BLK_NUM);
		}

		res_inos.push_back(ino_ptr);
	}

	blk_target += 1;
	ibm.get_ibm_blks().at(0)->set_vbid(blk_target);
	logMessage(FS_VRB_LOG_LEVEL, "MKFS: writing block [%d]", blk_target);
	write_blk(*(ibm.get_ibm_blks().at(0)), _bfs__O_SYNC);
	ibm.get_ibm_blks().at(0)->resizeAllocation(0, BLK_SZ, 0);
	ibm.get_ibm_blks().at(0)->burn(); // clear contents from old block and just
									  // use this zeroed block for the loop

	// memset(ibm.get_ibm_blks().at(0)->getBuffer(), 0x0, BLK_SZ); //
	// handled by burn
	for (uint32_t x = 1; x < NUM_IBITMAP_BLOCKS; x++) {
		blk_target += 1;
		ibm.get_ibm_blks().at(0)->set_vbid(blk_target);
		logMessage(FS_VRB_LOG_LEVEL, "MKFS: writing block [%d]", blk_target);
		write_blk(*(ibm.get_ibm_blks().at(0)), _bfs__O_SYNC);
		ibm.get_ibm_blks().at(0)->resizeAllocation(
			0, BLK_SZ, 0); // resize header space again for writing
	}

	/**
	 * 3. Alloc and write empty blocks for the inode table. Note that the
	 * reserved inodes should fit in the first itab block, so write it and then
	 * loop through the rest and write empty blocks.
	 */
	VBfsBlock itab_empty_blk_buf(NULL, BLK_SZ, 0, 0, 0);
	int64_t itab_entry_len = 0;

	for (uint32_t b = 0; b < FIRST_UNRESERVED_INO; b++) {
		itab_entry_len = res_inos.at(b)->serialize(itab_empty_blk_buf,
												   ITAB_ABSOLUTE_BLK_OFF(b));
		assert(itab_entry_len <= INODE_SZ);
	}

	blk_target += 1;
	itab_empty_blk_buf.set_vbid(blk_target);
	logMessage(FS_VRB_LOG_LEVEL, "MKFS: writing block [%d]", blk_target);
	write_blk(itab_empty_blk_buf, _bfs__O_SYNC);
	itab_empty_blk_buf.resizeAllocation(0, BLK_SZ, 0);
	itab_empty_blk_buf.burn(); // clear contents from old block and just
							   // use this zeroed block for the loop

	memset(itab_empty_blk_buf.getBuffer(), 0x0, BLK_SZ);
	for (uint32_t x = 1; x < NUM_ITAB_BLOCKS; x++) {
		blk_target += 1;
		itab_empty_blk_buf.set_vbid(blk_target);
		logMessage(FS_VRB_LOG_LEVEL, "MKFS: writing block [%d]", blk_target);
		write_blk(itab_empty_blk_buf, _bfs__O_SYNC);
		itab_empty_blk_buf.resizeAllocation(0, BLK_SZ, 0);
	}

	assert(blk_target == (METADATA_REL_START_BLK_NUM - 1));
	empty_buf.resizeAllocation(0, BLK_SZ, 0);
	empty_buf.burn();
	for (bfs_vbid_t x = 0; x < NUM_META_BLOCKS; x++) {
		blk_target += 1;
		empty_buf.set_vbid(blk_target);
		if (blk_target % 1000 == 0) {
			logMessage(FS_LOG_LEVEL, "MKFS: writing block [%d]", blk_target);
		}
		// write_blk(empty_buf, _bfs__O_SYNC); // ignore for now so we dont bump
		// into meta block writes during mkfs
		empty_buf.resizeAllocation(0, BLK_SZ, 0);
		empty_buf.burn();
	}

	/**
	 * 4. Allocate/reserve root inode. Note: We already reserved the inode in
	 * the bitmap and itable above and wrote to bdev, now just need to create
	 * the default dentries for the root inode. We always use the first
	 * available data block (DATA_REL_START_BLK_NUM) for the root dentries (in
	 * the sb we mark DATA_REL_START_BLK_NUM+1 as the next available for other
	 * files).
	 */
	VBfsBlock de_buf(NULL, BLK_SZ, 0, 0, 0);
	int64_t de_len = 0;
	DirEntry de(".", rt_ptr->get_i_no(), DATA_REL_START_BLK_NUM, 0);

	de_len = de.serialize(de_buf, DENTRY_ABSOLUTE_BLK_OFF(0));
	assert(de_len == DIRENT_SZ);

	de.set_de_name(std::string(".."));
	de.set_blk_idx_loc(1);
	de_len = de.serialize(de_buf, DENTRY_ABSOLUTE_BLK_OFF(1));
	assert(de_len == DIRENT_SZ);

	blk_target += 1;
	assert(blk_target == DATA_REL_START_BLK_NUM);
	de_buf.set_vbid(blk_target);
	logMessage(FS_LOG_LEVEL, "MKFS: writing block [%d]", blk_target);
	write_blk(de_buf, _bfs__O_SYNC);

	// add appropriate MACs for the rest of the blocks
	blk_target += 1;
	empty_buf.resizeAllocation(0, BLK_SZ, 0);
	empty_buf.burn();
	for (bfs_vbid_t b = blk_target; b < NUM_BLOCKS; b++) {
		empty_buf.set_vbid(b);
		if (b % 1000 == 0) {
			logMessage(FS_LOG_LEVEL, "MKFS: writing block [%d]", b);
		}
		write_blk(empty_buf, _bfs__O_SYNC);
		empty_buf.resizeAllocation(0, BLK_SZ, 0);
		empty_buf.burn();
	}

	/**
	 * Cleanup (must wait until dentries are written before rt_ptr can be
	 * deleted). Dont care about unlocking during mkfs.
	 */
	for (auto iptr : res_inos)
		delete iptr;

	// Now we flush the final state after mkfs. Here we reserve space for MT
	// root hash. Block hashes are in-line in blocks for now so that they are
	// easier to move around; alternatively may keep all block hashes for the MT
	// in a reserved portion of disk
	if (bfsUtilLayer::use_mt()) {
		// if (status < INITIALIZED) {
		if (BfsFsLayer::flush_merkle_tree() != BFS_SUCCESS)
			throw BfsServerError("Failed flushing merkle tree\n", NULL, NULL);
		logMessage(FS_LOG_LEVEL, "Merkle tree flushed");
		// } else {
		// 	throw BfsServerError("Invalid server status [>=INITIALIZED]\n",
		// 						 NULL, NULL);
		// }
	}

	status = FORMATTED;
	logMessage(FS_LOG_LEVEL, "Done formatting");

	return BFS_SUCCESS;
}

/**
 * @brief Read required contents from block devices to initialize the in-memory
 * file system structures and mount the file system. The rest of the data
 * structures are read/deserialized on-demand.
 *
 * Process (assuming we have active connections with block devices after init):
 * 1. read and fill superblock
 * 2. read and initialize the root inode and dentry
 *
 * @return BFS_SUCCESS if success, BFS_FAILURE or exception if failure
 */
int32_t BfsHandle::mount() {
	// This may be an error by the client (ie if they try to mount before mkfs)
	// or the server (ie a bug that calls mount before mkfs); for simplicity, we
	// will fail immediately with a server error to debug if this case is hit,
	// because we know how our client code behaves.
	if (status != FORMATTED)
		throw BfsServerError("Trying to mount from incorrect fs state.\n", NULL,
							 NULL);

	// Now first initialize the merkle tree from disk before we proceed with any
	// further reads (or writes). Here we initialize merkle tree from the disk
	// contents (expects to already read a formatted disk); only init mt once
	// (ie first client).
	if (bfsUtilLayer::use_mt()) {
		if (BfsFsLayer::init_merkle_tree() != BFS_SUCCESS)
			throw BfsServerError("Failed initializing merkle tree\n", NULL,
								 NULL);
		logMessage(FS_LOG_LEVEL, "Merkle tree initialized");
	} else {
		logMessage(FS_LOG_LEVEL, "Merkle tree disabled, skipping mt init");
	}

	VBfsBlock data_blk_buf(NULL, BLK_SZ, 0, 0, 0);
	int64_t sb_len, de_len;
	Inode *rt_ino_ptr;
	DirEntry *rt_de;

	// setup the global par hash buf here for reads/writes
	curr_par = (uint8_t *)calloc(BfsFsLayer::get_SA()->getKey()->getHMACsize(),
								 sizeof(uint8_t));

	// read the superblock and deserialize it to get root inode info
	data_blk_buf.set_vbid(SB_REL_START_BLK_NUM);
	read_blk(data_blk_buf);
	sb_len = sb.deserialize(data_blk_buf, 0);
	assert(sb_len <= SB_SZ);
	assert(sb.get_root_ino() == ROOT_INO);

	// read the root inode structure from bdev
	rt_ino_ptr = read_inode(sb.get_root_ino());
	assert(rt_ino_ptr->get_i_no() == sb.get_root_ino());
	assert(rt_ino_ptr->get_i_blks().at(0) == DATA_REL_START_BLK_NUM);
	data_blk_buf.set_vbid(rt_ino_ptr->get_i_blks().at(0));
	data_blk_buf.resizeAllocation(0, BLK_SZ, 0);
	data_blk_buf.burn(); // clear contents from old block
	read_blk(data_blk_buf);

	// get the root inode default dentries (both are in i_blk 0)
	rt_de = new DirEntry("", UINT64_MAX, rt_ino_ptr->get_i_blks().at(0), 0);
	de_len = rt_de->deserialize(data_blk_buf, DENTRY_ABSOLUTE_BLK_OFF(0));
	assert(de_len == DIRENT_SZ);
	assert(rt_de->get_de_name().compare(std::string(".")) == 0);
	write_dcache(stringCacheKey(rt_de->get_de_name()), rt_de);
	if (rt_de && !rt_de->unlock())
		throw BfsServerError("Failed releasing de\n", NULL, NULL);

	rt_de = new DirEntry("", UINT64_MAX, rt_ino_ptr->get_i_blks().at(0), 1);
	de_len = rt_de->deserialize(data_blk_buf, DENTRY_ABSOLUTE_BLK_OFF(1));
	assert(de_len == DIRENT_SZ);
	assert(rt_de->get_de_name().compare(std::string("..")) == 0);
	write_dcache(stringCacheKey(rt_de->get_de_name()), rt_de);
	if (rt_de && !rt_de->unlock())
		throw BfsServerError("Failed releasing de\n", NULL, NULL);

	if (!rt_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	status = MOUNTED;
	logMessage(FS_LOG_LEVEL, "Done mounting to client");

	return BFS_SUCCESS;
}

/**
 * @brief Get the attributes of an inode (file/directory). Called by stat.
 *
 * @param path absolute path for the file/directory
 * @param fino inode number to fill in for the file/dir
 * @param fmode mode to fill in for the file/dir
 * @param fsize size to fill in for the file/dir
 * @return int32_t: BFS_SUCCESS if success, throws exception if failure
 */
int32_t BfsHandle::bfs_getattr(BfsUserContext *usr, std::string path,
							   bfs_uid_t *uid, bfs_ino_id_t *fino,
							   uint32_t *fmode, uint64_t *fsize) {
	Inode *path_ino_ptr;
	DirEntry *de;
	int32_t res;

	// check if file name length valid
	if ((path.size() + 1) > MAX_FILE_NAME_LEN)
		throw BfsClientRequestFailedError("File name too long\n", NULL, NULL);

	// get the dentry
	de = NULL;
	if ((res = get_de(usr, &de, path)) != BFS_SUCCESS) {
		logMessage(FS_LOG_LEVEL, "File does not exist: %s", path.c_str());
		throw BfsClientRequestFailedError("File does not exist\n", NULL, NULL);
	}

	// get the inode for the dentry
	if (!(path_ino_ptr = read_inode(de->get_ino())))
		throw BfsServerError("Failed reading inode for dentry\n", NULL, NULL);

	/**
	 * Need to release immediately when done. Subsequent code or code from
	 * another thread may induce a cache eviction on e.g. this dentry and might
	 * need the lock. This generously early releases for other threads, and
	 * prevents deadlocks of this thread on itself (e.g., if it might induce an
	 * ejection on the path dentry or its parent).
	 */
	if (de && !de->unlock())
		throw BfsServerError("Failed releasing de\n", NULL, NULL);

	if ((BfsACLayer::is_owner(usr, path_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, path_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, path_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", NULL, path_ino_ptr);

	// fill in the attributes
	*uid = path_ino_ptr->get_uid();
	*fino = path_ino_ptr->get_i_no();
	*fmode = path_ino_ptr->get_mode();
	*fsize = path_ino_ptr->get_size();

	if (!path_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	return BFS_SUCCESS;
}

/**
 * @brief Open a directory file and return a file handle to the client. Same
 * process at bfs_open(), except no flags here.
 *
 * @param path absolute directory name
 * @return bfs_fh_t handle associated with the open directory, throws exception
 * on failure
 */
bfs_fh_t BfsHandle::bfs_opendir(BfsUserContext *usr, std::string path) {
	Inode *path_ino_ptr;
	OpenFile *of;
	DirEntry *de;
	bfs_fh_t fh;
	int32_t res;

	// check if file name length valid
	if ((path.size() + 1) > MAX_FILE_NAME_LEN)
		throw BfsClientRequestFailedError("File name too long\n", NULL, NULL);

	// get the dentry
	de = NULL;
	if ((res = get_de(usr, &de, path)) != BFS_SUCCESS) {
		logMessage(FS_LOG_LEVEL, "File does not exist: %s", path.c_str());
		throw BfsClientRequestFailedError("File does not exist\n", NULL, NULL);
	}

	// get the inode for the dentry
	if (!(path_ino_ptr = read_inode(de->get_ino())))
		throw BfsServerError("failed read_inode\n", NULL, NULL);

	if (de && !de->unlock())
		throw BfsServerError("Failed releasing de\n", NULL, NULL);

	if ((BfsACLayer::is_owner(usr, path_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, path_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, path_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", NULL, path_ino_ptr);

	if (!BFS__S_ISDIR(path_ino_ptr->get_mode()))
		throw BfsClientRequestFailedError("File is not a directory\n", NULL,
										  path_ino_ptr);

	// allocate a file handle and open file object
	if ((fh = alloc_fd()) < START_FD)
		throw BfsServerError("Invalid file handle in opendir\n", NULL,
							 path_ino_ptr);

	path_ino_ptr->set_ref_cnt(path_ino_ptr->get_ref_cnt() + 1);
	if (write_inode(path_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS)
		throw BfsServerError("Failed to write updated inode\n", NULL, NULL);

	of = new OpenFile(path_ino_ptr->get_i_no(), 0);
	open_file_tab.insert(std::make_pair(fh, of));

	logMessage(FS_VRB_LOG_LEVEL, "opened directory file [%lu]\n", fh);

	if (!path_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	return fh;
}

/**
 * @brief Read all dentries associated with the given file handle.
 *
 * @param fh the open directory file handle
 * @param ents vector to fill with dentries
 * @return int32_t: BFS_SUCCESS if success, throws exception if failure
 */
int32_t BfsHandle::bfs_readdir(BfsUserContext *usr, bfs_fh_t fh,
							   std::vector<DirEntry *> *ents) {
	OpenFile *of;
	bfs_ino_id_t fino;
	bool all_dentries_searched;
	uint32_t de_tested;

	// get the openfile object
	if (!(of = open_file_tab.at(fh)))
		throw BfsServerError("Error during bfs_release find openfile\n", NULL,
							 NULL);

	// get the inode number of the openfile
	if ((fino = of->get_ino()) < ROOT_INO)
		throw BfsServerError("Error during bfs_release get inode id\n", NULL,
							 NULL);

	/**
	 * Check the in/direct blocks for dentries. Note that de_handler==2 ensures
	 * that all dentry locks are released before the check functions return.
	 */
	de_tested = 0;
	all_dentries_searched = false;
	if (!check_direct_blks(usr, "", NULL, &all_dentries_searched, &fino,
						   &de_tested, 2, ents))
		throw BfsServerError("Failed reading dentries in direct blocks\n", NULL,
							 NULL);

	if (!all_dentries_searched &&
		!check_indirect_blks(usr, "", NULL, &all_dentries_searched, &fino,
							 &de_tested, 2, ents))
		throw BfsServerError("Failed reading dentries in indirect blocks\n",
							 NULL, NULL);

	return BFS_SUCCESS;
}

/**
 * @brief Create a directory. Doesn't handle recursive create for now.
 * Essentially the same process at bfs_create. Unlike create, does not open the
 * directory. Opendir should be called instead, and reads on the handle should
 * always go through readdir and check that the inode is a directory file. Also
 * here we have to alloc a data block for the default dentries and write them.
 *
 * @param path: absolute directory name
 * @param mode: directory permission bits
 * @return bfs_fh_t: handle for the open directory, failed request if too many
 * files exist (no more inodes), and throws exception if failure
 */
int32_t BfsHandle::bfs_mkdir(BfsUserContext *usr, std::string path,
							 uint32_t mode) {
	Inode *par_ino_ptr, *new_ino_ptr;
	bfs_ino_id_t ino;
	DirEntry *de, sub_de;
	int32_t res;
	int64_t de_len;
	bfs_vbid_t blk_target;
	std::string dirname, err_msg;
	char *path_copy;
	VBfsBlock de_buf(NULL, BLK_SZ, 0, 0, 0);

	// check if file name length valid
	if ((path.size() + 1) > MAX_FILE_NAME_LEN)
		throw BfsClientRequestFailedError("File name too long\n", NULL, NULL);

	// get the dentry
	de = NULL;
	if ((res = get_de(usr, &de, path)) != BFS_FAILURE) {
		if (de && !de->unlock())
			throw BfsServerError("Failed releasing de\n", NULL, NULL);

		throw BfsClientRequestFailedError("File exists\n", NULL, NULL);
	}

	// get dentry and inode of the parent directory
	path_copy = bfs_strdup(path.c_str());
	dirname = std::string(bfs_dirname(path_copy));
	free(path_copy);
	de = NULL;

	if ((res = get_de(usr, &de, dirname)) != BFS_SUCCESS)
		throw BfsServerError("Parent dir does not exist\n", NULL, NULL);

	if (!(par_ino_ptr = read_inode(de->get_ino())))
		throw BfsServerError("Parent inode does not exist\n", NULL, NULL);

	if (de && !de->unlock())
		throw BfsServerError("Failed releasing de\n", NULL, NULL);

	if ((BfsACLayer::is_owner(usr, par_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, par_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, par_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", par_ino_ptr, NULL);

	if (!BFS__S_ISDIR(par_ino_ptr->get_mode()))
		throw BfsServerError("Parent inode is not a dir\n", par_ino_ptr, NULL);

	// create new inode, write the default dentries then write to bdev
	if ((ino = alloc_ino()) < FIRST_UNRESERVED_INO)
		throw BfsClientRequestFailedError(
			"Failed allocating new inode: too many files\n", par_ino_ptr, NULL);

	// unlike create, nothing opened here; but ref links 2 for default dentries
	new_ino_ptr =
		new Inode(ino, usr->get_uid(), BFS__S_IFDIR | mode, 0, 0, 0, 0, 0, 2);

	sub_de.set_de_name(std::string("."));
	sub_de.set_ino(new_ino_ptr->get_i_no()); // set ino to self
	de_len = sub_de.serialize(de_buf, DENTRY_ABSOLUTE_BLK_OFF(0));
	assert(de_len == DIRENT_SZ);

	sub_de.set_de_name(std::string(".."));
	sub_de.set_ino(par_ino_ptr->get_i_no()); // set to parent
	de_len = sub_de.serialize(de_buf, DENTRY_ABSOLUTE_BLK_OFF(1));
	assert(de_len == DIRENT_SZ);

	if (!(blk_target = sb.alloc_blk())) // only need 1 block
		throw BfsServerError("Failed allocating a new direct block\n",
							 par_ino_ptr, new_ino_ptr);

	// mark the block as belonging to the new inode
	new_ino_ptr->set_i_blk(0, blk_target);

	// write the dentries for the new inode
	de_buf.set_vbid(blk_target);
	write_blk(de_buf, _bfs__O_SYNC);

	// first try to add to direct blocks, then try indirect blocks (let short
	// circuit return error)
	if ((add_dentry_to_direct_blks(par_ino_ptr, new_ino_ptr, path) !=
		 BFS_SUCCESS) &&
		(add_dentry_to_indirect_blks(par_ino_ptr, new_ino_ptr, path) !=
		 BFS_SUCCESS))
		throw BfsServerError(
			"Failed adding dentry to direct or indirect blocks\n", par_ino_ptr,
			new_ino_ptr);

	if (!par_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	// Write the new inode (note that since we are creating a new inode, if
	// there is an error just manually unlock and then delete the inode). The
	// write may also eject an entry from the cache (including the parent) so
	// need to ensure the parent lock is released before calling or it will
	// result in a deadlock.
	if (write_inode(new_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS) {
		if (!new_ino_ptr->unlock())
			err_msg = "Failed writing inode and failed releasing inode\n";
		else
			err_msg = "Failed writing inode\n";
		delete new_ino_ptr;
		throw BfsServerError(err_msg, NULL, NULL);
	}

	if (!new_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	return BFS_SUCCESS;
}

/**
 * @brief Delete a directory. First removes the dentry from the parent inode's
 * data blocks, then removes the actual directory's inode from the itable.
 * Also deallocates the directory's data blocks (assuming the directory is
 * empty; doesn't handle recursive delete as of now).
 *
 * @param path the absolute path of the directory to remove
 * @return int32_t: BFS_SUCCESS if success, throws exception if failure
 */
int32_t BfsHandle::bfs_rmdir(BfsUserContext *usr, std::string path) {
	DirEntry *de, *par_de;
	Inode *par_ino_ptr, *path_ino_ptr;
	std::string dirname;
	char *path_copy;
	int32_t res;
	VBfsBlock data_blk_buf(NULL, BLK_SZ, 0, 0, 0);
	int64_t temp_de_len;

	// check if file name length valid
	if ((path.size() + 1) > MAX_FILE_NAME_LEN)
		throw BfsClientRequestFailedError("File name too long\n", NULL, NULL);

	de = NULL;
	if ((res = get_de(usr, &de, path, true)) != BFS_SUCCESS) {
		logMessage(FS_LOG_LEVEL, "File does not exist: %s", path.c_str());
		throw BfsClientRequestFailedError("File does not exist\n", NULL, NULL);
	}

	// get the inode for the dentry (and pop from cache)
	if (!(path_ino_ptr = read_inode(de->get_ino(), true)))
		throw BfsServerError("Failed reading inode for dentry\n", NULL, NULL);

	if ((BfsACLayer::is_owner(usr, path_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, path_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, path_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", NULL, path_ino_ptr);

	// only allow rmdir to be called on directories
	if (!BFS__S_ISDIR(path_ino_ptr->get_mode())) {
		logMessage(LOG_ERROR_LEVEL, "Inode is not a directory file: %lu",
				   path_ino_ptr->get_i_no());
		throw BfsClientRequestFailedError(
			"Inode is not a directory file (rmdir)\n", NULL, path_ino_ptr);
	}

	// Doesn't handle recursive delete as of now
	if (path_ino_ptr->get_i_links_count() > 2)
		throw BfsClientRequestFailedError("Directory is not empty\n",
										  path_ino_ptr, NULL);

	// only allow delete if no open handles
	if ((path.compare("/") != 0) && (path_ino_ptr->get_ref_cnt() > 0))
		throw BfsClientRequestFailedError("Directory inode is still open\n",
										  path_ino_ptr, NULL);

	// delete the inode
	dealloc_ino(path_ino_ptr);

	if (!path_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	// get dentry and inode of the parent directory
	path_copy = bfs_strdup(path.c_str());
	dirname = std::string(bfs_dirname(path_copy));
	free(path_copy);
	par_de = NULL;

	if ((res = get_de(usr, &par_de, dirname)) != BFS_SUCCESS)
		throw BfsServerError("Parent dir does not exist\n", NULL, NULL);

	if (!(par_ino_ptr = read_inode(par_de->get_ino())))
		throw BfsServerError("Parent inode does not exist\n", NULL, NULL);

	if (par_de && !par_de->unlock())
		throw BfsServerError("Failed releasing par_de\n", NULL, NULL);

	if ((BfsACLayer::is_owner(usr, par_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, par_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, par_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", par_ino_ptr, NULL);

	if (!BFS__S_ISDIR(par_ino_ptr->get_mode()))
		throw BfsServerError("Parent inode is not a dir\n", par_ino_ptr, NULL);

	// read the block where the dentry was found by get_de and clear the dentry
	data_blk_buf.set_vbid(de->get_blk_loc());
	read_blk(data_blk_buf);
	// data_blk_buf.resizeAllocation(NULL, BLK_SZ,
	// 							BFS_IV_LEN,
	// 							BFS_MAC_LEN);

	// now serialize an empty dentry and write it
	de->set_de_name("");
	de->set_ino(0);
	temp_de_len =
		de->serialize(data_blk_buf, DENTRY_ABSOLUTE_BLK_OFF(de->get_idx_loc()));
	assert(temp_de_len == DIRENT_SZ);

	// Note: try to pop from cache and delete if present then just delete the
	// object if it was cached (as retrieved from get_de above), this just
	// deletes the cached ptr, otherwise this just deletes the newly created ptr
	// if ((cached_de = read_dcache(stringCacheKey(path), true)))
	// 	delete cached_de;
	delete de;

	write_blk(data_blk_buf, _bfs__O_SYNC);

	// update the parent inode ilinks count
	par_ino_ptr->set_i_links(par_ino_ptr->get_i_links_count() - 1);
	if (write_inode(par_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS)
		throw BfsServerError("Failed to write updated parent inode\n",
							 par_ino_ptr, NULL);

	if (!par_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	return BFS_SUCCESS;
}

/**
 * @brief Delete a file. First removes the dentry from the parent inode's
 * data blocks, then removes the actual file's inode from the itable.
 * Essentially the same as bfs_rmdir(). Only used for regular files for now
 * (i.e., same as rm utility). Also deallocates the regular file's data blocks.
 *
 * @param path the absolute path of the file to remove
 * @return int32_t: BFS_SUCCESS if success, throws exception if failure
 */
int32_t BfsHandle::bfs_unlink(BfsUserContext *usr, std::string path) {
	DirEntry *de, *par_de;
	Inode *par_ino_ptr, *path_ino_ptr;
	std::string dirname;
	char *path_copy;
	int32_t res;
	VBfsBlock data_blk_buf(NULL, BLK_SZ, 0, 0, 0);
	int64_t temp_de_len;

	// check if file name length valid
	if ((path.size() + 1) > MAX_FILE_NAME_LEN)
		throw BfsClientRequestFailedError("File name too long\n", NULL, NULL);

	de = NULL;
	if ((res = get_de(usr, &de, path, true)) != BFS_SUCCESS) {
		logMessage(FS_LOG_LEVEL, "File does not exist: %s", path.c_str());
		throw BfsClientRequestFailedError("File does not exist\n", NULL, NULL);
	}

	// get the inode for the dentry (and pop from cache)
	if (!(path_ino_ptr = read_inode(de->get_ino(), true)))
		throw BfsServerError("Failed reading inode for dentry\n", NULL, NULL);

	if ((BfsACLayer::is_owner(usr, path_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, path_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, path_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", NULL, path_ino_ptr);

	// only allowing unlink to be called on regular files
	if (!BFS__S_ISREG(path_ino_ptr->get_mode()))
		throw BfsClientRequestFailedError("Inode is not a regular file\n", NULL,
										  path_ino_ptr);

	if (path_ino_ptr->get_i_links_count() > 0)
		throw BfsServerError("Regular file has i_links and shouldnt\n", NULL,
							 path_ino_ptr);

	// only allow delete if no open handles
	if ((path.compare("/") != 0) && (path_ino_ptr->get_ref_cnt() > 0))
		throw BfsClientRequestFailedError("Regular inode is still open\n", NULL,
										  path_ino_ptr);

	// delete the inode
	dealloc_ino(path_ino_ptr);

	if (!path_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	// get dentry and inode of the parent directory
	path_copy = bfs_strdup(path.c_str());
	dirname = std::string(bfs_dirname(path_copy));
	free(path_copy);
	par_de = NULL;

	if ((res = get_de(usr, &par_de, dirname)) != BFS_SUCCESS)
		throw BfsServerError("Parent dir does not exist\n", NULL, NULL);

	if (!(par_ino_ptr = read_inode(par_de->get_ino())))
		throw BfsServerError("Parent inode does not exist\n", NULL, NULL);

	if (par_de && !par_de->unlock())
		throw BfsServerError("Failed releasing par_de\n", NULL, NULL);

	if ((BfsACLayer::is_owner(usr, par_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, par_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, par_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", par_ino_ptr, NULL);

	if (!BFS__S_ISDIR(par_ino_ptr->get_mode()))
		throw BfsServerError("Parent inode is not a dir\n", par_ino_ptr, NULL);

	// read the block where the dentry was found by get_de and clear the dentry
	data_blk_buf.set_vbid(de->get_blk_loc());
	read_blk(data_blk_buf);
	// data_blk_buf.resizeAllocation(NULL, BLK_SZ,
	// 							BFS_IV_LEN,
	// 							BFS_MAC_LEN);

	// now serialize an empty dentry and write it
	de->set_de_name("");
	de->set_ino(0);
	temp_de_len =
		de->serialize(data_blk_buf, DENTRY_ABSOLUTE_BLK_OFF(de->get_idx_loc()));
	assert(temp_de_len == DIRENT_SZ);

	// Note: try to pop from cache and delete if present then just delete the
	// object if it was cached (as retrieved from get_de above), this just
	// deletes the cached ptr, otherwise this just deletes the newly created ptr
	// if ((cached_de = read_dcache(stringCacheKey(path), true)))
	// 	delete cached_de;
	delete de;

	write_blk(data_blk_buf, _bfs__O_SYNC);

	// update the parent inode ilinks count
	par_ino_ptr->set_i_links(par_ino_ptr->get_i_links_count() - 1);
	if (write_inode(par_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS)
		throw BfsServerError("Failed to write updated parent inode\n",
							 par_ino_ptr, NULL);

	if (!par_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	return BFS_SUCCESS;
}

/**
 * @brief Rename a file. Modifies the associated dentry in the parent inode's
 * data blocks to reflect the new name of the file. Dentry cache is also
 * updated. Nothing else needs to be modified.
 *
 * @param fr_path the old name of the file
 * @param to_path the new name of the file
 * @return int32_t: BFS_SUCCESS if success, throws exception if failure
 */
int32_t BfsHandle::bfs_rename(BfsUserContext *usr, std::string fr_path,
							  std::string to_path) {
	DirEntry *de, *fr_par_de, *to_par_de;
	Inode *fr_par_ino_ptr, *to_par_ino_ptr, *fr_ino_ptr, *to_ino_ptr;
	char *fr_path_copy, *to_path_copy;
	std::string dirname;
	int32_t res;
	VBfsBlock data_blk_buf(NULL, BLK_SZ, 0, 0, 0);
	int64_t temp_de_len;
	bfs_ino_id_t new_i = 0;

	// check if file name length valid
	if ((fr_path.size() + 1) > MAX_FILE_NAME_LEN)
		throw BfsClientRequestFailedError("File name too long\n", NULL, NULL);

	/**
	 *  Delete the _fr_ dentry from the _fr_ parent. Then check if a file with
	 * the name _to_ path already exists. If so, do a inode number swap
	 * underneath. Otherwise, alloc a new dentry in the _to_ parent for the _fr_
	 * inode.
	 */

	/**
	 * Delete _fr_ dentry from the parent.
	 */
	// get dentry and inode of the _from_ parent directory, and do some checks
	fr_path_copy = bfs_strdup(fr_path.c_str());
	dirname = std::string(bfs_dirname(fr_path_copy));
	free(fr_path_copy);
	fr_par_de = NULL;

	if ((res = get_de(usr, &fr_par_de, dirname)) != BFS_SUCCESS)
		throw BfsServerError("Parent dir does not exist\n", NULL, NULL);

	if (!(fr_par_ino_ptr = read_inode(fr_par_de->get_ino())))
		throw BfsServerError("Parent inode does not exist\n", NULL, NULL);

	if (fr_par_de && !fr_par_de->unlock())
		throw BfsServerError("Failed releasing fr_par_de\n", NULL, NULL);

	if ((BfsACLayer::is_owner(usr, fr_par_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, fr_par_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, fr_par_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied on from source\n",
								   fr_par_ino_ptr, NULL);

	if (!BFS__S_ISDIR(fr_par_ino_ptr->get_mode()))
		throw BfsServerError("Parent inode is not a dir\n", fr_par_ino_ptr,
							 NULL);

	// try to read the _fr_ inode and get the number so we can do a dentry swap
	de = NULL;
	if ((res = get_de(usr, &de, fr_path, true)) != BFS_SUCCESS)
		throw BfsClientRequestFailedError("fr_path does not exist\n", NULL,
										  NULL);

	// get the inode for the _from_ dentry
	if (!(fr_ino_ptr = read_inode(de->get_ino(), true)))
		throw BfsServerError("Failed reading inode for dentry\n", NULL, NULL);

	// check if user has access permissions on the source file (ie can read it)
	if ((BfsACLayer::is_owner(usr, fr_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, fr_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, fr_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied on source file\n",
								   fr_ino_ptr, NULL);

	// TODO: only allowing overwrites from rename to be called on regular files
	// for now
	if (!BFS__S_ISREG(fr_ino_ptr->get_mode()))
		throw BfsClientRequestFailedError("Inode is not a regular file\n", NULL,
										  fr_ino_ptr);

	// save the _from_ inode num so we can swap in the _to_ dentry
	// new_i = de->get_ino();
	new_i = fr_ino_ptr->get_i_no(); // same as above

	// read the block where the _fr_ dentry was found in its parent by get_de
	// and clear the dentry
	data_blk_buf.set_vbid(de->get_blk_loc());
	read_blk(data_blk_buf);
	// data_blk_buf.resizeAllocation(NULL, BLK_SZ,
	// 							BFS_IV_LEN,
	// 							BFS_MAC_LEN);

	// now serialize an empty dentry and write it
	de->set_de_name("");
	de->set_ino(0);
	temp_de_len =
		de->serialize(data_blk_buf, DENTRY_ABSOLUTE_BLK_OFF(de->get_idx_loc()));
	assert(temp_de_len == DIRENT_SZ);

	// Note: try to pop from cache and delete if present then just delete the
	// object if it was cached (as retrieved from get_de above), this just
	// deletes the cached ptr, otherwise this just deletes the newly created ptr
	// if ((cached_de = read_dcache(stringCacheKey(path), true)))
	// 	delete cached_de;
	delete de;

	write_blk(data_blk_buf, _bfs__O_SYNC);

	// update the _fr_ parent inode ilinks count
	fr_par_ino_ptr->set_i_links(fr_par_ino_ptr->get_i_links_count() - 1);
	if (write_inode(fr_par_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS)
		throw BfsServerError("Failed to write updated parent inode\n",
							 fr_par_ino_ptr, NULL);

	if (!fr_par_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	// dont unlock yet so we can use below for adding a new dentry to the _to_
	// parent (ie to call add_dir_...) if (!fr_ino_ptr->unlock()) 	throw
	// BfsServerError("Failed releasing inode\n", NULL, NULL);

	// Note: the _fr_ de is deleted from the _fr_ parent, so no need to try
	// unlocking if (de && !de->unlock()) 	throw BfsServerError("Failed
	// releasing de (fr_path)\n", NULL, NULL);

	//////////////////////////////////////////////////////////////////////////////////////////

	/**
	 *  Check if a file with the name _to_ path already exists. If so, do a
	 * inode number swap underneath (and dealloc the _to_ inode). Otherwise,
	 * alloc a new dentry in the _to_ parent for the _fr_ inode.
	 *
	 * Note: dont delete the cache entry for the to_path (pop=false); just
	 * update the de as necessary
	 */
	de = NULL;
	if ((res = get_de(usr, &de, to_path)) != BFS_FAILURE) {
		if (!fr_ino_ptr->unlock())
			throw BfsServerError("Failed releasing inode\n", NULL, NULL);

		// get the inode for the dentry
		if (!(to_ino_ptr = read_inode(de->get_ino(), true)))
			throw BfsServerError("Failed reading inode for dentry\n", NULL,
								 NULL);

		// check if user has access permissions on the target file (ie can
		// overwrite it)
		if ((BfsACLayer::is_owner(usr, to_ino_ptr->get_uid()) &&
			 !BfsACLayer::owner_access_ok(usr, to_ino_ptr->get_mode())) ||
			!BfsACLayer::world_access_ok(usr, to_ino_ptr->get_mode()))
			throw BfsAccessDeniedError("Permission denied on target file\n",
									   to_ino_ptr, NULL);

		// dealloc the _to_ inode (to overwrite it)
		dealloc_ino(to_ino_ptr);

		if (!to_ino_ptr->unlock())
			throw BfsServerError("Failed releasing inode\n", NULL, NULL);

		// swap the old _to_ inode for the _from_ one and flush it
		de->set_ino(new_i);

		// Now serialize and write the _fr_ inode num to parent directory
		// entries read the block where the _to_ dentry was found by get_de and
		// update
		data_blk_buf.set_vbid(de->get_blk_loc());
		read_blk(data_blk_buf);
		// data_blk_buf.resizeAllocation(NULL, BLK_SZ,
		// 							BFS_IV_LEN,
		// 							BFS_MAC_LEN);

		// update the name, serialize, and write the dentry to update the parent
		// dir de->set_de_name(to_path);
		temp_de_len = de->serialize(data_blk_buf,
									DENTRY_ABSOLUTE_BLK_OFF(de->get_idx_loc()));
		assert(temp_de_len == DIRENT_SZ);

		// check and update de if in cache (the check in write cache is
		// redundant)
		// if ((cached_de = read_dcache(stringCacheKey(to_path), true)))
		// 	delete cached_de;
		// write_dcache(stringCacheKey(de->get_de_name()), de);

		write_blk(data_blk_buf, _bfs__O_SYNC);

		if (de && !de->unlock())
			throw BfsServerError("Failed releasing de (to_path)\n", NULL, NULL);

		// throw BfsClientRequestFailedError("File with specified name
		// exists\n", 								  NULL, NULL);
	} else {
		/**
		 * Otherwise the target file name doesnt exist. Just add a new dentry to
		 * the _to_ parent directory and point it at the _fr_ inode number.
		 */

		to_path_copy = bfs_strdup(to_path.c_str());
		dirname = std::string(bfs_dirname(to_path_copy));
		free(to_path_copy);
		to_par_de = NULL;

		if ((res = get_de(usr, &to_par_de, dirname)) != BFS_SUCCESS)
			throw BfsServerError("Parent dir does not exist\n", NULL, NULL);

		if (!(to_par_ino_ptr = read_inode(to_par_de->get_ino())))
			throw BfsServerError("Parent inode does not exist\n", NULL, NULL);

		if (to_par_de && !to_par_de->unlock())
			throw BfsServerError("Failed releasing to_par_de\n", NULL, NULL);

		if ((BfsACLayer::is_owner(usr, to_par_ino_ptr->get_uid()) &&
			 !BfsACLayer::owner_access_ok(usr, to_par_ino_ptr->get_mode())) ||
			!BfsACLayer::world_access_ok(usr, to_par_ino_ptr->get_mode()))
			throw BfsAccessDeniedError("Permission denied on from source\n",
									   to_par_ino_ptr, NULL);

		if (!BFS__S_ISDIR(to_par_ino_ptr->get_mode()))
			throw BfsServerError("Parent inode is not a dir\n", to_par_ino_ptr,
								 NULL);

		// pulled from bfs_create

		// Note: dont create new inode for rename
		// bfs_ino_id_t ino = 0;
		// if ((ino = alloc_ino()) < FIRST_UNRESERVED_INO)
		//     throw BfsClientRequestFailedError(
		//         "Failed allocating new inode: too many files\n",
		//         to_par_ino_ptr, NULL);
		// this method creates but does _not_ open so mark ref_cnt as 0
		// regular files dont have hard links in them so mark links as 0
		// new_ino_ptr =
		//     new Inode(ino, usr->get_uid(), BFS__S_IFREG | mode, 1, 0, 0, 0,
		//     0, 0);

		// first try to add to direct blocks, then try indirect blocks (let
		// short circuit return error)
		if ((add_dentry_to_direct_blks(to_par_ino_ptr, fr_ino_ptr, to_path) !=
			 BFS_SUCCESS) &&
			(add_dentry_to_indirect_blks(to_par_ino_ptr, fr_ino_ptr, to_path) !=
			 BFS_SUCCESS))
			throw BfsServerError(
				"Failed adding dentry to direct or indirect blocks\n",
				to_par_ino_ptr, fr_ino_ptr);

		// Note: dont open for rename
		// allocate an open file handle and mark as open
		// if ((fh = alloc_fd()) < START_FD)
		//     throw BfsServerError("Invalid file handle in create\n",
		//     to_par_ino_ptr,
		//                         new_ino_ptr);
		// of = new OpenFile(new_ino_ptr->get_i_no(), 0);
		// open_file_tab.insert(std::make_pair(fh, of));

		if (!fr_ino_ptr->unlock())
			throw BfsServerError("Failed releasing inode\n", NULL, NULL);

		if (!to_par_ino_ptr->unlock())
			throw BfsServerError("Failed releasing inode\n", NULL, NULL);

		// Note: dont write since we know the inode already existed
		// write the new inode (note that since we are creating a new inode, if
		// there is an error just manually unlock and then delete the inode)
		// if (write_inode(new_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS) {
		//     if (!new_ino_ptr->unlock())
		//         err_msg = "Failed writing inode and failed releasing
		//         inode\n";
		//     else
		//         err_msg = "Failed writing inode\n";
		//     delete new_ino_ptr;
		//     throw BfsServerError(err_msg, NULL, NULL);
		// }

		// if (!new_ino_ptr->unlock())
		//     throw BfsServerError("Failed releasing inode\n", NULL, NULL);
	}

	return BFS_SUCCESS;
}

uint32_t BfsHandle::prealloc_blks(Inode *new_ino_ptr) {
	// Prealloc some direct blocks
	bfs_vbid_t curr_blk_vbid;
	for (int ix = 0; ix < NUM_DIRECT_BLOCKS; ix++) {
		if (!(curr_blk_vbid = sb.alloc_blk()))
			throw BfsServerError(
				"Failed preallocating a new direct block vbid\n", NULL,
				new_ino_ptr);
		new_ino_ptr->set_i_blk(ix, curr_blk_vbid);
	}

	// Prealloc an indirect block
	VBfsBlock indir_data_blk_buf(NULL, BLK_SZ, 0, 0, 0);
	curr_blk_vbid = 0;
	if (!(curr_blk_vbid = sb.alloc_blk()))
		throw BfsServerError("Failed preallocating a new indirect block\n",
							 NULL, new_ino_ptr);
	new_ino_ptr->set_i_blk(NUM_DIRECT_BLOCKS, curr_blk_vbid);
	indir_data_blk_buf.set_vbid(curr_blk_vbid);

	// Prealloc some indirect data blocks
	IndirectBlock ib;
	for (uint32_t ix = 0; ix < NUM_BLKS_PER_IB; ix++) {
		if (!(curr_blk_vbid = sb.alloc_blk()))
			throw BfsServerError(
				"Failed preallocating a new indirect data block\n", NULL,
				new_ino_ptr);
		ib.set_indirect_loc(ix, curr_blk_vbid);
	}
	indir_data_blk_buf.resizeAllocation(0, BLK_SZ, 0);
	indir_data_blk_buf.burn();
	int64_t ib_len = ib.serialize(indir_data_blk_buf, 0);
	assert(ib_len <= BLK_SZ);

	// Finally write out the indirect block (then return to write out the inode)
	write_blk(indir_data_blk_buf, _bfs__O_SYNC);

	return BFS_SUCCESS;
}

/**
 * @brief Create and open a file. Doesn't handle recursive create for now.
 *
 * @param path: absolute file name
 * @param mode: file permission bits
 * @return bfs_fh_t: handle for the open file if success, throws exception if
 * failure
 */
bfs_fh_t BfsHandle::bfs_create(BfsUserContext *usr, std::string path,
							   uint32_t mode) {
	Inode *par_ino_ptr, *new_ino_ptr;
	OpenFile *of;
	bfs_ino_id_t ino;
	DirEntry *de;
	bfs_fh_t fh;
	int32_t res;
	std::string dirname, err_msg;
	char *path_copy;

	// check if file name length valid
	if ((path.size() + 1) > MAX_FILE_NAME_LEN)
		throw BfsClientRequestFailedError("File name too long\n", NULL, NULL);

	// get the dentry
	de = NULL;
	if ((res = get_de(usr, &de, path)) != BFS_FAILURE) {
		if (de && !de->unlock())
			throw BfsServerError("Failed releasing de\n", NULL, NULL);

		throw BfsClientRequestFailedError("File exists\n", NULL, NULL);
	}

	// get dentry and inode of the parent directory
	path_copy = bfs_strdup(path.c_str());
	dirname = std::string(bfs_dirname(path_copy));
	free(path_copy);
	de = NULL;

	if ((res = get_de(usr, &de, dirname)) != BFS_SUCCESS)
		throw BfsServerError("Parent dir does not exist\n", NULL, NULL);

	if (!(par_ino_ptr = read_inode(de->get_ino())))
		throw BfsServerError("Parent inode does not exist\n", NULL, NULL);

	if (de && !de->unlock())
		throw BfsServerError("Failed releasing de\n", NULL, NULL);

	if ((BfsACLayer::is_owner(usr, par_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, par_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, par_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", par_ino_ptr, NULL);

	if (!BFS__S_ISDIR(par_ino_ptr->get_mode()))
		throw BfsServerError("Parent inode is not a dir\n", par_ino_ptr, NULL);

	// create new inode, then write it to bdev
	if ((ino = alloc_ino()) < FIRST_UNRESERVED_INO)
		throw BfsClientRequestFailedError(
			"Failed allocating new inode: too many files\n", par_ino_ptr, NULL);

	// this method creates+opens so mark ref_cnt as 1
	// regular files dont have hard links in them so mark links as 0
	new_ino_ptr =
		new Inode(ino, usr->get_uid(), BFS__S_IFREG | mode, 1, 0, 0, 0, 0, 0);

	// first try to add to direct blocks, then try indirect blocks (let short
	// circuit return error)
	if ((add_dentry_to_direct_blks(par_ino_ptr, new_ino_ptr, path) !=
		 BFS_SUCCESS) &&
		(add_dentry_to_indirect_blks(par_ino_ptr, new_ino_ptr, path) !=
		 BFS_SUCCESS))
		throw BfsServerError(
			"Failed adding dentry to direct or indirect blocks\n", par_ino_ptr,
			new_ino_ptr);

	// allocate an open file handle and mark as open
	if ((fh = alloc_fd()) < START_FD)
		throw BfsServerError("Invalid file handle in create\n", par_ino_ptr,
							 new_ino_ptr);

	of = new OpenFile(new_ino_ptr->get_i_no(), 0);
	open_file_tab.insert(std::make_pair(fh, of));

	if (!par_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	// Prealloc some blocks for the inode (as an optimization to mitigate
	// indirect block allocation overhead).
	if (prealloc_blks(new_ino_ptr) != BFS_SUCCESS)
		throw BfsServerError("Failed preallocating blocks for inode\n", NULL,
							 NULL);

	// write the new inode (note that since we are creating a new inode, if
	// there is an error just manually unlock and then delete the inode)
	if (write_inode(new_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS) {
		if (!new_ino_ptr->unlock())
			err_msg = "Failed writing inode and failed releasing inode\n";
		else
			err_msg = "Failed writing inode\n";
		delete new_ino_ptr;
		throw BfsServerError(err_msg, NULL, NULL);
	}

	if (!new_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	return fh;
}

/**
 * @brief Change file permissions.
 *
 * @param path: absolute file name
 * @param new_mode: new file permission bits
 * @return bfs_fh_t: handle for the open file if success, throws exception if
 * failure
 */
bfs_fh_t BfsHandle::bfs_chmod(BfsUserContext *usr, std::string path,
							  uint32_t new_mode) {
	Inode *path_ino_ptr;
	DirEntry *de;
	uint32_t res;

	// check if file name length valid
	if ((path.size() + 1) > MAX_FILE_NAME_LEN)
		throw BfsClientRequestFailedError("File name too long\n", NULL, NULL);

	// get the dentry
	de = NULL;
	if ((res = get_de(usr, &de, path)) != BFS_SUCCESS) {
		logMessage(FS_LOG_LEVEL, "File does not exist: %s", path.c_str());
		throw BfsClientRequestFailedError("File does not exist\n", NULL, NULL);
	}

	// get the inode for the dentry
	if (!(path_ino_ptr = read_inode(de->get_ino())))
		throw BfsServerError("Inode does not exist\n", NULL, NULL);

	if (de && !de->unlock())
		throw BfsServerError("Failed releasing de\n", NULL, NULL);

	if ((BfsACLayer::is_owner(usr, path_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, path_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, path_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", NULL, path_ino_ptr);

	// update the file permissions
	path_ino_ptr->set_mode(new_mode);

	if (!path_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	return BFS_SUCCESS;
}

/**
 * @brief Open a file and return a file handle to the client.
 *
 * Process:
 * 1) get the parent dir name and the file name
 * 2) get the parent dir entry by scanning bdev
 * 3) get the file dir entry by scanning bdev
 * 4) read the inode for the file in the itable (search caches) and check type;
 * update ref cnt
 * 5) allocate open file in table and seek to correct position
 * 6) flush any dirty data (dont think there should be any here) nvfuse does
 * nvfuse_path_resolve first to get the parent dirent, then does nvfuse_openfile
 * to get the dirent for the actual file (should always exist for us in open,
 * otherwise return fail)
 *
 * @param path absolute file name
 * @return bfs_fh_t handle associated with the open file, throws exception on
 * failure
 */
bfs_fh_t BfsHandle::bfs_open(BfsUserContext *usr, std::string path,
							 uint32_t flags) {
	Inode *path_ino_ptr;
	OpenFile *of;
	DirEntry *de;
	bfs_fh_t fh;
	uint32_t res;
	uint64_t off;

	// check if file name length valid
	if ((path.size() + 1) > MAX_FILE_NAME_LEN)
		throw BfsClientRequestFailedError("File name too long\n", NULL, NULL);

	// get the dentry
	de = NULL;
	if ((res = get_de(usr, &de, path)) != BFS_SUCCESS) {
		logMessage(FS_LOG_LEVEL, "File does not exist: %s", path.c_str());
		throw BfsClientRequestFailedError("File does not exist\n", NULL, NULL);
	}

	// get the inode for the dentry
	if (!(path_ino_ptr = read_inode(de->get_ino())))
		throw BfsServerError("Inode does not exist\n", NULL, NULL);

	if (de && !de->unlock())
		throw BfsServerError("Failed releasing de\n", NULL, NULL);

	if ((BfsACLayer::is_owner(usr, path_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, path_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, path_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", NULL, path_ino_ptr);

	if (!BFS__S_ISREG(path_ino_ptr->get_mode()))
		throw BfsClientRequestFailedError("Inode is not a regular file\n", NULL,
										  path_ino_ptr);

	// allocate a file handle and open file object
	if ((fh = alloc_fd()) < START_FD)
		throw BfsServerError("Invalid file handle in open\n", NULL,
							 path_ino_ptr);

	off = 0;
	if (flags & _bfs__O_APPEND)
		off = path_ino_ptr->get_size();

	of = new OpenFile(path_ino_ptr->get_i_no(), off);

	path_ino_ptr->set_ref_cnt(path_ino_ptr->get_ref_cnt() + 1);
	if (write_inode(path_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS)
		throw BfsServerError("Failed to write updated inode\n", NULL, NULL);

	open_file_tab.insert(std::make_pair(fh, of));

	logMessage(FS_VRB_LOG_LEVEL, "opened regular file [%lu]\n", fh);

	if (!path_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	return fh;
}

/**
 * @brief Read from an open file for size bytes starting at the given offset.
 *
 * @param fh: the file handle associated with an open file to read from
 * @param buf: the read buffer to fill
 * @param size: the size of the read
 * @param off: the offset to begin the read
 * @return int32_t: number of bytes read if success, throws exception if failure
 */
uint64_t BfsHandle::bfs_read(BfsUserContext *usr, bfs_fh_t fh, char *buf,
							 uint64_t size, uint64_t off) {
	OpenFile *of;
	bfs_ino_id_t fino;
	Inode *path_ino_ptr;

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double bfs_read_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&bfs_read_start_time) != SGX_SUCCESS) ||
	// 			(bfs_read_start_time == -1))
	// 			return 0;
	// 		logMessage(FS_LOG_LEVEL, "===== Starting timer for bfs_read");
	// 	}
	// #else
	// 	double bfs_read_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((bfs_read_start_time = ocall_get_time2()) == -1)
	// 			return 0;
	// 		logMessage(FS_LOG_LEVEL, "===== Starting timer for bfs_read");
	// 	}
	// #endif

	// get the openfile object
	of = NULL;
	if (!(of = open_file_tab.at(fh)))
		throw BfsServerError("Error during bfs_read find openfile\n", NULL,
							 NULL);

	// get the inode number of the openfile
	fino = 0;
	if ((fino = of->get_ino()) < ROOT_INO)
		throw BfsServerError("Error during bfs_read get inode id\n", NULL,
							 NULL);

	// get the inode object
	path_ino_ptr = NULL;
	if (!(path_ino_ptr = read_inode(fino)))
		throw BfsServerError("Error during bfs_read read_inode\n", NULL, NULL);

	if ((BfsACLayer::is_owner(usr, path_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, path_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, path_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", NULL, path_ino_ptr);

	/**
	 * Read the data. The loop tries to read all size bytes, but breaks if the
	 * offset is equal to the length (could happen at a block boundary or in the
	 * middle of a block). At each iteration, it checks if the current block
	 * index indicated by the offset is in the direct or indirect data blocks.
	 * If the block is invalid (not allocated), then it reached EOF and breaks
	 * the loop. After finding the correct block to read from, it computes the
	 * number of bytes to read each iteration and performs the read.
	 */
	uint64_t num_read_bytes = size, curr_blk_read_sz = 0, max_curr_rd_sz = 0,
			 curr_file_off = off, curr_blk_pos = 0;
	bfs_vbid_t curr_blk_vbid = 0, curr_indir_blk_idx = 0,
			   curr_blk_idx = off / BLK_SZ; // relative to the file
	VBfsBlock data_blk_buf(NULL, BLK_SZ, 0, 0, 0),
		indir_data_blk_buf(NULL, BLK_SZ, 0, 0, 0);
	IndirectBlock ib;
	int64_t ib_len = 0;
	bool ib_read = false;

	// early return if the offset is too high
	if (curr_file_off > path_ino_ptr->get_size()) {
		logMessage(FS_LOG_LEVEL, "Offset [%lu] > size [%lu], exiting early\n",
				   curr_file_off, path_ino_ptr->get_size());
		if (!path_ino_ptr->unlock())
			throw BfsServerError("Failed releasing inode\n", NULL, NULL);
		return 0;
	}

	while (num_read_bytes > 0) {
		// break if we are at the end of the file
		if (curr_file_off >= path_ino_ptr->get_size())
			break;

		if (curr_blk_idx < NUM_DIRECT_BLOCKS) {
			if (path_ino_ptr->get_i_blks().at(curr_blk_idx) <
				DATA_REL_START_BLK_NUM)
				break;

			curr_blk_vbid = path_ino_ptr->get_i_blks().at(curr_blk_idx);
			curr_blk_idx++;
		} else {
			if (!ib_read) {
				assert(curr_file_off >= BLK_SZ * NUM_DIRECT_BLOCKS);
				indir_data_blk_buf.set_vbid(
					path_ino_ptr->get_i_blks().at(NUM_DIRECT_BLOCKS));
				read_blk(indir_data_blk_buf);
				ib_len = ib.deserialize(indir_data_blk_buf, 0);
				assert(ib_len <= BLK_SZ);

				curr_indir_blk_idx = (curr_blk_idx - NUM_DIRECT_BLOCKS);
				ib_read = true;
			}

			// file is max size (not a server error)
			if (curr_indir_blk_idx >= ib.get_indirect_locs().size()) {
				if (!path_ino_ptr->unlock())
					throw BfsServerError("Failed releasing inode\n", NULL,
										 NULL);
				return (size - num_read_bytes);
			}

			if (ib.get_indirect_locs().at(curr_indir_blk_idx) <
				DATA_REL_START_BLK_NUM)
				break;

			curr_blk_vbid = ib.get_indirect_locs().at(curr_indir_blk_idx);
			curr_indir_blk_idx++;
		}

		curr_blk_pos = curr_file_off % BLK_SZ;
		max_curr_rd_sz = num_read_bytes < (BLK_SZ - curr_blk_pos)
							 ? num_read_bytes
							 : (BLK_SZ - curr_blk_pos);

		// maybe read the rest of block (or rest of size bytes), or as much as
		// is left in the file
		if ((curr_file_off + max_curr_rd_sz) > path_ino_ptr->get_size())
			curr_blk_read_sz = path_ino_ptr->get_size() - curr_file_off;
		else
			curr_blk_read_sz = max_curr_rd_sz;

		data_blk_buf.set_vbid(curr_blk_vbid);
		read_blk(data_blk_buf);

		memcpy(&buf[size - num_read_bytes],
			   &(data_blk_buf.getBuffer()[curr_blk_pos]), curr_blk_read_sz);

		// now resize so we dont lose correct buffer start address (w/o memmove)
		data_blk_buf.resizeAllocation(0, BLK_SZ, 0);
		data_blk_buf.burn(); // clear contents from old block

		curr_file_off += curr_blk_read_sz;
		num_read_bytes -= curr_blk_read_sz;
	}

	// sanity check; if we ended up entering the loop above, then the initial
	// off was OK, and therefore we should always end at EOF at the max, which
	// implies should never be true (note: off may be == size though, indicating
	// that the off is at the end of the file); if we didnt enter loop, we
	// should have returned early; this is a server error
	if (curr_file_off > path_ino_ptr->get_size()) {
		logMessage(LOG_ERROR_LEVEL, "Offset [%lu] > size [%lu]\n",
				   curr_file_off, path_ino_ptr->get_size());
		throw BfsServerError("Offset > size, aborting\n", NULL, path_ino_ptr);
	}

	of->set_offset(curr_file_off);

	if (!path_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double bfs_read_end_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&bfs_read_end_time) != SGX_SUCCESS) ||
	// 			(bfs_read_end_time == -1))
	// 			return 0;
	// 		logMessage(FS_LOG_LEVEL, "===== Time in bfs_read total: %.3f us",
	// 				   bfs_read_end_time - bfs_read_start_time);
	// 	}
	// #else
	// 	double bfs_read_end_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((bfs_read_end_time = ocall_get_time2()) == -1)
	// 			return 0;
	// 		logMessage(FS_LOG_LEVEL, "===== Time in bfs_read total: %.3f us",
	// 				   bfs_read_end_time - bfs_read_start_time);
	// 	}
	// #endif

	return (size - num_read_bytes);
}

/**
 * @brief Write to an open file for size bytes starting at the given offset.
 *
 * @param fh: the file handle associated with an open file to write to
 * @param buf: the write contents
 * @param size: the size of the write
 * @param off: the offset to begin the write
 * @return int32_t: number of bytes written if success, throws exception if
 * failure
 */
uint64_t BfsHandle::bfs_write(BfsUserContext *usr, bfs_fh_t fh, char *buf,
							  uint64_t size, uint64_t off) {
	OpenFile *of;
	bfs_ino_id_t fino;
	Inode *path_ino_ptr;

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double bfs_write_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&bfs_write_start_time) != SGX_SUCCESS) ||
	// 			(bfs_write_start_time == -1))
	// 			return 0;
	// 		logMessage(FS_LOG_LEVEL, "===== Starting timer for bfs_write");
	// 	}
	// #else
	// 	double bfs_write_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((bfs_write_start_time = ocall_get_time2()) == -1)
	// 			return 0;
	// 		logMessage(FS_LOG_LEVEL, "===== Starting timer for bfs_write");
	// 	}
	// #endif

	// get the openfile object
	of = NULL;
	if (!(of = open_file_tab.at(fh)))
		throw BfsServerError("Error during bfs_write find openfile\n", NULL,
							 NULL);

	// get the inode number of the openfile
	fino = 0;
	if ((fino = of->get_ino()) < ROOT_INO)
		throw BfsServerError("Error during bfs_write get inode id\n", NULL,
							 NULL);

	// get the inode object
	path_ino_ptr = NULL;
	if (!(path_ino_ptr = read_inode(fino)))
		throw BfsServerError("Error during bfs_write read_inode\n", NULL, NULL);

	if ((BfsACLayer::is_owner(usr, path_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, path_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, path_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", NULL, path_ino_ptr);

	/**
	 * Write the data. The loop tries to write all size bytes. Stop immediately
	 * if the offset is bad (ie strictly greater than size). At each iteration,
	 * it checks if the current block index indicated by the offset is in the
	 * direct or indirect data blocks. Does allocation if the direct or indirect
	 * blocks do not exist (i.e., <DATA_REL_START_BLK_NUM). After finding the
	 * correct block to write to, it computes the number of bytes to write each
	 * iteration, reads a block if doing a partial write, then executes the
	 * write.
	 */
	uint64_t initial_size = size;
	uint64_t num_write_bytes = initial_size;
	uint64_t curr_blk_write_sz = 0;

	/**
	 * Special case we need to deal with because Make apparently depends on
	 * some arcane behavior permitted by POSIX. Need to see if we need to
	 * either (1) try to fill the gap created, or (2) when O_APPEND is used
	 * on the file descriptor, which I believe it is, then we should ignore
	 * the offset and execute the write at the current EOF.
	 *
	 * Edit: seems from FUSE logs that it in fact is not opened with O_APPEND:
	 *     create[8] flags: 0x8242 /bzip2/bzip2.o
	 *     ... where 0x8242 does not contain the O_APPEND flag
	 * so therefore we should not begin the write at the EOF but rather try to
	 * fill the hole created by the specified offset. We can do this efficiently
	 * by adjusting the size of the write to simply write additional bytes (some
	 * to fill the hole, and the rest to perform the requested client write).
	 */
	uint64_t curr_file_off = off;
	// uint64_t fill_bytes = 0;
	char *wbuf = buf;
	bool fill_hole = false;
	uint64_t hole_size;
	if (curr_file_off > path_ino_ptr->get_size()) {
		fill_hole = true;
		logMessage(FS_VRB_LOG_LEVEL,
				   "Trying to start write past EOF [off=%lu, file size=%lu, "
				   "write size=%lu]\n",
				   curr_file_off, path_ino_ptr->get_size(), size);
		// fill_bytes = path_ino_ptr->get_size() - curr_file_off;
		hole_size = curr_file_off - path_ino_ptr->get_size();
		initial_size += hole_size;
		num_write_bytes = initial_size;
		wbuf = (char *)malloc(num_write_bytes);
		memset(wbuf, 0x0, curr_file_off - path_ino_ptr->get_size());
		memcpy(&wbuf[curr_file_off - path_ino_ptr->get_size()], buf, size);
		curr_file_off = path_ino_ptr->get_size(); // now seek back to EOF
	}

	bfs_vbid_t curr_indir_blk_idx = 0, curr_blk_vbid = 0, new_blk = 0,
			   curr_blk_idx = curr_file_off / BLK_SZ; // relative to file
	uint64_t curr_blk_pos = 0;
	VBfsBlock data_blk_buf(NULL, BLK_SZ, 0, 0, 0),
		indir_data_blk_buf(NULL, BLK_SZ, 0, 0, 0);
	IndirectBlock ib;
	int64_t ib_len = 0;
	bool ib_read = false, using_new_blk = false, used_new_blocks = false;

	while (num_write_bytes > 0) {
		// In general if offset goes past EOF then we don't care, let it alloc
		// another block. This is more like a sanity check that the offset
		// should never be >=size+1, which would indicate some very bad server
		// error.
		// if (curr_file_off > path_ino_ptr->get_size())
		// 	break;

		if (curr_blk_idx < NUM_DIRECT_BLOCKS) {
			if (path_ino_ptr->get_i_blks().at(curr_blk_idx) <
				DATA_REL_START_BLK_NUM) {
				if (!(curr_blk_vbid = sb.alloc_blk()))
					throw BfsServerError(
						"Failed allocating a new direct block vbid\n", NULL,
						path_ino_ptr);

				using_new_blk = true;
				used_new_blocks = true;
				path_ino_ptr->set_i_blk(curr_blk_idx, curr_blk_vbid);
			} else {
				curr_blk_vbid = path_ino_ptr->get_i_blks().at(curr_blk_idx);
			}

			curr_blk_idx++;
		} else {
			if (!ib_read) {
				if (path_ino_ptr->get_i_blks().at(NUM_DIRECT_BLOCKS) <
					DATA_REL_START_BLK_NUM) {
					/* Allocate a new indirect block and save the loc in the
					 * inode. */
					if (!(new_blk = sb.alloc_blk()))
						throw BfsServerError(
							"Failed allocating a new indirect block\n", NULL,
							path_ino_ptr);

					// Mark this so we know to flush the inode before returning
					// (this will implicitly catch below because the new ib will
					// contain all 0s and a new ib loc will be allocated
					// anyway).
					using_new_blk = true;

					path_ino_ptr->set_i_blk(NUM_DIRECT_BLOCKS, new_blk);
					indir_data_blk_buf.set_vbid(new_blk);
					indir_data_blk_buf.resizeAllocation(
						0, BLK_SZ, 0); // resize for writing
					write_blk(indir_data_blk_buf,
							  _bfs__O_SYNC); // write the empty ib

					// // we know we allocated a new block so we are going to
					// // execute another write next, so resize for writing
					// indir_data_blk_buf.resizeAllocation(
					// 	BFS_IV_LEN, BLK_SZ,
					// 	BFS_MAC_LEN);
				} else { // otherwise it must already be allocated
					indir_data_blk_buf.set_vbid(
						path_ino_ptr->get_i_blks().at(NUM_DIRECT_BLOCKS));
					read_blk(indir_data_blk_buf);
					ib_len = ib.deserialize(indir_data_blk_buf, 0);
					assert(ib_len <= BLK_SZ);
				}

				curr_indir_blk_idx = (curr_blk_idx - NUM_DIRECT_BLOCKS);
				ib_read = true;
			}

			// file is max size (not a server error)
			if (curr_indir_blk_idx >= ib.get_indirect_locs().size()) {
				if (!path_ino_ptr->unlock())
					throw BfsServerError("Failed releasing inode\n", NULL,
										 NULL);
				return fill_hole ? (initial_size - hole_size - num_write_bytes)
								 : (initial_size - num_write_bytes);
			}

			if (ib.get_indirect_locs().at(curr_indir_blk_idx) <
				DATA_REL_START_BLK_NUM) {
				/* Allocate a new block. Here we are updating the ib rather than
				 * the inode, so need to flush. */
				if (!(curr_blk_vbid = sb.alloc_blk()))
					throw BfsServerError(
						"Failed allocating a new indirect block vbid\n", NULL,
						path_ino_ptr);

				// resize before serialization since we removed memmove
				// (otherwise the buffer data start address would change and the
				// write would be incorrect)
				indir_data_blk_buf.resizeAllocation(0, BLK_SZ, 0);
				// clear contents from old indirect block (indirect or indirect
				// data block)
				indir_data_blk_buf.burn();

				using_new_blk = true;
				used_new_blocks = true;
				ib.set_indirect_loc(curr_indir_blk_idx, curr_blk_vbid);
				ib_len = ib.serialize(indir_data_blk_buf, 0);
				assert(ib_len <= BLK_SZ);

				// we know we are not going to read, so just write then resize
				// the "data_blk_buf" for writing
				write_blk(indir_data_blk_buf, _bfs__O_SYNC);
				data_blk_buf.resizeAllocation(0, BLK_SZ, 0);
			} else {
				curr_blk_vbid = ib.get_indirect_locs().at(curr_indir_blk_idx);

				// we know we are going to read and data_blk_buf is sized
				// appropriately
			}

			curr_indir_blk_idx++;
		}

		curr_blk_pos = curr_file_off % BLK_SZ;
		if (num_write_bytes >= (BLK_SZ - curr_blk_pos))
			curr_blk_write_sz =
				(BLK_SZ - curr_blk_pos); // overwrite rest of block
		else
			curr_blk_write_sz = num_write_bytes; // overwrite part of the block

		// check if we need to read first
		data_blk_buf.burn(); // clear old contents before doing the block update
		data_blk_buf.set_vbid(curr_blk_vbid);
		if (!using_new_blk && (curr_blk_write_sz < BLK_SZ)) {
			read_blk(data_blk_buf);
		} else {
			memset(data_blk_buf.getBuffer(), 0x0, BLK_SZ);

			data_blk_buf.resizeAllocation(0, BLK_SZ, 0); // resize for writing
		}

		// either pad the hole with zeros or do an actual write
		// TODO: finish splicing in pad bytes and actual bytes
		// did_fill = false;
		// if (fill_bytes > 0) {
		// 	did_fill = true;
		// 	// if the last fill bytes dont fit to the end of the block, splice
		// 	if (fill_bytes < curr_blk_write_sz) {
		// 		memset(&(data_blk_buf.getBuffer()[curr_blk_pos]), 0x0,
		// 			   fill_bytes);
		// 		memcpy(&(data_blk_buf.getBuffer()[curr_blk_pos + fill_bytes]),
		// 			   &wbuf[initial_size - num_write_bytes],
		// 			   curr_blk_write_sz - fill_bytes);
		// 	} else {
		// 		memset(&(data_blk_buf.getBuffer()[curr_blk_pos]), 0x0,
		// 			   fill_bytes);
		// 	}
		// } else {
		memcpy(&(data_blk_buf.getBuffer()[curr_blk_pos]),
			   &wbuf[initial_size - num_write_bytes], curr_blk_write_sz);
		// }

		write_blk(data_blk_buf, _bfs__O_SYNC);
		data_blk_buf.resizeAllocation(0, BLK_SZ, 0);

		curr_file_off += curr_blk_write_sz;
		num_write_bytes -= curr_blk_write_sz;

		// if (using_new_blk || (curr_file_off > path_ino_ptr->get_size())) {
		// 	path_ino_ptr->set_size(curr_file_off);
		// 	if (write_inode(path_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS)
		// 		throw BfsServerError("Failed to write updated inode\n", NULL,
		// 							 NULL);
		// }

		using_new_blk = false;
	}

	of->set_offset(curr_file_off);

	// then write inode if size changed
	if (used_new_blocks || (curr_file_off > path_ino_ptr->get_size())) {
		path_ino_ptr->set_size(curr_file_off);
		if (write_inode(path_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS)
			throw BfsServerError("Failed to write updated inode\n", NULL, NULL);
	}

	if (!path_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double bfs_write_end_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&bfs_write_end_time) != SGX_SUCCESS) ||
	// 			(bfs_write_end_time == -1))
	// 			return 0;
	// 		logMessage(FS_LOG_LEVEL, "===== Time in bfs_write total: %.3f us",
	// 				   bfs_write_end_time - bfs_write_start_time);
	// 	}
	// #else
	// 	double bfs_write_end_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((bfs_write_end_time = ocall_get_time2()) == -1)
	// 			return 0;
	// 		logMessage(FS_LOG_LEVEL, "===== Time in bfs_write total: %.3f us",
	// 				   bfs_write_end_time - bfs_write_start_time);
	// 	}
	// #endif

	return fill_hole ? (initial_size - hole_size - num_write_bytes)
					 : (initial_size - num_write_bytes);
}

/**
 * @brief Synchronize a file's in-core state with storage device. Does not
 * support directory-type inodes. If the datasync parameter is non-zero, then
 * only the user data should be flushed, not the meta data.
 *
 * @param fh: the file handle associated with the file to synchronize
 * @param datasync: a flag indicating whether or not to synchronize data
 * @return int32_t: BFS_SUCCESS if success, throws exception if failure
 */
int32_t BfsHandle::bfs_fsync(BfsUserContext *usr, bfs_fh_t fh,
							 uint32_t datasync) {
	(void)datasync;
	OpenFile *of;
	bfs_ino_id_t fino;
	Inode *path_ino_ptr;

	// get the openfile object
	if (!(of = open_file_tab.at(fh)))
		throw BfsServerError("Error during bfs_fsync find openfile\n", NULL,
							 NULL);

	// get the inode of the openfile
	if ((fino = of->get_ino()) < ROOT_INO)
		throw BfsServerError("Error during bfs_fsync get inode id\n", NULL,
							 NULL);

	// get the inode object
	if (!(path_ino_ptr = read_inode(fino)))
		throw BfsServerError("Error during bfs_fsync read_inode\n", NULL, NULL);

	if ((BfsACLayer::is_owner(usr, path_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, path_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, path_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", NULL, path_ino_ptr);

	// flush the inode object to the itable
	if (write_inode(path_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS)
		throw BfsServerError("Error during write inode in bfs_fsync\n", NULL,
							 path_ino_ptr);

	// TODO: then flush the data buffers (similar code to read/write)

	if (!path_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	return BFS_SUCCESS;
}

/**
 * @brief Release a file handle associated with an open file/dir. Should be fine
 * to route both file and directory releases to this method.
 *
 * @param fh the file handle to close
 * @return int32_t BFS_SUCCESS if success, throws exception if failure
 */
int32_t BfsHandle::bfs_release(BfsUserContext *usr, bfs_fh_t fh) {
	OpenFile *of;
	bfs_ino_id_t fino;
	Inode *path_ino_ptr;

	logMessage(FS_VRB_LOG_LEVEL, "trying to close [%lu]\n", fh);
	logMessage(FS_VRB_LOG_LEVEL, "open_file_tab size: %d\n",
			   open_file_tab.size());
	logMessage(FS_VRB_LOG_LEVEL, "#inodes: num=%lu, free=%lu\n",
			   sb.get_no_inodes(), sb.get_no_inodes_free());

	// get the openfile object
	if (!(of = open_file_tab.at(fh)))
		throw BfsServerError("Error during bfs_release find openfile\n", NULL,
							 NULL);

	// get the inode number of the openfile
	if ((fino = of->get_ino()) < ROOT_INO)
		throw BfsServerError("Error during bfs_release get inode id\n", NULL,
							 NULL);

	// get the inode object
	if (!(path_ino_ptr = read_inode(fino)))
		throw BfsServerError("Error during bfs_release read_inode\n", NULL,
							 NULL);

	if ((BfsACLayer::is_owner(usr, path_ino_ptr->get_uid()) &&
		 !BfsACLayer::owner_access_ok(usr, path_ino_ptr->get_mode())) ||
		!BfsACLayer::world_access_ok(usr, path_ino_ptr->get_mode()))
		throw BfsAccessDeniedError("Permission denied\n", NULL, path_ino_ptr);

	// update ref counts to this inode to close the file
	path_ino_ptr->set_ref_cnt(path_ino_ptr->get_ref_cnt() - 1);
	if (write_inode(path_ino_ptr, _bfs__O_SYNC) != BFS_SUCCESS)
		throw BfsServerError("Failed to write updated inode\n", NULL, NULL);

	// delete the entry from the open file table
	if (open_file_tab.erase(fh) != 1)
		throw BfsServerError("Error during bfs_release erase openfile\n", NULL,
							 path_ino_ptr);

	delete of;

	if (!path_ino_ptr->unlock())
		throw BfsServerError("Failed releasing inode\n", NULL, NULL);

	return BFS_SUCCESS;
}

/**
 * SuperBlock definitions
 */

/**
 * @brief Initializes a superblock object.
 */
SuperBlock::SuperBlock() {
	magic = 0;

	blk_sz = 0;
	ino_sz = 0;
	no_blocks = 0;
	no_inodes = 0;
	no_dblocks_free = 0;
	no_inodes_free = 0;
	first_data_blk_loc = 0;
	next_vbid = 0;

	root_ino = 0;
	ibm_ino = 0;
	itab_ino = 0;
	journal_ino = 0;
	first_unresv_ino = 0;

	state = 0;
	dirty = true;
};

/**
 * @brief Clean up superblock object.
 */
SuperBlock::~SuperBlock() {
	magic = 0;

	blk_sz = 0;
	ino_sz = 0;
	no_blocks = 0;
	no_inodes = 0;
	no_dblocks_free = 0;
	no_inodes_free = 0;
	first_data_blk_loc = 0;
	next_vbid = 0;

	root_ino = 0;
	ibm_ino = 0;
	itab_ino = 0;
	journal_ino = 0;
	first_unresv_ino = 0;

	state = 0;
	dirty = false;
};

/**
 * @brief Gets the root inode number for the file system (see: ROOT_INO macro).
 *
 * @return bfs_ino_id_t: the root inode number
 */
bfs_ino_id_t SuperBlock::get_root_ino() { return root_ino; }

/**
 * @brief Gets the current number of free inodes in the file system.
 *
 * @return bfs_ino_id_t: the number of free inodes.
 */
bfs_ino_id_t SuperBlock::get_no_inodes_free() { return no_inodes_free; }

/**
 * @brief Gets the total number of available inodes in the file system.
 *
 * @return bfs_ino_id_t: the total number of inodes.
 */
bfs_ino_id_t SuperBlock::get_no_inodes() { return no_inodes; }

/**
 * @brief Save the magic number to identify the start of a bfs partition. Then
 * mark the structure as dirty (needs sync with bdev).
 *
 * @param m: value to set as the magic number
 */
void SuperBlock::set_magic(uint64_t m) {
	magic = m;
	dirty = true;
}

/**
 * @brief Save the core superblock parameters into the structure. Then
 * mark the structure as dirty (needs sync with bdev).
 *
 * @param a: block size for the file system
 * @param b: inode size (in bytes)
 * @param c: total number of blocks
 * @param d: total number of data blocks
 * @param e: total number of inodes
 * @param i: total number of free data blocks
 * @param j: totala number of free inodes
 * @param k: first available data block location (after reserved blocks)
 */
void SuperBlock::set_sb_params(uint32_t a, uint32_t b, bfs_vbid_t c,
							   bfs_vbid_t d, bfs_ino_id_t e, bfs_vbid_t i,
							   bfs_ino_id_t j, bfs_vbid_t k) {
	blk_sz = a;
	ino_sz = b;
	no_blocks = c;
	no_dblocks = d;
	no_inodes = e;
	no_dblocks_free = i;
	no_inodes_free = j;
	first_data_blk_loc = k;
	next_vbid = k; // set to the first (virtual) data blk location
	dirty = true;
}

/**
 * @brief Set the number of free inodes.
 *
 * @param f: the number of free inodes
 */
void SuperBlock::set_no_inodes_free(bfs_ino_id_t f) {
	no_inodes_free = f;
	dirty = true;
}

/**
 * @brief Set the special inode numbers for the file system. Then
 * mark the structure as dirty (needs sync with bdev).
 *
 * @param a: the root inode number
 * @param c: the ibitmap inode number
 * @param d: the itable inode number
 * @param e: the journal inode number
 * @param f: the first unreserved inode number
 */
void SuperBlock::set_reserved_inos(bfs_ino_id_t a, bfs_ino_id_t c,
								   bfs_ino_id_t d, bfs_ino_id_t e,
								   bfs_ino_id_t f) {
	root_ino = a;
	ibm_ino = c;
	itab_ino = d;
	journal_ino = e;
	first_unresv_ino = f;
	dirty = true;
}

/**
 * @brief Set the current statue of the system (same as BfsHandle::status). Then
 * mark the structure as dirty (needs sync with bdev).
 *
 * @param s: current state
 */
void SuperBlock::set_state(uint32_t s) {
	state = s;
	dirty = true;
}

/**
 * @brief Set the dirty status of the superblock.
 *
 * @param d: status flag
 */
void SuperBlock::set_dirty(bool d) { dirty = d; }

/**
 * @brief Check if the superblock is dirty and needs to be sync'd with the bdev.
 *
 * @return bool: true if dirty, false if not
 */
bool SuperBlock::is_dirty() { return dirty; }

/**
 * @brief Serialize the superblock into an on-bdev format beginning at the
 * offset off_start in the block buffer.
 *
 * @param b: block to copy to
 * @param off_start: offset to start copying to
 * @return uint64_t: number of bytes copied
 */
int64_t SuperBlock::serialize(VBfsBlock &b, uint64_t off_start) {
	uint64_t off = off_start;

	memcpy(&(b.getBuffer()[off]), &magic, sizeof(magic));
	off += sizeof(magic);

	memcpy(&(b.getBuffer()[off]), &blk_sz, sizeof(blk_sz));
	off += sizeof(blk_sz);

	memcpy(&(b.getBuffer()[off]), &ino_sz, sizeof(ino_sz));
	off += sizeof(ino_sz);

	memcpy(&(b.getBuffer()[off]), &no_blocks, sizeof(no_blocks));
	off += sizeof(no_blocks);

	memcpy(&(b.getBuffer()[off]), &no_dblocks, sizeof(no_dblocks));
	off += sizeof(no_dblocks);

	memcpy(&(b.getBuffer()[off]), &no_inodes, sizeof(no_inodes));
	off += sizeof(no_inodes);

	memcpy(&(b.getBuffer()[off]), &no_dblocks_free, sizeof(no_dblocks_free));
	off += sizeof(no_dblocks_free);

	memcpy(&(b.getBuffer()[off]), &no_inodes_free, sizeof(no_inodes_free));
	off += sizeof(no_inodes_free);

	memcpy(&(b.getBuffer()[off]), &first_data_blk_loc,
		   sizeof(first_data_blk_loc));
	off += sizeof(first_data_blk_loc);

	memcpy(&(b.getBuffer()[off]), &next_vbid, sizeof(next_vbid));
	off += sizeof(next_vbid);

	memcpy(&(b.getBuffer()[off]), &root_ino, sizeof(root_ino));
	off += sizeof(root_ino);

	memcpy(&(b.getBuffer()[off]), &ibm_ino, sizeof(ibm_ino));
	off += sizeof(ibm_ino);

	memcpy(&(b.getBuffer()[off]), &itab_ino, sizeof(itab_ino));
	off += sizeof(itab_ino);

	memcpy(&(b.getBuffer()[off]), &journal_ino, sizeof(journal_ino));
	off += sizeof(journal_ino);

	memcpy(&(b.getBuffer()[off]), &first_unresv_ino, sizeof(first_unresv_ino));
	off += sizeof(first_unresv_ino);

	memcpy(&(b.getBuffer()[off]), &state, sizeof(state));
	off += sizeof(state);

	return (off - off_start);
}

/**
 * @brief Deserialize into an in-memory format beginning at the offset
 * off_start in the block buffer.
 *
 * @param b: block to copy from
 * @param off_start: offset to start copying from
 * @return uint64_t: number of bytes copied
 */
int64_t SuperBlock::deserialize(VBfsBlock &b, uint64_t off_start) {
	uint64_t off = off_start;

	memcpy(&magic, &(b.getBuffer()[off]), sizeof(magic));
	off += sizeof(magic);

	memcpy(&blk_sz, &(b.getBuffer()[off]), sizeof(blk_sz));
	off += sizeof(blk_sz);

	memcpy(&ino_sz, &(b.getBuffer()[off]), sizeof(ino_sz));
	off += sizeof(ino_sz);

	memcpy(&no_blocks, &(b.getBuffer()[off]), sizeof(no_blocks));
	off += sizeof(no_blocks);

	memcpy(&no_dblocks, &(b.getBuffer()[off]), sizeof(no_dblocks));
	off += sizeof(no_dblocks);

	memcpy(&no_inodes, &(b.getBuffer()[off]), sizeof(no_inodes));
	off += sizeof(no_inodes);

	memcpy(&no_dblocks_free, &(b.getBuffer()[off]), sizeof(no_dblocks_free));
	off += sizeof(no_dblocks_free);

	memcpy(&no_inodes_free, &(b.getBuffer()[off]), sizeof(no_inodes_free));
	off += sizeof(no_inodes_free);

	memcpy(&first_data_blk_loc, &(b.getBuffer()[off]),
		   sizeof(first_data_blk_loc));
	off += sizeof(first_data_blk_loc);

	memcpy(&next_vbid, &(b.getBuffer()[off]), sizeof(next_vbid));
	off += sizeof(next_vbid);

	memcpy(&root_ino, &(b.getBuffer()[off]), sizeof(root_ino));
	off += sizeof(root_ino);

	memcpy(&ibm_ino, &(b.getBuffer()[off]), sizeof(ibm_ino));
	off += sizeof(ibm_ino);

	memcpy(&itab_ino, &(b.getBuffer()[off]), sizeof(itab_ino));
	off += sizeof(itab_ino);

	memcpy(&journal_ino, &(b.getBuffer()[off]), sizeof(journal_ino));
	off += sizeof(journal_ino);

	memcpy(&first_unresv_ino, &(b.getBuffer()[off]), sizeof(first_unresv_ino));
	off += sizeof(first_unresv_ino);

	memcpy(&state, &(b.getBuffer()[off]), sizeof(state));
	off += sizeof(state);

	dirty = false;

	return (off - off_start);
}

/**
 * @brief Allocate the next available block id. Note that the (virtual) block
 * ids are allocated using a monotonic counter.
 *
 * @return int32_t: the allocated block id if success, 0 if failure
 */
bfs_vbid_t SuperBlock::alloc_blk() {
	/**
	 * Dont let the number of blocks allocated go past the number available;
	 * this lets us track when we run out of space since we are not using a
	 * bitmap for data blocks.
	 */
	if (no_dblocks_free == 0)
		return 0;

	no_dblocks_free--;

	return next_vbid++;
}

/**
 * @brief Release ownership of a device block so that is is free to be
 * allocated for something else. Note: do not need to change next_vbid, since it
 * is a monotonic counter used for block id allocation at the fs layer.
 *
 * @param b: the block to free
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int32_t SuperBlock::dealloc_blk(bfs_vbid_t b) {
	/**
	 * Should never deallocate vbids <= DATA_REL_START_BLK_NUM (reserved,
	 * including the root inodes iblocks for default directories), and don't let
	 * the number of blocks unallocated go past the number available.
	 */
	if ((b <= DATA_REL_START_BLK_NUM) || (no_dblocks_free == no_dblocks))
		return BFS_FAILURE;

	// TODO: kind of redundant to catch and rethrow, but leave for now
	try {
		if (bfsBlockLayer::deallocBlock(b) != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL, "Failed to deallocate block\n");
			return BFS_FAILURE;
		}
	} catch (bfsBlockError *err) {
		logMessage(LOG_ERROR_LEVEL, "%s", err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	}

	no_dblocks_free++;

	return BFS_SUCCESS;
}

/**
 * IBitMap definitions
 */

/**
 * @brief Initializes an in-memory representation of an inode bitmap structure.
 */
IBitMap::IBitMap() {}

/**
 * @brief Cleans up the inode bitmap object by deleting all the virtual block
 * object pointers.
 */
IBitMap::~IBitMap() {
	// for (VBfsBlock *b : ibm_blks)
	// 	delete b;
}

/**
 * @brief Gets the list of inode bitmap blocks for the file system.
 *
 * @return std::vector<char *>: the list of ibitmap blocks
 */
std::vector<VBfsBlock *> &IBitMap::get_ibm_blks() { return ibm_blks; }

/**
 * @brief Append a new block to the in-memory list of ibitmap blocks.
 *
 * @param b: block to append
 */
void IBitMap::append_ibm_blk(VBfsBlock *b) { ibm_blks.push_back(b); }

/**
 * @brief Sets the specific bit number to _one_ in the bitmap block, to indicate
 * that the associated inode number is allocated.
 *
 * TODO: add bounds checking
 *
 * @param b: the bit to set
 */
void IBitMap::set_bit(bfs_ino_id_t b) {
	bfs_set_bit(b % BLK_SZ_BITS, ibm_blks[b / BLK_SZ_BITS]->getBuffer());
}

/**
 * @brief Zero (clear) a bit in the bitmap block to indicate the inode is free.
 *
 * TODO: add bounds checking
 *
 * @param b: the bit to clear
 */
void IBitMap::clear_bit(bfs_ino_id_t b) {
	bfs_clear_bit(b % BLK_SZ_BITS, ibm_blks[b / BLK_SZ_BITS]->getBuffer());
}

/**
 * Inode definitions
 */

/**
 * @brief Initializes an in-memory representation of an inode structure. Then
 * allocates NUM_INODE_IBLKS i_blk elements for the inode and zeros them out.
 *
 * @param _i_no: inode number
 * @param _uid: owner user id
 * @param _mode: permission bits of inode
 * @param _ref_cnt: number of references to the inode
 * @param _atime: last access time
 * @param _mtime: last modified time
 * @param _ctime: creation time
 * @param _size: current size of inode
 * @param _i_links_count: number of hard links in the inode
 */
Inode::Inode(bfs_ino_id_t _i_no, bfs_uid_t _uid, uint32_t _mode,
			 uint64_t _ref_cnt, uint64_t _atime, uint64_t _mtime,
			 uint64_t _ctime, uint64_t _size, uint64_t _i_links_count)
	: CacheableObject() {
	i_no = _i_no;
	uid = _uid;
	mode = _mode;
	ref_cnt = _ref_cnt;
	atime = _atime;
	mtime = _mtime;
	ctime = _ctime;
	size = _size;
	i_links_count = _i_links_count;
	i_blks.resize(NUM_INODE_IBLKS);
	std::fill(i_blks.begin(), i_blks.end(), 0);
}

/**
 * @brief Reset inode structure contents so that it can be repurposed.
 */
void Inode::clear() {
	i_no = 0;
	mode = 0;
	ref_cnt = 0;
	atime = 0;
	mtime = 0;
	ctime = 0;
	size = 0;
	i_links_count = 0;
	i_blks.resize(NUM_INODE_IBLKS);
	std::fill(i_blks.begin(), i_blks.end(), 0);
}

/**
 * @brief Get the inode number.
 *
 * @return bfs_ino_id_t: inode number
 */
bfs_ino_id_t Inode::get_i_no() { return i_no; }

/**
 * @brief Get the owner user id.
 *
 * @return bfs_uid_t: id of the owner
 */
bfs_uid_t Inode::get_uid() { return uid; }

/**
 * @brief Get the current permission bits of the inode.
 *
 * @return uint32_t: integer representation of the rwx permissions
 */
uint32_t Inode::get_mode() { return mode; }

/**
 * @brief Get the current number of references to the inode.
 *
 * @return uint64_t: reference count
 */
uint64_t Inode::get_ref_cnt() { return ref_cnt; }

/**
 * @brief Get the last access time on the inode.
 *
 * @return uint64_t: access time
 */
uint64_t Inode::get_atime() { return atime; }

/**
 * @brief Get the last modified time on the inode.
 *
 * @return uint64_t: modified time
 */
uint64_t Inode::get_mtime() { return mtime; }

/**
 * @brief Get the creation time on the inode.
 *
 * @return uint64_t: creation time
 */
uint64_t Inode::get_ctime() { return ctime; }

/**
 * @brief Get the current size of the inode (in bytes).
 *
 * @return uint64_t: size of inode
 */
uint64_t Inode::get_size() { return size; }

/**
 * @brief Get the current number of hard links in the inode (only for directory
 * files; it is the number of child inodes).
 *
 * @return uint64_t: number of hard links
 */
uint64_t Inode::get_i_links_count() { return i_links_count; }

/**
 * @brief Get the list of i_blks for the inode.
 *
 * @return const std::vector<bfs_vbid_t>&: reference to the iblks list
 */
const std::vector<bfs_vbid_t> &Inode::get_i_blks() { return i_blks; }

/**
 * @brief Set the inode number
 *
 * @param i: inode number
 */
void Inode::set_i_no(bfs_ino_id_t i) {
	i_no = i;
	dirty = true;
}

/**
 * @brief Set the owner user id of the inode.
 *
 * @param u: user id to set as owner
 */
void Inode::set_uid(bfs_uid_t u) {
	uid = u;
	dirty = true;
}

/**
 * @brief Set the permission bits of the inode.
 *
 * @param m: new mode
 */
void Inode::set_mode(uint32_t m) {
	mode = m;
	dirty = true;
}

/**
 * @brief Set the number of references to the inode.
 *
 * @param rc: current reference count
 */
void Inode::set_ref_cnt(uint64_t rc) {
	ref_cnt = rc;
	dirty = true;
}

/**
 * @brief Set the last access time on the inode.
 *
 * @param a: access time
 */
void Inode::set_atime(uint64_t a) {
	atime = a;
	dirty = true;
}

/**
 * @brief Set the last modified time on the inode.
 *
 * @param m: modified time
 */
void Inode::set_mtime(uint64_t m) {
	mtime = m;
	dirty = true;
}

/**
 * @brief Set the creation time of the inode.
 *
 * @param c: creation time
 */
void Inode::set_ctime(uint64_t c) {
	ctime = c;
	dirty = true;
}

/**
 * @brief Set the current size of the inode.
 *
 * @param s: inode size
 */
void Inode::set_size(uint64_t s) {
	size = s;
	dirty = true;
}

/**
 * @brief Set the current number of hard links that the inode's data blocks
 * contain (number of child files).
 *
 * @param l: number of hard links
 */
void Inode::set_i_links(uint64_t l) {
	i_links_count = l;
	dirty = true;
}

/**
 * @brief Set a value (newly allocated block id) in the inode's i_blks.
 *
 * @param idx: i_blk index to set
 * @param vbid: the id of the block
 */
void Inode::set_i_blk(uint64_t idx, bfs_vbid_t vbid) {
	i_blks.at(idx) = vbid;
	dirty = true;
}

/**
 * @brief Serialize into an on-bdev format beginning at the offset
 * off_start. Copies all of the direct block ids to the block from the
 * vector (including 0s to indicate unused ids).
 *
 * @param b: block to copy to
 * @param off_start: offset to start copying to
 * @return uint64_t: number of bytes copied
 */
int64_t Inode::serialize(VBfsBlock &b, uint64_t off_start) {
	uint64_t off = off_start;

	memcpy(&(b.getBuffer()[off]), &i_no, sizeof(i_no));
	off += sizeof(i_no);

	memcpy(&(b.getBuffer()[off]), &uid, sizeof(uid));
	off += sizeof(uid);

	memcpy(&(b.getBuffer()[off]), &mode, sizeof(mode));
	off += sizeof(mode);

	memcpy(&(b.getBuffer()[off]), &ref_cnt, sizeof(ref_cnt));
	off += sizeof(ref_cnt);

	memcpy(&(b.getBuffer()[off]), &atime, sizeof(atime));
	off += sizeof(atime);

	memcpy(&(b.getBuffer()[off]), &mtime, sizeof(mtime));
	off += sizeof(mtime);

	memcpy(&(b.getBuffer()[off]), &ctime, sizeof(ctime));
	off += sizeof(ctime);

	memcpy(&(b.getBuffer()[off]), &size, sizeof(size));
	off += sizeof(size);

	memcpy(&(b.getBuffer()[off]), &i_links_count, sizeof(i_links_count));
	off += sizeof(i_links_count);

	assert(i_blks.size() == NUM_INODE_IBLKS);
	for (uint32_t ix = 0; ix < NUM_DIRECT_BLOCKS; ix++) {
		memcpy(&(b.getBuffer()[off]), &i_blks.at(ix), sizeof(bfs_vbid_t));
		off += sizeof(bfs_vbid_t);
	}

	memcpy(&(b.getBuffer()[off]), &i_blks.at(NUM_DIRECT_BLOCKS),
		   sizeof(bfs_vbid_t));
	off += sizeof(bfs_vbid_t);

	return (off - off_start);
}

/**
 * @brief Deserialize into an in-memory format beginning at the offset
 * off_start. Copies all of the direct block ids from the block into the
 * vector (including 0s to indicate unused ids).
 *
 * @param b: block to copy from
 * @param off_start: offset to start copying from
 * @return uint64_t: number of bytes copied
 */
int64_t Inode::deserialize(VBfsBlock &b, uint64_t off_start) {
	uint64_t off = off_start;

	memcpy(&i_no, &(b.getBuffer()[off]), sizeof(i_no));
	off += sizeof(i_no);

	memcpy(&uid, &(b.getBuffer()[off]), sizeof(uid));
	off += sizeof(uid);

	memcpy(&mode, &(b.getBuffer()[off]), sizeof(mode));
	off += sizeof(mode);

	memcpy(&ref_cnt, &(b.getBuffer()[off]), sizeof(ref_cnt));
	off += sizeof(ref_cnt);

	memcpy(&atime, &(b.getBuffer()[off]), sizeof(atime));
	off += sizeof(atime);

	memcpy(&mtime, &(b.getBuffer()[off]), sizeof(mtime));
	off += sizeof(mtime);

	memcpy(&ctime, &(b.getBuffer()[off]), sizeof(ctime));
	off += sizeof(ctime);

	memcpy(&size, &(b.getBuffer()[off]), sizeof(size));
	off += sizeof(size);

	memcpy(&i_links_count, &(b.getBuffer()[off]), sizeof(i_links_count));
	off += sizeof(i_links_count);

	i_blks.clear();
	bfs_vbid_t vbid = 0;
	for (uint32_t ix = 0; ix < NUM_DIRECT_BLOCKS; ix++) {
		memcpy(&vbid, &(b.getBuffer()[off]), sizeof(bfs_vbid_t));
		i_blks.push_back(vbid);
		off += sizeof(bfs_vbid_t);
	}

	memcpy(&vbid, &(b.getBuffer()[off]), sizeof(bfs_vbid_t));
	i_blks.push_back(vbid);
	off += sizeof(bfs_vbid_t);
	assert(i_blks.size() == NUM_INODE_IBLKS);

	dirty = false;

	return (off - off_start);
}

/**
 * DirEntry definitions
 */

/**
 * @brief Initializes an in-memory representation of a dentry structure.
 *
 * @param n: the name of the file/inode that the dentry is pointing to
 * @param i: the inode number of the dentry
 * @param b_loc: the block id where the dentry is located
 * @param idx: the index into the b_loc where the dentry is located
 */
DirEntry::DirEntry(std::string n, bfs_ino_id_t i, bfs_vbid_t b_loc,
				   uint64_t idx, uint32_t m, uint64_t s, uint32_t at,
				   uint32_t mt, uint32_t ct)
	: CacheableObject() {
	de_name = n;
	ino = i;
	blk_loc = b_loc;
	blk_idx_loc = idx;

	e_mode = m;
	e_size = s;

	atime = at;
	mtime = mt;
	ctime = ct;

	dirty = true;
}

/**
 * @brief Cleanup the in-memory dentry structure.
 */
DirEntry::~DirEntry() {}

/**
 * @brief Gets the name of the file/inode that the dentry points to.
 *
 * @return std::string: file name
 */
std::string DirEntry::get_de_name() { return de_name; };

/**
 * @brief Gets the inode number that the dentry points to.
 *
 * @return bfs_ino_id_t: inode number
 */
bfs_ino_id_t DirEntry::get_ino() { return ino; };

/**
 * @brief Gets the block location where the dentry is located.
 *
 * @return bfs_vbid_t: block id where dentry is written to
 */
bfs_vbid_t DirEntry::get_blk_loc() { return blk_loc; }

/**
 * @brief Gets the index into the b_loc where the dentry is located.
 *
 * @return uint64_t: dentry block index
 */
uint64_t DirEntry::get_idx_loc() { return blk_idx_loc; }

uint32_t DirEntry::get_e_mode() { return e_mode; }

uint64_t DirEntry::get_e_size() { return e_size; }

uint32_t DirEntry::get_atime() { return atime; }

uint32_t DirEntry::get_mtime() { return mtime; }

uint32_t DirEntry::get_ctime() { return ctime; }

/**
 * @brief Sets the name in the dentry.
 *
 * @param d: new name
 */
void DirEntry::set_de_name(std::string d) {
	de_name = d;
	dirty = true;
}

/**
 * @brief Sets the inode number in the dentry.
 *
 * @param i: new inode number
 */
void DirEntry::set_ino(bfs_ino_id_t i) {
	ino = i;
	dirty = true;
}

/**
 * @brief Sets the block location of the dentry.
 *
 * @param v: block location
 */
void DirEntry::set_blk_loc(bfs_vbid_t v) {
	blk_loc = v;
	dirty = true;
}

/**
 * @brief Sets the dentry block index.
 *
 * @param i: index into the block
 */
void DirEntry::set_blk_idx_loc(uint64_t i) {
	blk_idx_loc = i;
	dirty = true;
}

/**
 * @brief Serialize into an on-bdev format beginning at the offset
 * off_start. Copies in the inode number, then copies in the
 * MAX_FILE_NAME_LEN-byte dentry name.
 *
 * @param b: block to copy to
 * @param off_start: offset to start copying to
 * @return uint64_t: number of bytes copied
 */
int64_t DirEntry::serialize(VBfsBlock &b, uint64_t off_start) {
	uint64_t off = off_start;

	memcpy(&(b.getBuffer()[off]), &ino, sizeof(ino));
	off += sizeof(ino);

	assert(de_name.size() + 1 <= MAX_FILE_NAME_LEN);
	assert((sizeof(ino) + de_name.size() + 1) <= DIRENT_SZ);
	memcpy(&(b.getBuffer()[off]), de_name.c_str(), de_name.size());
	off += de_name.size();
	memset(&(b.getBuffer()[off]), '\0',
		   DIRENT_SZ - (off - off_start)); // pad to DIRENT_SZ
	off += DIRENT_SZ - (off - off_start);

	return (off - off_start);
}

/**
 * @brief Deserialize into an in-memory format beginning at the offset
 * off_start. Copies in the inode number, then copies in the
 * MAX_FILE_NAME_LEN-byte dentry name. The other two dentry members (blk
 * location and blk index location are only in-memory, and they are initialized
 * by the caller.)
 *
 * @param b: block to copy from
 * @param off_start: offset to start copying from
 * @return uint64_t: number of bytes copied
 */
int64_t DirEntry::deserialize(VBfsBlock &b, uint64_t off_start) {
	uint64_t off = off_start;
	char p[MAX_FILE_NAME_LEN] = {0};

	memcpy(&ino, &(b.getBuffer()[off]), sizeof(ino));
	off += sizeof(ino);

	memcpy(p, &(b.getBuffer()[off]),
		   MAX_FILE_NAME_LEN); // copy any pad bytes too
	de_name = std::string(p);
	off += MAX_FILE_NAME_LEN;

	dirty = false;

	return (off - off_start);
}

/**
 * IndirectBlock definitions
 */

/**
 * @brief Initializes an in-memory representation of an indirect block
 * structure. Then allocates an appropriate number of _indirect data block_ ids
 * (the actual block ids where data will be written) for the _indirect block_
 * (the block id that is the last i_blk of the inode, which holds the ids for
 * the indirect data blocks) and zeros them out.
 */
IndirectBlock::IndirectBlock() {
	indirect_locs.resize(NUM_BLKS_PER_IB);
	std::fill(indirect_locs.begin(), indirect_locs.end(), 0);
}

/**
 * @brief Gets the list of indirect data block ids.
 *
 * @return const std::vector<bfs_vbid_t>&: reference to the list of ids
 */
const std::vector<bfs_vbid_t> &IndirectBlock::get_indirect_locs() {
	return indirect_locs;
};

/**
 * @brief Set a value (newly allocated indirect data blk) in the indirect blk.
 *
 * @param idx: index into the indirect block to set
 * @param vbid: block id of the newly allocated block
 */
void IndirectBlock::set_indirect_loc(uint64_t idx, bfs_vbid_t vbid) {
	indirect_locs.at(idx) = vbid;
}

/**
 * @brief Serialize into an on-bdev format beginning at the offset
 * off_start. Copies all of the indirect block ids to the block from the
 * vector (including unused/0 ids).
 *
 * @param b: block to copy to
 * @param off_start: offset to start copying to
 * @return uint64_t: number of bytes copied
 */
int64_t IndirectBlock::serialize(VBfsBlock &b, uint64_t off_start) {
	uint64_t off = off_start;

	// only write as many ids as exist, the rest should be 0
	assert(indirect_locs.size() <= NUM_BLKS_PER_IB);
	for (uint32_t ix = 0; ix < indirect_locs.size(); ix++) {
		memcpy(&(b.getBuffer()[off]), &indirect_locs.at(ix),
			   sizeof(bfs_vbid_t));
		off += sizeof(bfs_vbid_t);
	}

	// just zero out the next one to mark the end, but dont change off
	// Note: this will prevent any garbage values from creeping past the end of
	// the ib locs (although the block should be zeroed out before being written
	// anyway, so this is redundant)
	if (indirect_locs.size() < (NUM_BLKS_PER_IB - 1))
		memset(&(b.getBuffer()[off]), 0x0, sizeof(bfs_vbid_t));

	return (off - off_start);
}

/**
 * @brief Deserialize into an in-memory format beginning at the offset
 * off_start. Copies all of the indirect block ids from the block into the
 * vector (including 0s to indicate unused ids).
 *
 * @param b: block to copy from
 * @param off_start: offset to start copying from
 * @return uint64_t: number of bytes copied
 */
int64_t IndirectBlock::deserialize(VBfsBlock &b, uint64_t off_start) {
	uint64_t off = off_start;
	bfs_vbid_t vbid = 0;

	indirect_locs.clear();
	for (uint32_t ix = 0; ix < NUM_BLKS_PER_IB; ix++) {
		memcpy(&vbid, &(b.getBuffer()[off]), sizeof(bfs_vbid_t));
		indirect_locs.push_back(
			vbid); // read entire block and fill it even with 0s
		off += sizeof(bfs_vbid_t);
	}
	assert(indirect_locs.size() <= NUM_BLKS_PER_IB);

	return (off - off_start);
}

/**
 * OpenFile definitions
 */

/**
 * @brief Initializes a structure that tracks a read/write offset on an inode
 * The offset is only appropriate for regular files.
 *
 * @param i: inode to map to
 * @param o: offset to track
 */
OpenFile::OpenFile(bfs_ino_id_t i, uint64_t o) {
	ino = i;
	offset = o;
}

/**
 * @brief Get the inode that the object maps to.
 *
 * @return bfs_ino_id_t: mapped inode
 */
bfs_ino_id_t OpenFile::get_ino() { return ino; }

/**
 * @brief Get the currently tracked offset into the inode's data.
 *
 * @param o: offset in the inode's data
 */
void OpenFile::set_offset(uint64_t o) { offset = o; }

/**
 * BfsServerError definitions
 */

/**
 * @brief Exception thrown as a result of a server error. Expects at most two
 * parent/child inode arguments for which to try to release inode locks during
 * exception handling.
 *
 * @param s: error string
 * @param par_ino: parent inode
 * @param ino: child inode
 */
BfsServerError::BfsServerError(std::string s, Inode *par_ino, Inode *ino) {
	err_msg = s;

	if (par_ino && !par_ino->unlock()) {
		logMessage(LOG_ERROR_LEVEL, "Error when releasing parent inode lock\n");
		abort(); // very bad
	}

	if (ino && !ino->unlock()) {
		logMessage(LOG_ERROR_LEVEL, "Error when releasing inode lock\n");
		abort(); // very bad
	}
}

/**
 * @brief Gets the error message description in a format suitable for printing.
 *
 * @return std::string: description of the error
 */
std::string BfsServerError::err() { return err_msg; }

/**
 * BfsClientRequestFailedError definitions
 */

/**
 * @brief Exceptions thrown as a result of a failed client request. Expects at
 * most two parent/child inode arguments for which to try to release inode locks
 * during exception handling.
 *
 * @param s: error string
 * @param par_ino: parent inode
 * @param ino: child inode
 */
BfsClientRequestFailedError::BfsClientRequestFailedError(std::string s,
														 Inode *par_ino,
														 Inode *ino) {
	err_msg = s;

	if (par_ino && !par_ino->unlock()) {
		logMessage(LOG_ERROR_LEVEL, "Error when releasing parent inode lock\n");
		abort(); // very bad
	}

	logMessage(FS_VRB_LOG_LEVEL, "Released parent inode lock\n");

	if (ino && !ino->unlock()) {
		logMessage(LOG_ERROR_LEVEL, "Error when releasing inode lock\n");
		abort(); // very bad
	}

	logMessage(FS_VRB_LOG_LEVEL, "Released inode lock\n");
}

/**
 * @brief Gets the error message description in a format suitable for printing.
 *
 * @return std::string: description of the error
 */
std::string BfsClientRequestFailedError::err() { return err_msg; }

/**
 * BfsAccessDeniedError definitions
 */

/**
 * @brief Exception thrown as a result of an access denied for a client request.
 * Expects at most two parent/child inode arguments for which to try to release
 * inode locks during exception handling.
 *
 * @param s: error string
 * @param par_ino: parent inode
 * @param ino: child inode
 */
BfsAccessDeniedError::BfsAccessDeniedError(std::string s, Inode *par_ino,
										   Inode *ino) {
	err_msg = s;

	if (par_ino && !par_ino->unlock()) {
		logMessage(LOG_ERROR_LEVEL, "Error when releasing parent inode lock\n");
		abort(); // very bad
	}

	if (ino && !ino->unlock()) {
		logMessage(LOG_ERROR_LEVEL, "Error when releasing inode lock\n");
		abort(); // very bad
	}
}

/**
 * @brief Gets the error message description in a format suitable for printing.
 *
 * @return std::string: description of the error
 */
std::string BfsAccessDeniedError::err() { return err_msg; }

// #if 0

// #ifdef __BFS_ENCLAVE_MODE
// /**
//  * @brief Log all of the time measurements to a file.
//  *
//  */
// void write_core_latencies() {
// 	if (!bfsUtilLayer::perf_test() || !collect_core_lats)
// 		return;

// 	// Log network-related sends for enclave reads
// 	std::string __s_read__net_d_send_lats =
// 		vec_to_str<long>(s_read__net_d_send_lats);
// 	std::string __s_read__net_d_send_lats_fname("__s_read__net_d_send_lats");
// 	write_lats_to_file(__s_read__net_d_send_lats,
// 					   __s_read__net_d_send_lats_fname);
// 	logMessage(
// 		FS_LOG_LEVEL,
// 		"read latencies (network sends device, us, %lu records):\n[%s]\n",
// 		s_read__net_d_send_lats.size(), __s_read__net_d_send_lats.c_str());

// 	// Log network-related sends for enclave writes
// 	std::string __s_write__net_d_send_lats =
// 		vec_to_str<long>(s_write__net_d_send_lats);
// 	std::string __s_write__net_d_send_lats_fname("__s_write__net_d_send_lats");
// 	write_lats_to_file(__s_write__net_d_send_lats,
// 					   __s_write__net_d_send_lats_fname);
// 	logMessage(
// 		FS_LOG_LEVEL,
// 		"Write latencies (network sends device, us, %lu records):\n[%s]\n",
// 		s_write__net_d_send_lats.size(), __s_write__net_d_send_lats.c_str());

// 	// Log network-related sends for enclave other ops
// 	std::string __s_other__net_d_send_lats =
// 		vec_to_str<long>(s_other__net_d_send_lats);
// 	std::string __s_other__net_d_send_lats_fname("__s_other__net_d_send_lats");
// 	write_lats_to_file(__s_other__net_d_send_lats,
// 					   __s_other__net_d_send_lats_fname);
// 	logMessage(
// 		FS_LOG_LEVEL,
// 		"Other op latencies (network sends device, us, %lu records):\n[%s]\n",
// 		s_other__net_d_send_lats.size(), __s_other__net_d_send_lats.c_str());
// }
// #endif

// #ifdef __BFS_ENCLAVE_MODE
// void write_lats_to_file(std::string __e__lats, std::string __e__lats_fname) {
// 	int ocall_ret = -1;
// 	if ((ocall_write_to_file(&ocall_ret, __e__lats_fname.size() + 1,
// 							 __e__lats_fname.c_str(), __e__lats.size() + 1,
// 							 __e__lats.c_str()) != SGX_SUCCESS) ||
// 		(ocall_ret != BFS_SUCCESS)) {
// 		logMessage(LOG_ERROR_LEVEL, "Error in ocall_write_to_file for [%s]",
// 				   __e__lats.c_str());
// 		abort();
// 	}
// }
// #endif

// #endif
