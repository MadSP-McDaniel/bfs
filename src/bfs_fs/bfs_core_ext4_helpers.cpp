/**
 * @file bfs_core_ext4_helpers.cpp
 * @brief Definitions for lwext4 helpers for use by bfs and unit tests.
 */

#include "bfs_core_ext4_helpers.h"
#include "bfsLocalDevice.h"
#include "bfs_acl.h"
#include "bfs_core.h"
#include "bfs_fs_layer.h"
#include <bfs_common.h>
#include <bfs_log.h>
#include <bfs_util.h>
#include <set>
#ifdef __BFS_ENCLAVE_MODE
#include "bfs_enclave_t.h" /* For ocalls */
#elif defined(__BFS_DEBUG_NO_ENCLAVE)
#include <bfs_util_ocalls.h>
#endif

static bfsLocalDevice *bfs_blk_dev = NULL;
static std::vector<bfs_vbid_t> *seen = NULL;
static std::vector<bfs_vbid_t> *blk_accesses = NULL;
static int status = UNINITIALIZED;
static pthread_mutex_t open_file_tab_mux, mp_mux;

static int fs_type = F_SET_EXT4;

static struct ext4_fs fs;

static void _lock(void);
static void _unlock(void);
static void __lock(pthread_mutex_t *);
static void __unlock(pthread_mutex_t *);

static const struct ext4_lock mp_lock_handlers = {.lock = _lock,
												  .unlock = _unlock};

static struct ext4_mkfs_info info = {
	.block_size = BLK_SZ,
	// .journal = true,
};

typedef struct _bfs_lwext4_open_file_t {
	void *f; // ext4_file* or ext4_dir*
	char *path;
	bfs_fh_t fh;
} bfs_lwext4_open_file_t;

std::map<bfs_fh_t, bfs_lwext4_open_file_t *> *open_file_tab;

static uint8_t *curr_par = NULL;

static uint64_t next_fh = 0;
static uint64_t next_dfh = 0;

/**@brief   Default filename.*/
static char *fname = "ext4";

/**@brief   Image block size.*/
// #define EXT4_FILEDEV_BSIZE 512

/**@brief   Image file descriptor.*/
// static FILE *dev_file;
/**@brief   Input stream name.*/
static char input_name[128] = "lwext4/ext_images/ext4";

/**@brief   Read-write size*/
static int rw_szie = 131072;

/**@brief   Read-write size*/
static int rw_count = 1000;

/**@brief   Directory test count*/
static int dir_cnt = 1;

/**@brief   Cleanup after test.*/
// static bool cleanup_flag = false;

/**@brief   Block device stats.*/
// static bool bstat = false;

/**@brief   Superblock stats.*/
// static bool sbstat = false;

/**@brief   Indicates that input is windows partition.*/
// static bool winpart = false;

/**@brief   Verbose mode*/
static bool verbose = 0;

/**@brief   Block device handle.*/
static struct ext4_blockdev *bd;

/**@brief   Block cache handle.*/
// struct ext4_bcache *bc;

// static bfsLocalDevice *bfs_block_dev = NULL;

// static const char *usage = "                                    \n\
// Welcome in ext4 generic demo.                                   \n\
// Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)  \n\
// Usage:                                                          \n\
// [-i] --input    - input file         (default = ext4)           \n\
// [-w] --rw_size  - single R/W size    (default = 1024 * 1024)    \n\
// [-c] --rw_count - R/W count          (default = 10)             \n\
// [-d] --dirs   - directory test count (default = 0)              \n\
// [-l] --clean  - clean up after test                             \n\
// [-b] --bstat  - block device stats                              \n\
// [-t] --sbstat - superblock stats                                \n\
// [-w] --wpart  - windows partition mode                          \n\
// \n";

EXT4_BLOCKDEV_STATIC_INSTANCE(file_dev, BLK_SZ, 0, file_dev_open,
							  file_dev_bread, file_dev_bwrite, file_dev_close,
							  0, 0);

static int update_merkle_tree(bfs_vbid_t vbid_start, uint32_t nvbids,
							  uint8_t **macs);
static int verify_mt(bfs_vbid_t vbid_start, uint32_t nvbids, uint8_t **macs);

static char *entry_to_str(uint8_t type) {
	switch (type) {
	case EXT4_DE_UNKNOWN:
		return "[unk] ";
	case EXT4_DE_REG_FILE:
		return "[fil] ";
	case EXT4_DE_DIR:
		return "[dir] ";
	case EXT4_DE_CHRDEV:
		return "[cha] ";
	case EXT4_DE_BLKDEV:
		return "[blk] ";
	case EXT4_DE_FIFO:
		return "[fif] ";
	case EXT4_DE_SOCK:
		return "[soc] ";
	case EXT4_DE_SYMLINK:
		return "[sym] ";
	default:
		break;
	}
	return "[???]";
}

static void io_timings_clear(void) {}

static const struct ext4_io_stats *io_timings_get(uint32_t time_sum_ms) {
	return NULL;
}

double __get_time() {
	double s = 0.0;
#ifdef __BFS_ENCLAVE_MODE
	if (ocall_get_time2(&s) != SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed __get_time, aborting");
		abort();
	}
#else
	s = ocall_get_time2();
	if (!(s > 0)) {
		logMessage(LOG_ERROR_LEVEL, "Failed __get_time, aborting");
		abort();
	}
#endif
	return s;
}

static struct ext4_blockdev *file_dev_get(void) {
	/**
	 * Block layer must initialized before we set the num_blocks in bd struct
	 * using the FS layer value taken from the config. The block layer should
	 * always be initialized (by the call to FS layer init) before this is
	 * called anyway (for the normal BFS server, lwext4 file test, or lwext4
	 * block test), so just abort if it's not.
	 */
	// if (!BfsFsLayer::initialized()) {
	// 	logMessage(LOG_ERROR_LEVEL, "BfsFsLayer not initialized\n");
	// 	// return NULL;
	// 	abort();
	// }
	if (!bfsBlockLayer::initialized() || !(BFS_LWEXT4_NUM_BLKS > 0)) {
		logMessage(LOG_ERROR_LEVEL, "bfsBlockLayer not initialized properly\n");
		abort();
	}

	file_dev.bdif->ph_bcnt = BFS_LWEXT4_NUM_BLKS;
	assert(file_dev.bdif->ph_bcnt > 0);
	file_dev.part_size = file_dev.bdif->ph_bcnt * file_dev.bdif->ph_bsize;
	assert(file_dev.part_size > 0);
	file_dev.part_offset = 0;

	return &file_dev;
}

static void file_dev_name_set(char *n) { fname = n; }

static bool open_linux(void) {
	file_dev_name_set(input_name);

	// This is the primary block device pointer that gets passed around and is
	// stored in many other structs (eg the mp and fs structs). The current
	// lwext4 code base uses it as a global but also passes it as a local param
	// to some functions (and re-assigns occasionally), which is strange, but
	// after tracking it down, it seems that this is what kicks off everything
	// so that the statically declared instance (file_dev) is stored in bd
	// (during the open_filedev() call) and passed around thereafter.
	bd = file_dev_get();

	// // force set these explicitly for now, since the static struct
	// // initialization doesnt seem to be setting them
	// Edit: NVM these are set in mkfs
	// bd->lg_bcnt = bd->bdif->ph_bsize;
	// bd->lg_bcnt = bd->bdif->ph_bcnt;

	if (!bd) {
		logMessage(LOG_ERROR_LEVEL, "open_filedev: fail\n");
		return false;
	}

	return true;
}

static bool open_filedev(void) { return open_linux(); }

int file_dev_open(struct ext4_blockdev *bdev) {
	// init_blk_dev(&bfs_blk_dev);
	if (bfs_blk_dev)
		return BFS_SUCCESS;

	// bdev should always be given
	if (!bdev)
		return BFS_FAILURE;

	bfs_blk_dev = new bfsLocalDevice(
		1, std::string(""),
		bdev->bdif->ph_bcnt + BFS_LWEXT4_META_SPC); // reads cfg for path

	if (bfs_blk_dev->bfsDeviceInitialize() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failure during bfsDeviceInitialize");
		return BFS_FAILURE;
	}

	// bdev should be == file_dev
	// bdev->part_offset = 0;
	// bdev->part_size = 1000; // should already be initialized to 1000 for now
	// bdev->bdif->ph_bcnt = bdev->part_size / bdev->bdif->ph_bsize;
	// bdev->bdif->ph_bcnt = bdev->part_size / bdev->bdif->ph_bsize;

	/**
	 * The ph_bcnt and part_size should already be set in open_filedev(), so
	 * here we should be OK. The other fields (eg lg_bcnt) should be set
	 * thereafter from these.
	 */

	// bdev->part_size = bdev->bdif->ph_bcnt * bdev->bdif->ph_bsize;

	// dev_file = fopen(fname, "r+b");

	// if (!dev_file)
	// 	return EIO;

	// /*No buffering at file.*/
	// setbuf(dev_file, 0);

	// if (fseeko(dev_file, 0, SEEK_END))
	// 	return EFAULT;

	// file_dev.part_offset = 0;
	// file_dev.part_size = ftello(dev_file);
	// file_dev.bdif->ph_bcnt = file_dev.part_size / file_dev.bdif->ph_bsize;

	if (bfsUtilLayer::use_mt()) {
		// If block I/O unit test is the caller, it would be ==UNINITIALIZED,
		// but if this is called by the file I/O unit test or the actual bfs
		// server (ext4_mkfs/ext4_mount) then it would be ==INITIALIZED.
		// Only init merkle tree if fs is in initialized state (ie not formatted
		// nor mounted).
		if ((status == FORMATTED) || (status == FORMATTING)) {
			if (BfsFsLayer::init_merkle_tree(true) != BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failed initializing merkle tree\n");
				return BFS_FAILURE;
			}
			logMessage(FS_LOG_LEVEL, "Merkle tree initialized");
		} else {
			logMessage(LOG_ERROR_LEVEL, "Bad FS status [%d]", status);
			return BFS_FAILURE;
		}
	}

	return EOK;
}

int file_dev_close(struct ext4_blockdev *bdev) {
	if (!bfs_blk_dev)
		return BFS_FAILURE;
	// fclose(dev_file);

	if (bfsUtilLayer::use_mt()) {
		// If block I/O unit test is the caller, it would be XXX,
		// but if this is called by the file I/O unit test or the actual bfs
		// server (ext4_mkfs/ext4_mount) then it would be XXX.
		// Only allow flush if the FS is in mounted state.
		if ((status == MOUNTED) || (status == FORMATTING)) {
			if (BfsFsLayer::flush_merkle_tree() != BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL, "Failed flushing merkle tree\n");
				return BFS_FAILURE;
			}
			logMessage(FS_LOG_LEVEL, "Merkle tree flushed");
		} else {
			logMessage(LOG_ERROR_LEVEL, "Bad FS status [%d]", status);
			return BFS_FAILURE;
		}
	}

	if (bfs_blk_dev->bfsDeviceUninitialize() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failure during bfsDeviceUninitialize");
		return BFS_FAILURE;
	}

	delete bfs_blk_dev;

	// might be reused in same process (eg by unit test), so clear
	bfs_blk_dev = NULL;
	// bd = NULL;

	return EOK;
}

int file_dev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
				   uint32_t blk_cnt) {
	if (!bfs_blk_dev)
		return BFS_FAILURE;
	// if (fseeko(dev_file, blk_id * bdev->bdif->ph_bsize, SEEK_SET))
	// 	return EIO;
	if (!blk_cnt)
		return EOK;
	// if (!fread(buf, bdev->bdif->ph_bsize * blk_cnt, 1, dev_file))
	// 	return EIO;

	// logMessage(LOG_ERROR_LEVEL, "start file_dev_bread");

	// if (blk_cnt > 1) {
	// 	logMessage(LOG_ERROR_LEVEL, "blk_cnt > 1");
	// 	return BFS_FAILURE;
	// }

	// if ((blk_id >= METADATA_REL_START_BLK_NUM) &&
	// 	(blk_id < DATA_REL_START_BLK_NUM)) {
	// 	logMessage(LOG_ERROR_LEVEL, "Trying to write to meta block directly");
	// 	return BFS_FAILURE;
	// }

	if (!BfsFsLayer::get_SA())
		return BFS_FAILURE;

	if (blk_accesses)
		blk_accesses[0].push_back(blk_id);

	// TODO: remap blk_id onto a suitable physical block id.
	// The challenge is that ext4 only supports powers-of-2 block sizes.
	// Maybe as a workound, we can use 4096-byte blocks and make the mmap
	// 4096+MAC*num_blocks.

	// Unlike BFS, the lwext4 code sometimes reads blocks that have not yet been
	// written do, even during mkfs. So our decryption will therefore fail. This
	// is a workaround that basically checks if we have already seen the block
	// during mkfs. If not, then it completes the read by leaving the buf as-is.
	if (!seen) {
		logMessage(LOG_ERROR_LEVEL, "seen is null");
		return BFS_FAILURE;
	}
	// logMessage(LOG_ERROR_LEVEL, "seen: %s",
	// 		   vec_to_str<bfs_vbid_t>(*seen).c_str());

	bool found = false;
	VBfsBlock blk(NULL, BLK_SZ, 0, 0, 0);
	bfs_vbid_t vbid = 0;
	bfs_vbid_t vbid_start = blk_id;
	// uint8_t **macs = (uint8_t **)calloc(blk_cnt, sizeof(uint8_t *));
	uint8_t **macs = new uint8_t *[blk_cnt];
	uint8_t *mac_copy = NULL, *iv = NULL;
	bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer();
	for (int b_idx = 0; b_idx < blk_cnt; b_idx++) {
		vbid = blk_id + b_idx;
		found = std::find(seen->begin(), seen->end(), vbid) != seen->end();
		if ((status >= FORMATTING) && !found) {
			memset(buf, 0, BLK_SZ);
			// logMessage(FS_LOG_LEVEL, "Skipping read of block [%lu]", vbid);
			// return BFS_SUCCESS;
			continue;
		}

		blk.set_vbid(vbid);

		mac_copy = (uint8_t *)calloc(
			BfsFsLayer::get_SA()->getKey()->getMACsize(), sizeof(uint8_t));
		macs[b_idx] = mac_copy;
		iv = (uint8_t *)calloc(BfsFsLayer::get_SA()->getKey()->getIVlen(),
							   sizeof(uint8_t));

		// first read raw block
		if (__do_get_block(vbid, blk.getBuffer()) != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL, "Failed getting physical block [%lu]",
					   vbid);
			goto err;
		}
		// for (int b_idx = 0; b_idx < blk_cnt; b_idx++) {
		// 	if (bfs_blk_dev->getBlock(blk_id + b_idx,
		// 							  &((char *)buf)[b_idx * BLK_SZ])) {
		// 		logMessage(LOG_ERROR_LEVEL, "Failed getting physical block
		// [%lu]", 				   blk_id); 		return BFS_FAILURE;
		// 	}
		// }

		// now decrypt and get plaintext
		// int32_t z = -1;
		// blk.removeTrailer((char *)&z, sizeof(z));

		// if (!BfsFsLayer::get_SA())
		// 	return BFS_FAILURE;

		// if (status < FORMATTED) {
		// 	logMessage(LOG_ERROR_LEVEL,
		// 			   "Failed read_blk, filesystem not formatted");
		// 	return BFS_FAILURE;
		// }

		// Note: here we use MAC/GMAC size (16B hash) for the block but check
		// the HMAC (32B hash) of the root below uint8_t *const mac_copy =
		// (uint8_t *)calloc( 	BfsFsLayer::get_SA()->getKey()->getMACsize(),
		// sizeof(uint8_t));
		if (BfsFsLayer::read_blk_meta(vbid, &iv, &mac_copy) != BFS_SUCCESS)
			goto err;

		// decrypt and verify MAC
		aad->burn();
		aad->resizeAllocation(0, sizeof(vbid), 0);
		*aad << vbid;
		BfsFsLayer::get_SA()->decryptData2(blk, aad, iv, mac_copy);
		// delete aad;

		// validate that correct amount of data read, then copy over
		assert(blk.getLength() == BLK_SZ);

		free(iv);

		// Now do mt checks.
		// Copy over contents before verifying if they are correct. This is a
		// workaround to avoid dealing with another temp buffer when handling
		// multiple blocks. Doesn't affect anything.
		memcpy(buf + (b_idx * BLK_SZ), blk.getBuffer(), BLK_SZ);

		// Only do the update on the last block.
		if ((blk_cnt > 1) && (b_idx < (blk_cnt - 1)))
			continue;
		if (bfsUtilLayer::use_mt() && (status == MOUNTED) &&
			(verify_mt(vbid_start, blk_cnt, macs) != BFS_SUCCESS)) {
			logMessage(LOG_ERROR_LEVEL, "Failed to check merkle tree\n");
			goto err;
		}
	}
	// free(macs); // just free the array, not the macs themselves
	delete[] macs;
	macs = NULL;
	// logMessage(FS_VRB_LOG_LEVEL,
	// 		   "end file_dev_bread loop [blk_id=%lu, blk_cnt=%d]", blk_id,
	// 		   blk_cnt);

	delete aad;
	return BFS_SUCCESS;

err:
	if (mac_copy)
		free(mac_copy);
	if (macs)
		free(macs);
	if (iv)
		free(iv);
	delete aad;
	return BFS_FAILURE;
}

int file_dev_bwrite(struct ext4_blockdev *bdev, const void *buf,
					uint64_t blk_id, uint32_t blk_cnt) {
	if (!bfs_blk_dev)
		return BFS_FAILURE;
	// if (fseeko(dev_file, blk_id * bdev->bdif->ph_bsize, SEEK_SET))
	// 	return EIO;
	if (!blk_cnt)
		return EOK;

	// logMessage(LOG_ERROR_LEVEL, "start file_dev_bwrite");

	// if (blk_cnt > 1) {
	// 	logMessage(LOG_ERROR_LEVEL, "blk_cnt > 1");
	// 	return BFS_FAILURE;
	// }

	// if ((blk_id >= METADATA_REL_START_BLK_NUM) &&
	// 	(blk_id < DATA_REL_START_BLK_NUM)) {
	// 	logMessage(LOG_ERROR_LEVEL, "Trying to write to meta block directly");
	// 	return BFS_FAILURE;
	// }

	// if (!fwrite(buf, bdev->bdif->ph_bsize * blk_cnt, 1, dev_file))
	// 	return EIO;

	if (!BfsFsLayer::get_SA())
		return BFS_FAILURE;

	if (blk_accesses)
		blk_accesses[1].push_back(blk_id);

	if (!seen) {
		logMessage(LOG_ERROR_LEVEL, "seen is null");
		return BFS_FAILURE;
	}
	// logMessage(LOG_ERROR_LEVEL, "seen: %s",
	// 		   vec_to_str<bfs_vbid_t>(*seen).c_str());

	VBfsBlock blk(NULL, BLK_SZ, 0, 0, 0);

	// TODO: make sure we clean these up properly
	uint8_t *mac_copy = NULL, *iv = NULL;
	// if (bfsUtilLayer::use_mt() && (status == MOUNTED)) {
	// if (status != CORRUPTED) {

	bfs_vbid_t vbid = 0;
	bfs_vbid_t vbid_start = blk_id;
	// uint8_t **macs = (uint8_t **)calloc(blk_cnt, sizeof(uint8_t *));
	uint8_t **macs = new uint8_t *[blk_cnt];
	bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer();
	for (int b_idx = 0; b_idx < blk_cnt; b_idx++) {
		// logMessage(LOG_ERROR_LEVEL, "start file_dev_bwrite loop");

		vbid = blk_id + b_idx;
		if ((status >= FORMATTING) &&
			(std::find(seen->begin(), seen->end(), vbid) == seen->end())) {
			// logMessage(LOG_ERROR_LEVEL, "Adding block [%lu] to seen", vbid);
			// (*seen)[vbid] = true;
			seen->push_back(vbid);
		}

		blk.set_vbid(vbid);
		blk.setData((const char *)(buf + (b_idx * BLK_SZ)), BLK_SZ);

		mac_copy = (uint8_t *)calloc(
			BfsFsLayer::get_SA()->getKey()->getMACsize(), sizeof(uint8_t));
		macs[b_idx] = mac_copy;
		iv = (uint8_t *)calloc(BfsFsLayer::get_SA()->getKey()->getIVlen(),
							   sizeof(uint8_t));

		// encrypt and add MAC tag
		// The buffer should contain the IV (12 bytes) + data (4064) + MAC (16
		// bytes) and should fit within the device block size.
		aad->burn();
		aad->resizeAllocation(0, sizeof(vbid), 0);
		*aad << vbid;
		BfsFsLayer::get_SA()->encryptData2(blk, aad, &iv, &mac_copy);

		assert(blk.getLength() == BLK_SZ);

		// logMessage(LOG_ERROR_LEVEL, "file_dev_bwrite loop encrypt done");

		if (BfsFsLayer::write_blk_meta(vbid, &iv, &mac_copy) != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL, "Failed writing security metadata");
			goto err;
		}

		// logMessage(LOG_ERROR_LEVEL, "file_dev_bwrite loop write_blk_meta
		// done");

		// uint8_t *mac_copy;
		// if (bfsUtilLayer::use_mt() && (status != CORRUPTED)) {
		// 	mac_copy = (uint8_t *)calloc(
		// 		BfsFsLayer::get_SA()->getKey()->getMACsize(), sizeof(uint8_t));
		// 	// copy over before block is prepared to be written to disk/network
		// 	memcpy(mac_copy, &(blk.getBuffer()[BFS_IV_LEN + BLK_SZ +
		// PKCS_PAD_SZ]), BfsFsLayer::get_SA()->getKey()->getMACsize());
		// }

		// int32_t z = 0;
		// blk.addTrailer((char *)&z, sizeof(z));

		// for (int b_idx = 0; b_idx < blk_cnt; b_idx++) {
		// 	if (bfs_blk_dev->putBlock(blk_id + b_idx,
		// 							  &((char *)buf)[b_idx * BLK_SZ])) {
		// 		logMessage(LOG_ERROR_LEVEL, "Failed putting physical block
		// [%lu]", 				   blk_id); 		return BFS_FAILURE;
		// 	}
		// }
		if (__do_put_block(vbid, blk.getBuffer()) != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL, "Failed getting physical block [%lu]",
					   vbid);
			goto err;
		}

		// logMessage(LOG_ERROR_LEVEL, "file_dev_bwrite loop __do_put_block
		// done");

		free(iv);

		// Now do mt updates.
		// For synchronous multi-block writes, just do a batch update (ie we are
		// not caching them then batching); this should eliminate having to do a
		// bunch of hashes when the lwext4 code knows it is doing multi-block
		// writes.
		// Only do the update on the last block.
		if ((blk_cnt > 1) && (b_idx < (blk_cnt - 1)))
			continue;
		if (bfsUtilLayer::use_mt() && (status != CORRUPTED) &&
			(update_merkle_tree(vbid_start, blk_cnt, macs) != BFS_SUCCESS)) {
			logMessage(LOG_ERROR_LEVEL, "Failed to update merkle tree\n");
			goto err;
		}
		// For testing the new group-based logic in verify/update, just use:
		// if (bfsUtilLayer::use_mt() && (status != CORRUPTED) &&
		// 	(update_merkle_tree(vbid_start + b_idx, 1, &macs[b_idx]) !=
		// 	 BFS_SUCCESS)) {
		// 	logMessage(LOG_ERROR_LEVEL, "Failed to update merkle tree\n");
		// 	goto err;
		// }

		// logMessage(LOG_ERROR_LEVEL,
		// 		   "file_dev_bwrite loop update_merkle_tree done");
		// logMessage(LOG_ERROR_LEVEL, "end file_dev_bwrite loop");
	}
	// free(macs); // just free the array, not the macs themselves
	delete[] macs;
	macs = NULL;
	// logMessage(FS_VRB_LOG_LEVEL,
	// 		   "end file_dev_bwrite loop [blk_id=%lu, blk_cnt=%d]", blk_id,
	// 		   blk_cnt);

	// drop_cache();
	delete aad;
	return BFS_SUCCESS;

err:
	if (mac_copy)
		free(mac_copy);
	if (macs)
		free(macs);
	if (iv)
		free(iv);
	delete aad;
	return BFS_FAILURE;
}

int __do_get_block(bfs_vbid_t b, void *buf) {
	if (!bfs_blk_dev)
		return BFS_FAILURE;

	if (bfs_blk_dev->getBlock(b, (char *)buf)) {
		logMessage(LOG_ERROR_LEVEL, "Failed getting physical block [%lu]", b);
		return BFS_FAILURE;
	}

	return EOK;
}

int __do_put_block(bfs_vbid_t b, void *buf) {
	if (!bfs_blk_dev)
		return BFS_FAILURE;

	if (bfs_blk_dev->putBlock(b, (char *)buf)) {
		logMessage(LOG_ERROR_LEVEL, "Failed putting physical block [%lu]", b);
		return BFS_FAILURE;
	}

	return EOK;
}

// static void drop_cache(void) {
// #if defined(__linux__) && DROP_LINUXCACHE_BUFFERS
// 	int fd;
// 	char *data = "3";

// 	sync();
// 	fd = open("/proc/sys/vm/drop_caches", O_WRONLY);
// 	write(fd, data, sizeof(char));
// 	close(fd);
// #endif
// }

// static long int get_ms(void) { return 0; }
static long int get_ms(void) { return (long int)(__get_time() / 1e3); }

static uint32_t get_s(void) { return (uint32_t)(__get_time() / 1e6); }

static void printf_io_timings(long int diff) {
	const struct ext4_io_stats *stats = io_timings_get(diff);
	if (!stats)
		return;

	logMessage(FS_LOG_LEVEL, "io_timings:\n");
	logMessage(FS_LOG_LEVEL, "  io_read: %.3f%%\n", (double)stats->io_read);
	logMessage(FS_LOG_LEVEL, "  io_write: %.3f%%\n", (double)stats->io_write);
	logMessage(FS_LOG_LEVEL, "  io_cpu: %.3f%%\n", (double)stats->cpu);
}

static void test_lwext4_dir_ls(const char *path) {
	char sss[255];
	ext4_dir d;
	const ext4_direntry *de;

	logMessage(FS_LOG_LEVEL, "ls %s\n", path);

	ext4_dir_open(&d, path);
	de = ext4_dir_entry_next(&d);

	while (de) {
		memcpy(sss, de->name, de->name_length);
		sss[de->name_length] = 0;
		logMessage(FS_LOG_LEVEL, "  %s%s\n", entry_to_str(de->inode_type), sss);
		de = ext4_dir_entry_next(&d);
	}
	ext4_dir_close(&d);
}

static void test_lwext4_mp_stats(void) {
	struct ext4_mount_stats stats;
	ext4_mount_point_stats("/", &stats);

	logMessage(FS_LOG_LEVEL, "********************\n");
	logMessage(FS_LOG_LEVEL, "ext4_mount_point_stats\n");
	logMessage(FS_LOG_LEVEL, "inodes_count = %" PRIu32 "\n",
			   stats.inodes_count);
	logMessage(FS_LOG_LEVEL, "free_inodes_count = %" PRIu32 "\n",
			   stats.free_inodes_count);
	logMessage(FS_LOG_LEVEL, "blocks_count = %" PRIu32 "\n",
			   (uint32_t)stats.blocks_count);
	logMessage(FS_LOG_LEVEL, "free_blocks_count = %" PRIu32 "\n",
			   (uint32_t)stats.free_blocks_count);
	logMessage(FS_LOG_LEVEL, "block_size = %" PRIu32 "\n", stats.block_size);
	logMessage(FS_LOG_LEVEL, "block_group_count = %" PRIu32 "\n",
			   stats.block_group_count);
	logMessage(FS_LOG_LEVEL, "blocks_per_group= %" PRIu32 "\n",
			   stats.blocks_per_group);
	logMessage(FS_LOG_LEVEL, "inodes_per_group = %" PRIu32 "\n",
			   stats.inodes_per_group);
	logMessage(FS_LOG_LEVEL, "volume_name = %s\n", stats.volume_name);
	logMessage(FS_LOG_LEVEL, "********************\n");
}

static void test_lwext4_block_stats(void) {
	if (!bd)
		return;

	logMessage(FS_LOG_LEVEL, "********************\n");
	logMessage(FS_LOG_LEVEL, "ext4 blockdev stats\n");
	logMessage(FS_LOG_LEVEL, "bdev->bread_ctr = %" PRIu32 "\n",
			   bd->bdif->bread_ctr);
	logMessage(FS_LOG_LEVEL, "bdev->bwrite_ctr = %" PRIu32 "\n",
			   bd->bdif->bwrite_ctr);

	logMessage(FS_LOG_LEVEL, "bcache->ref_blocks = %" PRIu32 "\n",
			   bd->bc->ref_blocks);
	logMessage(FS_LOG_LEVEL, "bcache->max_ref_blocks = %" PRIu32 "\n",
			   bd->bc->max_ref_blocks);
	logMessage(FS_LOG_LEVEL, "bcache->lru_ctr = %" PRIu32 "\n",
			   bd->bc->lru_ctr);

	logMessage(FS_LOG_LEVEL, "\n");

	logMessage(FS_LOG_LEVEL, "********************\n");
}

static bool test_lwext4_dir_test(int len) {
	ext4_file f;
	int r;
	int i;
	char path[64];
	long int diff;
	long int stop;
	long int start;

	logMessage(FS_LOG_LEVEL, "test_lwext4_dir_test: %d\n", len);
	io_timings_clear();
	start = get_ms();

	logMessage(FS_LOG_LEVEL, "directory create: /dir1\n");
	r = ext4_dir_mk("/dir1", NULL);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_dir_mk: rc = %d\n", r);
		return false;
	}

	logMessage(FS_LOG_LEVEL, "add files to: /dir1\n");
	for (i = 0; i < len; ++i) {
		snprintf(path, 63, "/dir1/f%d", i);
		path[63] = '\0';
		r = ext4_fopen(&f, path, "wb");
		if (r != EOK) {
			logMessage(LOG_ERROR_LEVEL, "ext4_fopen: rc = %d\n", r);
			return false;
		}
	}

	stop = get_ms();
	diff = stop - start;
	test_lwext4_dir_ls("/dir1");
	logMessage(FS_LOG_LEVEL, "test_lwext4_dir_test: time: %d ms\n", (int)diff);
	logMessage(FS_LOG_LEVEL, "test_lwext4_dir_test: av: %d ms/entry\n",
			   (int)diff / (len + 1));
	printf_io_timings(diff);
	return true;
}

static int verify_buf(const unsigned char *b, size_t len, unsigned char c) {
	size_t i;
	for (i = 0; i < len; ++i) {
		if (b[i] != c)
			return c - b[i];
	}

	return 0;
}

static bool test_lwext4_file_test(uint8_t *rw_buff, uint32_t rw_size,
								  uint32_t rw_count) {
	int r;
	size_t size;
	uint32_t i;
	long int start;
	long int stop;
	long int diff;
	uint32_t xbps;
	uint64_t size_bytes;

	ext4_file f;

	logMessage(FS_LOG_LEVEL, "file_test:\n");
	logMessage(FS_LOG_LEVEL, "  rw size: %" PRIu32 "\n", rw_size);
	logMessage(FS_LOG_LEVEL, "  rw count: %" PRIu32 "\n", rw_count);

	/*Add hello world file.*/
	r = ext4_fopen(&f, "/hello.txt", "wb");
	r = ext4_fwrite(&f, "Hello World !\n", strlen("Hello World !\n"), 0);
	r = ext4_fclose(&f);

	r = ext4_fopen(&f, "/test1", "wb");
	if (r != EOK) {
		logMessage(FS_LOG_LEVEL, "ext4_fopen ERROR = %d\n", r);
		return false;
	}

	logMessage(FS_LOG_LEVEL, "ext4_write: %" PRIu32 " * %" PRIu32 " ...\n",
			   rw_size, rw_count);

	io_timings_clear();
	start = get_ms();

	for (i = 0; i < rw_count; ++i) {

		memset(rw_buff, i % 10 + '0', rw_size);

		r = ext4_fwrite(&f, rw_buff, rw_size, &size);

		if ((r != EOK) || (size != rw_size))
			break;
	}

	if (i != rw_count) {
		logMessage(LOG_ERROR_LEVEL, "  file_test: rw_count = %" PRIu32 "\n", i);
		return false;
	}

	stop = get_ms();
	diff = stop - start;

	size_bytes = rw_size * rw_count;
	// size_bytes = (size_bytes * 1000) / 1024; // put in KB
	size_bytes = (size_bytes * 1000) / (1024 * 1024); // put in MB
	xbps = (size_bytes) / (diff + 1);
	logMessage(FS_LOG_LEVEL, "  write time: %d ms\n", (int)diff);
	logMessage(FS_LOG_LEVEL, "  write speed: %" PRIu32 " MB/s\n", xbps);
	printf_io_timings(diff);
	r = ext4_fclose(&f);

	r = ext4_fopen(&f, "/test1", "r+");
	if (r != EOK) {
		logMessage(FS_LOG_LEVEL, "ext4_fopen ERROR = %d\n", r);
		return false;
	}

	logMessage(FS_LOG_LEVEL, "ext4_read: %" PRIu32 " * %" PRIu32 " ...\n",
			   rw_size, rw_count);

	io_timings_clear();
	start = get_ms();

	for (i = 0; i < rw_count; ++i) {
		r = ext4_fread(&f, rw_buff, rw_size, &size);

		if ((r != EOK) || (size != rw_size))
			break;

		if (verify_buf(rw_buff, rw_size, i % 10 + '0'))
			break;
	}

	if (i != rw_count) {
		logMessage(LOG_ERROR_LEVEL, "  file_test: rw_count = %" PRIu32 "\n", i);
		return false;
	}

	stop = get_ms();
	diff = stop - start;

	size_bytes = rw_size * rw_count;
	// size_bytes = (size_bytes * 1000) / 1024; // put in KB
	size_bytes = (size_bytes * 1000) / (1024 * 1024); // put in MB
	xbps = (size_bytes) / (diff + 1);
	logMessage(FS_LOG_LEVEL, "  read time: %d ms\n", (int)diff);
	logMessage(FS_LOG_LEVEL, "  read speed: %d MB/s\n", (int)xbps);
	printf_io_timings(diff);

	r = ext4_fclose(&f);
	return true;
}

static void test_lwext4_cleanup(void) {
	long int start;
	long int stop;
	long int diff;
	int r;

	logMessage(FS_LOG_LEVEL, "cleanup:\n");
	r = ext4_fremove("/hello.txt");
	if (r != EOK && r != ENOENT) {
		logMessage(LOG_ERROR_LEVEL, "ext4_fremove error: rc = %d\n", r);
	}

	logMessage(FS_LOG_LEVEL, "remove /test1\n");
	r = ext4_fremove("/test1");
	if (r != EOK && r != ENOENT) {
		logMessage(LOG_ERROR_LEVEL, "ext4_fremove error: rc = %d\n", r);
	}

	logMessage(FS_LOG_LEVEL, "remove /dir1\n");
	io_timings_clear();
	start = get_ms();
	r = ext4_dir_rm("/dir1");
	if (r != EOK && r != ENOENT) {
		logMessage(LOG_ERROR_LEVEL, "ext4_fremove ext4_dir_rm: rc = %d\n", r);
	}
	stop = get_ms();
	diff = stop - start;
	logMessage(FS_LOG_LEVEL, "cleanup: time: %d ms\n", (int)diff);
	printf_io_timings(diff);
}

// static bool test_lwext4_mount(struct ext4_blockdev *bdev,
// 							  struct ext4_bcache *bcache) {
static bool test_lwext4_mount(struct ext4_blockdev *bdev) {
	int r;

	// bc = bcache;

	// QB: dont know why this is here, since bd is already accessible globally,
	// and to keep things local it should really just use bdev directly.
	// bd = bdev;

	if (!bdev) {
		logMessage(LOG_ERROR_LEVEL, "test_lwext4_mount: no block device\n");
		return false;
	}

	ext4_dmask_set(DEBUG_ALL);

	r = ext4_device_register(bdev, "ext4_fs");
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_device_register: rc = %d\n", r);
		return false;
	}

	r = ext4_mount("ext4_fs", "/", false, UTIL_CACHE_MAX_SZ);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_mount: rc = %d\n", r);
		return false;
	}

	r = ext4_recover("/");
	if (r != EOK && r != ENOTSUP) {
		logMessage(LOG_ERROR_LEVEL, "ext4_recover: rc = %d\n", r);
		return false;
	}

	r = ext4_journal_start("/");
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_journal_start: rc = %d\n", r);
		return false;
	}

	ext4_cache_write_back("/", 1);
	return true;
}

static bool test_lwext4_umount(void) {
	int r;

	ext4_cache_write_back("/", 0);

	r = ext4_journal_stop("/");
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_journal_stop: fail %d", r);
		return false;
	}

	r = ext4_umount("/");
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_umount: fail %d", r);
		return false;
	}
	return true;
}

/**
 * @brief This is to init the bdev for the block I/O part of the unit test.
 */
int init_bfs_core_ext4_blk_test() {
	if (!open_filedev()) {
		logMessage(LOG_ERROR_LEVEL, "open_filedev error\n");
		return EXIT_FAILURE;
	}

	// so that we can open/close bdev
	status = FORMATTING;

	seen = new std::vector<bfs_vbid_t>();

	// for block unit test, we dont use ext4 code so we need to explicitly open
	// the bdev
	if (file_dev_open(bd) != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to open file device\n");
		return BFS_FAILURE;
	}

	// setup the global par hash buf here for block reads/writes
	curr_par = (uint8_t *)calloc(BfsFsLayer::get_SA()->getKey()->getHMACsize(),
								 sizeof(uint8_t));

	if (!curr_par) {
		logMessage(LOG_ERROR_LEVEL, "Failed to allocate memory for curr_par\n");
		return BFS_FAILURE;
	}

	// if (bfsUtilLayer::use_mt()) {
	// 	if (BfsFsLayer::init_merkle_tree(true) != BFS_SUCCESS) {
	// 		logMessage(LOG_ERROR_LEVEL, "Failed initializing merkle tree\n");
	// 		return BFS_FAILURE;
	// 	}
	// 	logMessage(LOG_ERROR_LEVEL, "Merkle tree initialized");
	// }

	return BFS_SUCCESS;
}

int fini_bfs_core_ext4_blk_test() {
	// if (bfsUtilLayer::use_mt()) {
	// 	if (BfsFsLayer::flush_merkle_tree() != BFS_SUCCESS) {
	// 		logMessage(LOG_ERROR_LEVEL, "Failed flushing merkle tree\n");
	// 		return BFS_FAILURE;
	// 	}
	// 	logMessage(FS_LOG_LEVEL, "Merkle tree flushed");
	// }

	// for block unit test, we dont use ext4 code so we need to explicitly close
	// the bdev
	if (file_dev_close(bd) != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to close file device\n");
		return BFS_FAILURE;
	}

	bd = NULL;

	delete seen;
	free(curr_par);
	curr_par = NULL;

	// so that we are done opening/closing bdev
	status = UNINITIALIZED;

	return BFS_SUCCESS;
}

static int update_merkle_tree(bfs_vbid_t vbid_start, uint32_t nvbids,
							  uint8_t **macs) {
	const merkle_tree_t &mt = BfsFsLayer::get_mt();

	if (!mt.nodes[0].hash)
		return BFS_FAILURE;

	bfs_vbid_t max_vbid = vbid_start + nvbids - 1;
	bfs_vbid_t i = 0;

	// first change out all the leaf hashes
	for (uint32_t j = 0; j < nvbids; j++) {
		// find the tree node index of each vbid
		i = vbid_start + j + ((1 << mt.height) - 1);

		if (!mt.nodes[i].hash)
			return BFS_FAILURE;

		free(mt.nodes[i].hash);
		mt.nodes[i].hash = macs[j];

		// track the max so we know where to start iterating from when updating
		// the actual tree
		// if ((vbid_start + j) > max_vbid)
		// 	max_vbid = vbid_start + j;
	}
	assert(i == max_vbid + ((1 << mt.height) - 1));

	// As an optimization, figure out exactly which internal/parent nodes need
	// to be updated now to avoid having to recompute the hashes for all the
	// nodes in the tree. To do this, we first find all the unique internal
	// nodes that need to be updated across all vbids, sort them, then
	// update them from highest index to lowest index (root node should thus be
	// last).
	std::set<bfs_vbid_t> unique_nodes;
	// bfs_vbid_t **to_update =
	// 	(bfs_vbid_t **)calloc(nvbids, sizeof(bfs_vbid_t *));
	bfs_vbid_t ii = 0;
	for (uint32_t j = 0; j < nvbids; j++) {
		// compute the leaf index for block j
		ii = vbid_start + j + ((1 << mt.height) - 1);

		// compute the block's parent index
		if (ii % 2 == 0)
			ii = (ii - 2) / 2;
		else
			ii = (ii - 1) / 2;

		// add all ancestors to the update list
		while (1) {
			unique_nodes.insert(ii);

			// stop if at root
			if (ii == 0)
				break;

			if (ii % 2 == 0)
				ii = (ii - 2) / 2;
			else
				ii = (ii - 1) / 2;
		}
	}
	// std::sort(unique_nodes.begin(), unique_nodes.end(),
	// 		  std::greater<bfs_vbid_t>());

	// iterate over the unique nodes in reverse order
	for (auto it = unique_nodes.rbegin(); it != unique_nodes.rend(); ++it) {
		// for (auto it = unique_nodes.begin(); it != unique_nodes.end(); ++it)
		// {
		if (BfsFsLayer::hash_node(*it, mt.nodes[*it].hash) != BFS_SUCCESS)
			return BFS_FAILURE;
	}

	// // Now start updating from i (max_vbid) -- ie do the batch update after
	// // we updated all the leaf hashes, starting at the bottom-right-most
	// parent
	// // who needs an update.
	// if (i % 2 == 0)
	// 	i = (i - 2) / 2;
	// else
	// 	i = (i - 1) / 2;

	// while (1) {
	// 	if (BfsFsLayer::hash_node(i, mt.nodes[i].hash) !=
	// 		BFS_SUCCESS)
	// 		return BFS_FAILURE;

	// 	// stop if at root
	// 	if (i == 0)
	// 		break;

	// 	if (i % 2 == 0)
	// 		i = (i - 2) / 2;
	// 	else
	// 		i = (i - 1) / 2;
	// 	// i--;
	// }

	if (BfsFsLayer::save_root_hash() != BFS_SUCCESS)
		return BFS_FAILURE;

	return BFS_SUCCESS;
}

static int verify_mt(bfs_vbid_t vbid_start, uint32_t nvbids, uint8_t **macs) {
	// now do mt checks
	// iterate backwards and compute hashes for appropriate nodes up to
	// root; compare the result with mt.nodes[0]
	uint8_t **ih = new uint8_t *[nvbids];
	const merkle_tree_t &mt = BfsFsLayer::get_mt();

	if (!mt.nodes[0].hash) {
		logMessage(LOG_ERROR_LEVEL, "NULL root hash in read_blk");
		abort();
	}

	bfs_vbid_t max_vbid = vbid_start + nvbids - 1;
	bfs_vbid_t i = 0;
	for (uint32_t j = 0; j < nvbids; j++) {
		// find the tree node index of each vbid
		i = vbid_start + j + ((1 << mt.height) - 1);

		if (!mt.nodes[i].hash)
			return BFS_FAILURE;

		ih[j] = mt.nodes[i].hash;
		mt.nodes[i].hash = macs[j];
	}
	assert(i == max_vbid + ((1 << mt.height) - 1));

	std::set<bfs_vbid_t> unique_nodes;
	bfs_vbid_t ii = 0;
	for (uint32_t j = 0; j < nvbids; j++) {
		// compute the leaf index for block j
		ii = vbid_start + j + ((1 << mt.height) - 1);

		// compute the block's parent index
		if (ii % 2 == 0)
			ii = (ii - 2) / 2;
		else
			ii = (ii - 1) / 2;

		// add all ancestors to the update list
		while (1) {
			unique_nodes.insert(ii);

			// stop if at root
			if (ii == 0)
				break;

			if (ii % 2 == 0)
				ii = (ii - 2) / 2;
			else
				ii = (ii - 1) / 2;
		}
	}
	// std::sort(unique_nodes.begin(), unique_nodes.end(),
	// 		  std::greater<bfs_vbid_t>());

	int hash_sz = BfsFsLayer::get_SA()->getKey()->getHMACsize();
	bfs_vbid_t cnt = 0;
	for (auto it = unique_nodes.rbegin(); it != unique_nodes.rend(); ++it) {
		// for (auto it = unique_nodes.begin(); it != unique_nodes.end(); ++it)
		// { For debugging, this does a fast return: if vbid is less than a
		// direct-parent index, we have successfully memcpd'd all direct-parents
		// and therefore can conclude that the tree is valid.
		// if (*it < (1 << mt.height) - 1)
		// 	cnt++;
		// if (cnt == 1)
		// 	break;
		// if (cnt > 0)
		// 	break;
		if (*it < (1 << mt.height - 1) - 1)
			break;

		// Compute the new hash for node i based on the current block
		// data/hash that we just read, then compare to what was there
		// before (the previously trusted version)
		memcpy(curr_par, mt.nodes[*it].hash, hash_sz);

		// these may be dependent on the new vbid so just always
		// recompute them (TODO: optimize later)
		if (BfsFsLayer::hash_node(*it, mt.nodes[*it].hash) != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL, "Failed hash_node in read_blk");
			abort();
		}

		// check the new computed parent against the current parent to do an
		// early return (caching the MT in memory, whether entirely or
		// sparsely, enables this)
		if (memcmp(mt.nodes[*it].hash, curr_par, hash_sz) != 0) {
			char utstr[129];
			bufToString((const char *)curr_par, hash_sz, utstr, 128);
			logMessage(LOG_ERROR_LEVEL, "curr par hash: [%s]", utstr);
			bufToString((const char *)mt.nodes[*it].hash, hash_sz, utstr, 128);
			logMessage(LOG_ERROR_LEVEL, "computed par hash: [%s]", utstr);

			// Just abort and dont deal with free'ing pointers, because we'll
			// have to make sure that ownership is correct.
			logMessage(LOG_ERROR_LEVEL,
					   "Invalid par hash comparison in read_blk [blk_id=%lu, "
					   "blk_cnt=%d, i=%lu]",
					   vbid_start, nvbids, *it);
			abort();
		}
	}

	// if (i % 2 == 0)
	// 	i = (i - 2) / 2;
	// else
	// 	i = (i - 1) / 2;

	// // int hash_sz = BfsFsLayer::get_SA()->getKey()->getHMACsize();
	// // uint8_t *curr_par = (uint8_t *)calloc(hash_sz, sizeof(uint8_t));
	// int hash_sz = BfsFsLayer::get_SA()->getKey()->getHMACsize();
	// bfs_vbid_t cnt = 0;
	// while (1) {
	// 	// Compute the new hash for node i based on the current block
	// 	// data/hash that we just read, then compare to what was there
	// 	// before (the previously trusted version)
	// 	memcpy(curr_par, mt.nodes[i].hash, hash_sz);

	// 	// these may be dependent on the new vbid so just always
	// 	// recompute them (TODO: optimize later)
	// 	if (BfsFsLayer::hash_node(i, mt.nodes[i].hash) !=
	// 		BFS_SUCCESS) {
	// 		logMessage(LOG_ERROR_LEVEL, "Failed hash_node in read_blk");
	// 		abort();
	// 	}

	// 	// check the new computed parent against the current parent to do an
	// 	// early return (caching the MT in memory, whether entirely or
	// 	// sparsely, enables this)
	// 	if (memcmp(mt.nodes[i].hash, curr_par, hash_sz) !=
	// 		0) {
	// 		char utstr[129];
	// 		bufToString((const char *)curr_par, hash_sz, utstr, 128);
	// 		logMessage(LOG_ERROR_LEVEL, "curr par hash: [%s]", utstr);
	// 		bufToString((const char *)mt.nodes[i].hash,
	// 					hash_sz, utstr, 128);
	// 		logMessage(LOG_ERROR_LEVEL, "computed par hash: [%s]", utstr);

	// 		// Just abort and dont deal with free'ing pointers, because we'll
	// 		// have to make sure that ownership is correct.
	// 		logMessage(LOG_ERROR_LEVEL,
	// 				   "Invalid par hash comparison in read_blk [blk_id=%lu, "
	// 				   "blk_cnt=%d, i=%lu]",
	// 				   vbid_start, nvbids, i);
	// 		abort();

	// 		// free(mt
	// 		// 		 .nodes[i]
	// 		// 		 .hash); // free the newly computed value
	// 		// mt.nodes[i].hash =
	// 		// 	curr_par; // swap in the old value

	// 		// free(mt
	// 		// 		 .nodes[(vbid + (1 << mt.height) - 1)]
	// 		// 		 .hash); // free mac_copy
	// 		// mt
	// 		// 	.nodes[(vbid + (1 << mt.height) - 1)]
	// 		// 	.hash = ih; // swap in the old value
	// 	}

	// 	// For debugging, this does a fast return
	// 	// cnt++;
	// 	// if (cnt == 1)
	// 	// 	break;
	// 	// if (cnt > 0)
	// 	// 	break;

	// 	// stop if at root
	// 	if (i == 0)
	// 		break;

	// 	// if (i % 2 == 0)
	// 	// 	i = (i - 2) / 2;
	// 	// else
	// 	// 	i = (i - 1) / 2;
	// 	i--;
	// }

	// cleanup a bit
	// for (uint32_t j = 0; j < nvbids; j++)
	// 	free(ih[j]);
	delete[] ih;
	ih = NULL;

	return BFS_SUCCESS;
}

/**
 * @brief For lwext4, this takes the blk from the bfs_server or unit test and
 * puts it directly on disk (bypassing virt block cluster). Note that it
 * translates the vbid directly into the pbid.
 */
int __do_file_dev_bwrite(const void *blk_writer) {
	if (!BfsFsLayer::get_SA())
		return BFS_FAILURE;

	VBfsBlock &blk = *((VBfsBlock *)blk_writer);

	// if ((blk.get_vbid() >= METADATA_REL_START_BLK_NUM) &&
	// 	(blk.get_vbid() < DATA_REL_START_BLK_NUM)) {
	// 	logMessage(LOG_ERROR_LEVEL, "Trying to write to meta block directly");
	// 	return BFS_FAILURE;
	// }

	// uint8_t *mac_copy = NULL, *iv = NULL;
	// // if (bfsUtilLayer::use_mt() && (status == MOUNTED)) {
	// // if (status != CORRUPTED) {
	// mac_copy = (uint8_t
	// *)calloc(BfsFsLayer::get_SA()->getKey()->getMACsize(),
	// sizeof(uint8_t)); iv = (uint8_t
	// *)calloc(BfsFsLayer::get_SA()->getKey()->getIVlen(),
	// sizeof(uint8_t));

	// // encrypt and add MAC tag
	// // The buffer should contain the IV (12 bytes) + data (4064) + MAC (16
	// // bytes) and should fit within the device block size.
	// bfs_vbid_t vbid = blk.get_vbid();
	// bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer();
	// aad->resizeAllocation(0, sizeof(vbid), 0);
	// *aad << vbid;
	// BfsFsLayer::get_SA()->encryptData2(blk, aad, &iv, &mac_copy);
	// delete aad;

	// uint8_t *mac_copy;
	// if (bfsUtilLayer::use_mt() && (status != CORRUPTED)) {
	// 	mac_copy = (uint8_t *)calloc(
	// 		BfsFsLayer::get_SA()->getKey()->getMACsize(), sizeof(uint8_t));
	// 	// copy over before block is prepared to be written to disk/network
	// 	memcpy(mac_copy, &(blk.getBuffer()[BFS_IV_LEN + BLK_SZ + PKCS_PAD_SZ]),
	// 		   BfsFsLayer::get_SA()->getKey()->getMACsize());
	// }

	// int32_t z = 0;
	// blk.addTrailer((char *)&z, sizeof(z));

	// assert(blk.getLength() == BLK_SZ);

	// write meta first
	// if (BfsFsLayer::write_blk_meta(blk.get_vbid(), &iv, &mac_copy) !=
	// 	BFS_SUCCESS) {
	// 	free(mac_copy);
	// 	free(iv);
	// 	logMessage(LOG_ERROR_LEVEL, "Failed writing security metadata");
	// 	return BFS_FAILURE;
	// }
	// free(iv);

	// now do write
	int r = file_dev_bwrite(NULL, blk.getBuffer(), blk.get_vbid(), 1);
	if (r != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to write to file device\n");
		return r;
	}

	// // now do mt updates
	// if (update_merkle_tree(vbid, mac_copy) != BFS_SUCCESS) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to update merkle tree\n");
	// 	return BFS_FAILURE;
	// }

	return BFS_SUCCESS;
}

int __do_file_dev_bread(void *blk_reader) {
	if (!BfsFsLayer::get_SA())
		return BFS_FAILURE;

	VBfsBlock &blk = *((VBfsBlock *)blk_reader);

	// if ((blk.get_vbid() >= METADATA_REL_START_BLK_NUM) &&
	// 	(blk.get_vbid() < DATA_REL_START_BLK_NUM)) {
	// 	logMessage(LOG_ERROR_LEVEL, "Trying to write to meta block directly");
	// 	return BFS_FAILURE;
	// }

	// first read raw block
	int r = file_dev_bread(NULL, blk.getBuffer(), blk.get_vbid(), 1);
	if (r != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to read to file device\n");
		return r;
	}

	// now decrypt and get plaintext
	// int32_t z = -1;
	// blk.removeTrailer((char *)&z, sizeof(z));

	// if (!BfsFsLayer::get_SA())
	// 	return BFS_FAILURE;

	// if (status < FORMATTED) {
	// 	logMessage(LOG_ERROR_LEVEL,
	// 			   "Failed read_blk, filesystem not formatted");
	// 	return BFS_FAILURE;
	// }

	// Note: here we use MAC/GMAC size (16B hash) for the block but check the
	// HMAC (32B hash) of the root below
	// uint8_t *const mac_copy = (uint8_t *)calloc(
	// 	BfsFsLayer::get_SA()->getKey()->getMACsize(), sizeof(uint8_t));
	// uint8_t *mac_copy, *iv;
	// mac_copy = (uint8_t
	// *)calloc(BfsFsLayer::get_SA()->getKey()->getMACsize(),
	// sizeof(uint8_t)); iv = (uint8_t
	// *)calloc(BfsFsLayer::get_SA()->getKey()->getIVlen(),
	// sizeof(uint8_t)); if (BfsFsLayer::read_blk_meta(blk.get_vbid(), &iv,
	// &mac_copy) != 	BFS_SUCCESS) { 	free(mac_copy); 	free(iv); 	throw
	// BfsServerError("Failed reading security metadata MAC", NULL,
	// NULL);
	// }

	// // decrypt and verify MAC
	// bfs_vbid_t vbid = blk.get_vbid();
	// bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer();
	// aad->resizeAllocation(0, sizeof(vbid), 0);
	// *aad << vbid;
	// BfsFsLayer::get_SA()->decryptData2(blk, aad, iv, mac_copy);
	// delete aad;

	// // validate that correct amount of data read, then copy over
	// assert(blk.getLength() == BLK_SZ);

	// // now do mt checks
	// if (verify_mt(vbid, mac_copy) != BFS_SUCCESS) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to check merkle tree\n");
	// 	return BFS_FAILURE;
	// }

	return BFS_SUCCESS;
}

/**
 * @brief This is the lwext4 method used by the bfs core blk unit test when
 * using the lwext4 backend.
 *
 * @return int 0 if success, -1 if failure
 */
int run_bfs_core_ext4_file_test(void) {
	int r;
	// if (!parse_opt(argc, argv))
	// 	return EXIT_FAILURE;

	logMessage(FS_LOG_LEVEL, "ext4_generic\n");
	logMessage(FS_LOG_LEVEL, "\tinput name: %s\n", input_name);

	if (!open_filedev()) {
		logMessage(LOG_ERROR_LEVEL, "open_filedev error\n");
		return EXIT_FAILURE;
	}

	if (verbose)
		ext4_dmask_set(DEBUG_ALL);

	seen = new std::vector<bfs_vbid_t>();

	curr_par = (uint8_t *)calloc(BfsFsLayer::get_SA()->getKey()->getHMACsize(),
								 sizeof(uint8_t));
	if (!curr_par) {
		logMessage(LOG_ERROR_LEVEL, "Failed to allocate memory for curr_par\n");
		return BFS_FAILURE;
	}

	// do a mkfs (this will init the merkle tree then flush it)
	logMessage(FS_LOG_LEVEL, "ext4_mkfs: ext%d\n", fs_type);
	status = FORMATTING;
	info.journal = bfsUtilLayer::journal_enabled();
	r = ext4_mkfs(&fs, bd, &info, fs_type, UTIL_CACHE_MAX_SZ);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_mkfs error: %d\n", r);
		return EXIT_FAILURE;
	}

	// read the fs info (this will init the merkle tree then flush it)
	memset(&info, 0, sizeof(struct ext4_mkfs_info));
	r = ext4_mkfs_read_info(bd, &info);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_mkfs_read_info error: %d\n", r);
		return EXIT_FAILURE;
	}
	status = FORMATTED;

	logMessage(FS_LOG_LEVEL, "Created filesystem with parameters:\n");
	logMessage(FS_LOG_LEVEL, "Size: %" PRIu64 "\n", info.len);
	logMessage(FS_LOG_LEVEL, "Block size: %" PRIu32 "\n", info.block_size);
	logMessage(FS_LOG_LEVEL, "Blocks per group: %" PRIu32 "\n",
			   info.blocks_per_group);
	logMessage(FS_LOG_LEVEL, "Inodes per group: %" PRIu32 "\n",
			   info.inodes_per_group);
	logMessage(FS_LOG_LEVEL, "Inode size: %" PRIu32 "\n", info.inode_size);
	logMessage(FS_LOG_LEVEL, "Inodes: %" PRIu32 "\n", info.inodes);
	logMessage(LOG_ERROR_LEVEL, "journal flag on ext4_mkfs_read_info: %s",
			   info.journal ? "true" : "false");
	logMessage(FS_LOG_LEVEL, "Journal blocks: %" PRIu32 "\n",
			   info.journal_blocks);
	logMessage(FS_LOG_LEVEL, "Features ro_compat: 0x%x\n", info.feat_ro_compat);
	logMessage(FS_LOG_LEVEL, "Features compat: 0x%x\n", info.feat_compat);
	logMessage(FS_LOG_LEVEL, "Features incompat: 0x%x\n", info.feat_incompat);
	logMessage(FS_LOG_LEVEL, "BG desc reserve: %" PRIu32 "\n",
			   info.bg_desc_reserve_blocks);
	logMessage(FS_LOG_LEVEL, "Descriptor size: %" PRIu32 "\n", info.dsc_size);
	logMessage(FS_LOG_LEVEL, "Label: %s\n", info.label);

	logMessage(FS_LOG_LEVEL, "Done mkfs\n");

	// now do the tests (these will generally init the merkle tree then flush it
	// when done)
	// if (!test_lwext4_mount(bd, bc))
	if (!test_lwext4_mount(bd))
		return EXIT_FAILURE;
	status = MOUNTED;

	test_lwext4_cleanup();

	// if (sbstat)
	// 	test_lwext4_mp_stats();

	test_lwext4_dir_ls("/");
	// fflush(stdout);
	if (!test_lwext4_dir_test(dir_cnt))
		return EXIT_FAILURE;

	// fflush(stdout);
	uint8_t *rw_buff = (uint8_t *)malloc(rw_szie);
	if (!rw_buff) {
		free(rw_buff);
		return EXIT_FAILURE;
	}
	if (!test_lwext4_file_test(rw_buff, rw_szie, rw_count)) {
		free(rw_buff);
		return EXIT_FAILURE;
	}

	// free(rw_buff);

	// fflush(stdout);
	// test_lwext4_dir_ls("/");

	// if (sbstat)
	// 	test_lwext4_mp_stats();

	// if (cleanup_flag)
	test_lwext4_cleanup();

	// if (bstat)
	// 	test_lwext4_block_stats();

	if (!test_lwext4_umount())
		return EXIT_FAILURE;
	status = UNINITIALIZED;

	// printf("\ntest finished\n");
	// return EXIT_SUCCESS;

	delete seen;
	free(curr_par);
	curr_par = NULL;

	return BFS_SUCCESS;
}

/**
 * Main fops methods for lwext4 backend.
 */

/**
 * Note on concurrency in this source file: The only shared object we deal with
 * is the open_file_tab list, which we must control concurrent
 * insertions/deletions/gets using the open_file_tab_mux. Generally we dont deal
 * with concurrent writers on files (same semantics as NFS), but concurrent
 * readers should be fine. In this case, they may end up retrieving similar
 * inodes during path traversal (ie every client will always try to read/stat
 * the root inode). The lwext4 code currently deals with this simply by
 * serializing individual operations with a global mp lock (mp_mux, which is
 * initialized during init and setup during mount). This is conservative and
 * ideally we would use per-inode locks. However, for now this may allow some
 * operations (that don't require locking) to progress (e.g., copying buffers
 * in/out of TEE) and therefore provide some performance improvements. If no
 * perf improvements seen there, then the bottleneck is likely the fs code and
 * not sgx mechanisms then, which I'm not sure how we can really deal with now
 * other than using caching.
 */
static void _lock(void) { __lock(&mp_mux); }

static void _unlock(void) { __unlock(&mp_mux); }

static void __lock(pthread_mutex_t *m) {
	if (!m) {
		logMessage(LOG_ERROR_LEVEL, "Mutex is NULL\n");
		abort();
	}

	if (pthread_mutex_lock(m) != 0) {
		logMessage(LOG_ERROR_LEVEL, "Failed to lock mutex\n");
		abort();
	}

	// logMessage(FS_LOG_LEVEL, "lock(%p) OK\n", m);
}

static void __unlock(pthread_mutex_t *m) {
	if (!m) {
		logMessage(LOG_ERROR_LEVEL, "Mutex is NULL\n");
		abort();
	}

	if (pthread_mutex_unlock(m) != 0) {
		logMessage(LOG_ERROR_LEVEL, "Failed to unlock mutex\n");
		abort();
	}

	// logMessage(FS_LOG_LEVEL, "unlock(%p) OK\n", m);
}

/**
 * @brief This is the lwext4 _init_ method used by the bfs server when using
 * the lwext4 backend.
 *
 * @return int 0 if success, -1 if failure
 */
int __do_lwext4_init(void *testing) {
	logMessage(FS_LOG_LEVEL, "ext4_generic\n");
	logMessage(FS_LOG_LEVEL, "\tinput name: %s\n", input_name);

	// sets the global bd variable (pointing to file_dev)
	if (!open_filedev()) {
		logMessage(LOG_ERROR_LEVEL, "open_filedev error\n");
		return BFS_FAILURE;
	}

	if (verbose)
		ext4_dmask_set(DEBUG_ALL);

	if (testing)
		blk_accesses = (std::vector<bfs_vbid_t> *)testing;

	seen = new std::vector<bfs_vbid_t>();
	open_file_tab = new std::map<bfs_vbid_t, bfs_lwext4_open_file_t *>();
	curr_par = (uint8_t *)calloc(BfsFsLayer::get_SA()->getKey()->getHMACsize(),
								 sizeof(uint8_t));
	next_fh = START_FD;
	next_dfh = 1e6 + START_FD;
	if (pthread_mutex_init(&open_file_tab_mux, NULL) != 0)
		abort();
	if (pthread_mutex_init(&mp_mux, NULL) != 0)
		abort();

	status = INITIALIZED;

	logMessage(FS_LOG_LEVEL, "Done do_init_lwext4");

	return BFS_SUCCESS;
}

int __do_lwext4_mkfs(void) {
	int r;

	// For simplicity, we don't allow mkfs to be called multiple times when the
	// server is running. So status should be UNINITIALIZED (ie unformatted)
	// before we try mkfs (and open bdev).
	// if ((status < INITIALIZED)) {
	if (status != INITIALIZED) {
		logMessage(LOG_ERROR_LEVEL, "FS in bad state in mkfs [%d]\n", status);
		return BFS_FAILURE;
	}

	status = FORMATTING;

	logMessage(FS_LOG_LEVEL, "ext4_mkfs: ext%d\n", fs_type);

	// if (bfsUtilLayer::use_mt()) {
	// 	if (status < INITIALIZED) {
	// 		if (BfsFsLayer::init_merkle_tree(true) != BFS_SUCCESS) {
	// 			logMessage(LOG_ERROR_LEVEL,
	// 					   "Failed initializing merkle tree\n");
	// 			return BFS_FAILURE;
	// 		}
	// 	} else {
	// 		logMessage(LOG_ERROR_LEVEL, "FS in bad state in mkfs 1\n");
	// 		return BFS_FAILURE;
	// 	}
	// 	logMessage(LOG_ERROR_LEVEL, "Merkle tree initialized");
	// }

	// do a mkfs (this will init the merkle tree then flush it)
	info.journal = bfsUtilLayer::journal_enabled();
	logMessage(LOG_ERROR_LEVEL, "set journal flag to: %s",
			   info.journal ? "true" : "false");
	r = ext4_mkfs(&fs, bd, &info, fs_type, UTIL_CACHE_MAX_SZ);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_mkfs error: %d\n", r);
		return BFS_FAILURE;
	}

	logMessage(FS_LOG_LEVEL, "(mkfs) CONFIG_BLOCK_DEV_CACHE_SIZE: %d\n",
			   CONFIG_BLOCK_DEV_CACHE_SIZE);
	logMessage(FS_LOG_LEVEL, "(mkfs) UTIL_CACHE_MAX_SZ: %d\n",
			   UTIL_CACHE_MAX_SZ);

	// read the fs info (this will init the merkle tree then flush it)
	memset(&info, 0, sizeof(struct ext4_mkfs_info));
	r = ext4_mkfs_read_info(bd, &info);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_mkfs_read_info error: %d\n", r);
		return BFS_FAILURE;
	}

	status = FORMATTED;
	logMessage(FS_LOG_LEVEL, "MKFS done");

	logMessage(FS_LOG_LEVEL, "Created filesystem with parameters:\n");
	logMessage(FS_LOG_LEVEL, "Size: %" PRIu64 "\n", info.len);
	logMessage(FS_LOG_LEVEL, "Block size: %" PRIu32 "\n", info.block_size);
	logMessage(FS_LOG_LEVEL, "Blocks per group: %" PRIu32 "\n",
			   info.blocks_per_group);
	logMessage(FS_LOG_LEVEL, "Inodes per group: %" PRIu32 "\n",
			   info.inodes_per_group);
	logMessage(FS_LOG_LEVEL, "Inode size: %" PRIu32 "\n", info.inode_size);
	logMessage(FS_LOG_LEVEL, "Inodes: %" PRIu32 "\n", info.inodes);
	logMessage(LOG_ERROR_LEVEL, "journal flag on ext4_mkfs_read_info: %s",
			   info.journal ? "true" : "false");
	logMessage(FS_LOG_LEVEL, "Journal blocks: %" PRIu32 "\n",
			   info.journal_blocks);
	logMessage(FS_LOG_LEVEL, "Features ro_compat: 0x%x\n", info.feat_ro_compat);
	logMessage(FS_LOG_LEVEL, "Features compat: 0x%x\n", info.feat_compat);
	logMessage(FS_LOG_LEVEL, "Features incompat: 0x%x\n", info.feat_incompat);
	logMessage(FS_LOG_LEVEL, "BG desc reserve: %" PRIu32 "\n",
			   info.bg_desc_reserve_blocks);
	logMessage(FS_LOG_LEVEL, "Descriptor size: %" PRIu32 "\n", info.dsc_size);
	logMessage(FS_LOG_LEVEL, "Label: %s\n", info.label);

	// if (bfsUtilLayer::use_mt()) {
	// 	if (status < INITIALIZED) {
	// 		if (BfsFsLayer::flush_merkle_tree() != BFS_SUCCESS) {
	// 			logMessage(LOG_ERROR_LEVEL, "Failed flushing merkle tree\n");
	// 			return BFS_FAILURE;
	// 		}
	// 	} else {
	// 		logMessage(LOG_ERROR_LEVEL, "FS in bad state in mkfs 2\n");
	// 		return BFS_FAILURE;
	// 	}
	// 	logMessage(FS_LOG_LEVEL, "Merkle tree flushed");
	// }

	return BFS_SUCCESS;
}

/**
 * @brief This is the lwext4 _mount_ method used by the bfs server when using
 * the lwext4 backend. Mostly the same as test_lwext4_mount(), except we remove
 * the redundant variable setting and other stuff we dont need.
 *
 * @return int 0 if success, -1 if failure
 */
int __do_lwext4_mount(void) {
	int r;

	// For simplicity, we don't allow mount to be called multiple times when the
	// server is running. Multiple clients are handled by bfs_server_ecalls
	// differently. So status should be INITIALIZED. If it somehow gets here, we
	// should fail immediately to check the issue.
	if (status != FORMATTED) {
		logMessage(LOG_ERROR_LEVEL, "FS in bad state in mount [%d]\n", status);
		return BFS_FAILURE;
	}
	// While ext4_mount will normally open the block dev, here we just open it
	// first, which will set the ph_refctr to 1 (which is fine), so that we can
	// init the mt before the mount call starts reading/writing blocks. This is
	// a temp workaround to avoid having to change the ext4_mount code.
	// if (!open_filedev()) {
	// 	logMessage(LOG_ERROR_LEVEL, "open_filedev error\n");
	// 	return BFS_FAILURE;
	// }
	// if (file_dev_open(bd) != BFS_SUCCESS) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to open file device\n");
	// 	return BFS_FAILURE;
	// }
	// if (bfsUtilLayer::use_mt()) {
	// 	if (BfsFsLayer::init_merkle_tree() != BFS_SUCCESS) {
	// 		logMessage(LOG_ERROR_LEVEL, "Failed initializing merkle tree\n");
	// 		return BFS_FAILURE;
	// 	}
	// 	logMessage(LOG_ERROR_LEVEL, "Merkle tree initialized");
	// }

	ext4_dmask_set(DEBUG_ALL);

	// Register this name with this (statically defined) block device
	r = ext4_device_register(bd, "ext4_fs");
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_device_register: rc = %d\n", r);
		return false;
	}

	// Mount the block device with the specified name under the target mp.
	// This also initializes a bc (inside the statically defined mp).
	// Ultimately, this binds [mp, fs, bc, and bd] to all point to each other.
	// This will init the merkle tree then flush it.
	r = ext4_mount("ext4_fs", "/", false, UTIL_CACHE_MAX_SZ);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_mount: rc = %d\n", r);
		return false;
	}

	logMessage(FS_LOG_LEVEL, "(mount) CONFIG_BLOCK_DEV_CACHE_SIZE: %d\n",
			   CONFIG_BLOCK_DEV_CACHE_SIZE);
	logMessage(FS_LOG_LEVEL, "(mount) UTIL_CACHE_MAX_SZ: %d\n",
			   UTIL_CACHE_MAX_SZ);

	// Now set status to mounted so that MT will not be initialized again for
	// the server.
	status = MOUNTED;

	r = ext4_mount_setup_locks("/", &mp_lock_handlers);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_mount_setup_locks: rc = %d\n", r);
		return false;
	}
	logMessage(FS_LOG_LEVEL, "(mount) locks setup\n");

	r = ext4_recover("/");
	if (r != EOK && r != ENOTSUP) {
		logMessage(LOG_ERROR_LEVEL, "ext4_recover: rc = %d\n", r);
		return false;
	}

	r = ext4_journal_start("/");
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_journal_start: rc = %d\n", r);
		return false;
	}

	ext4_cache_write_back("/", 1);

	logMessage(FS_LOG_LEVEL, "mount done");

	return BFS_SUCCESS;
}

/**
 * @brief Check that the file permissions are OK. This is similar to
 * what BFS currently does at the beginning of every fop method.
 *
 * @return int 0 if success, otherwise failure
 */
static int __check_perms(void *usr, const char *path, uint32_t *uid) {
	// use local uid/gid vars before filling in fields to send back to client
	uint32_t _gid = 0, _uid = 0;
	int r;
	BfsUserContext *_usr = (BfsUserContext *)usr;

	// if so, get uid and check permissions before continuing
	r = ext4_owner_get(path, &_uid, &_gid);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_owner_get ERROR = %d\n", r);
		return BFS_FAILURE;
	}

	/**
	 * For simplicity, let all perms checks pass for now, since we are not
	 * really managing a complex ACL structure yet.
	 */
	// if (!BfsACLayer::is_owner(_usr, _uid))
	// 	return -EPERM;

	if (uid)
		*uid = _uid;

	return BFS_SUCCESS;
}

static bfs_fh_t alloc_fh(int fh_type) {
	// __lock(&open_file_tab_mux); // dont need to lock, only called below
	if (open_file_tab->size() == MAX_OPEN_FILES) {
		logMessage(LOG_ERROR_LEVEL, "Too many files open\n");
		return 0;
	}
	// __unlock(&open_file_tab_mux);

	// use diff ranges so we can know how to release them quicker based on the
	// type
	if (fh_type == 0)
		return next_fh++;
	else if (fh_type == 1)
		return next_dfh++;

	return 0;
}

static bfs_lwext4_open_file_t *init_open_file(const char *path, void *f,
											  int fh_type) {
	__lock(&open_file_tab_mux);

	bfs_fh_t fh = alloc_fh(fh_type);
	if (!fh) {
		logMessage(LOG_ERROR_LEVEL, "Failed to allocate file handle\n");
		return NULL;
	}
	bfs_lwext4_open_file_t *of =
		(bfs_lwext4_open_file_t *)malloc(sizeof(bfs_lwext4_open_file_t));
	of->path = (char *)malloc(strlen(path) + 1);
	strncpy(of->path, path, strlen(path) + 1);
	of->f = f;
	of->fh = fh;
	open_file_tab->insert(std::make_pair(fh, of));

	__unlock(&open_file_tab_mux);

	return of;
}

static int del_open_file(bfs_fh_t fh) {
	__lock(&open_file_tab_mux);

	if (open_file_tab->find(fh) == open_file_tab->end()) {
		logMessage(LOG_ERROR_LEVEL, "File handle not found\n");
		return BFS_FAILURE;
	}
	bfs_lwext4_open_file_t *of = open_file_tab->at(fh);
	free(of->path);
	free(of->f);
	free(of);
	open_file_tab->erase(fh);

	__unlock(&open_file_tab_mux);

	return BFS_SUCCESS;
}

/**
 * @brief Update file access, modification, and change times. dmod and mmod
 * indicate whether there was a data change or metadata change, respectively.
 */
static int touch_file(const char *path, int dmod, int mmod, ext4_file *of) {
	// early ret if no times changed
	if (!dmod && !mmod)
		return BFS_SUCCESS;

	uint32_t curr_time = (uint32_t)get_s();
	uint32_t _m = dmod ? curr_time : 0;
	uint32_t _c = mmod ? curr_time : 0;

	// enable/disable for gramine comparison
	if (ext4_all_time_set(path, _m, _c, of)) {
		logMessage(LOG_ERROR_LEVEL, "touch_file call ERROR (line: %d)\n",
				   __LINE__);
		return BFS_FAILURE;
	}
	return BFS_SUCCESS;
}

/**
 * TODO:
 * - call __check_perms on all these methods
 * -
 * -
 */
double total_gettattr_time = 0.;
int __do_lwext4_getattr(void *usr, const char *path, uint32_t *uid,
						uint64_t *fino, uint32_t *fmode, uint64_t *fsize,
						uint32_t *atime, uint32_t *mtime, uint32_t *ctime) {
	// int r = __check_perms(usr, path, uid);
	// if (r != EOK) {
	// 	logMessage(LOG_ERROR_LEVEL, "__check_perms ERROR = %d\n", r);
	// 	return r;
	// }

	// Try to stat as regular or directory file.
	// Normally we would let ext4 code check for existence, but we need to call
	// diff methods based on the file type, so just check explicitly here.
	// int r = ext4_inode_exist(path, EXT4_DE_REG_FILE);
	// if (r != EOK) {
	// 	r = ext4_inode_exist(path, EXT4_DE_DIR);
	// 	if (r != EOK) {
	// 		// logMessage(LOG_ERROR_LEVEL,
	// 		// 		   "ext4_inode_exist ERROR in getattr on [path=%s] = %d\n",
	// 		// 		   path, r);
	// 		return r;
	// 	}

	// ext4_file f;
	// int r = ext4_generic_open2(&f, path, O_RDONLY, EXT4_DE_UNKNOWN, 0, 0);
	// if (r != EOK) {
	// 	logMessage(FS_LOG_LEVEL, "ext4_generic_open2 ERROR = %d\n", r);
	// 	return r;
	// }
	// *fino = f.inode;
	// *fsize = f.fsize;
	// r = ext4_mode_get(path, fmode, &f);
	// if (r != EOK) {
	// 	logMessage(LOG_ERROR_LEVEL, "ext4_mode_get ERROR = %d\n", r);
	// 	return r;
	// }

	// double getattr_start_time = __get_time();

	ext4_file f;
	int r = ext4_fopen(&f, path, "r");
	if (r != EOK) {
		ext4_dir d;
		r = ext4_dir_open(&d, path);
		if (r != EOK) {
			logMessage(FS_LOG_LEVEL, "ext4_fopen ERROR = %d\n", r);
			return r;
		}
		*fino = d.f.inode;
		*fsize = d.f.fsize;

		r = ext4_mode_get(path, fmode, &d.f);
		if (r != EOK) {
			logMessage(LOG_ERROR_LEVEL, "ext4_mode_get ERROR = %d\n", r);
			return r;
		}

		// enable/disable for gramine comparison
		if (ext4_all_time_get(path, atime, mtime, ctime, &d.f)) {
			logMessage(LOG_ERROR_LEVEL, "ERROR getting times = %d\n", r);
			return r;
		}
	} else {
		*fino = f.inode;
		*fsize = f.fsize;

		r = ext4_mode_get(path, fmode, &f);
		if (r != EOK) {
			logMessage(LOG_ERROR_LEVEL, "ext4_mode_get ERROR = %d\n", r);
			return r;
		}

		// enable/disable for gramine comparison
		if (ext4_all_time_get(path, atime, mtime, ctime, &f)) {
			logMessage(LOG_ERROR_LEVEL, "ERROR getting times = %d\n", r);
			return r;
		}
	}

	// double getattr_end_time = __get_time();
	// total_gettattr_time += (getattr_end_time - getattr_start_time);
	// }

	// if (ext4_atime_get(path, atime) || ext4_mtime_get(path, mtime) ||
	// 	ext4_ctime_get(path, ctime)) {
	// 	logMessage(LOG_ERROR_LEVEL, "ERROR getting times = %d\n", r);
	// 	return r;
	// }

	// now get mode
	// r = ext4_mode_get(path, fmode);
	// if (r != EOK) {
	// 	logMessage(LOG_ERROR_LEVEL, "ext4_mode_get ERROR = %d\n", r);
	// 	return r;
	// }
	// logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_getattr mode: %o", *fmode);

	// if (uid)
	// 	*uid = 1000;

	logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_getattr OK [path=%s]", path);
	// logMessage(FS_VRB_LOG_LEVEL,
	// 		   "__do_lwext4_getattr total_gettattr_time [%.3f s]",
	// 		   total_gettattr_time / 1e6);

	return BFS_SUCCESS;
}

int __do_lwext4_mkdir(void *usr, const char *path, uint32_t fmode) {
	ext4_file f;
	int r = ext4_dir_mk(path, &f);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_dir_mk: rc = %d\n", r);
		return r;
	}

	r = ext4_mode_set(path, fmode, &f);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_mode_set: rc = %d\n", r);
		return r;
	}

	if (touch_file(path, 1, 1, &f)) {
		logMessage(LOG_ERROR_LEVEL, "touch_file ERROR (line: %d)\n", __LINE__);
		return BFS_FAILURE;
	}

	logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_mkdir OK [path=%s]", path);

	return BFS_SUCCESS;
}

int __do_lwext4_unlink(void *usr, const char *path) {
	// The ext4 code should make sure ref counts are OK to remove, so we assume
	// here that the open_file_tab does not contain any entries for the file.
	int r = ext4_fremove(path);
	// if (r != EOK && r != ENOENT) {
	if (r != EOK) { // propogate ENOENT back to client
		logMessage(LOG_ERROR_LEVEL, "ext4_fremove error: rc = %d\n", r);
		return r;
	}

	logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_unlink OK [path=%s]", path);

	return BFS_SUCCESS;
}

int __do_lwext4_rename(void *usr, const char *fpath, const char *tpath) {
	// Rename the file in the open file table so that further operations on the
	// file will use the correct name. In the future, will have to make sure
	// this is multi-threading safe.
	bfs_lwext4_open_file_t *of = NULL;

	__lock(&open_file_tab_mux);
	for (auto it = open_file_tab->begin(); it != open_file_tab->end(); ++it) {
		if (strcmp(it->second->path, fpath) == 0) {
			of = it->second;
			it->second->path =
				(char *)realloc(it->second->path, strlen(tpath) + 1);
			strncpy(it->second->path, tpath, strlen(tpath) + 1);
			break;
		}
	}
	__unlock(&open_file_tab_mux);

	/**
	 * Lwext4 does not complete rename if the tpath exists (it returns EEXIST).
	 * But some programs expect this to overwrite the file, so as a workaround
	 * we just remove the tpath underneath. Only does regular files for now.
	 */
	int r = ext4_inode_exist(tpath, EXT4_DE_REG_FILE);
	if (r == EOK) {
		logMessage(FS_VRB_LOG_LEVEL,
				   "ext4_inode_exists for tpath in rename, deleting it");
		r = ext4_fremove(tpath);
		if (r != EOK) { // propogate ENOENT back to client
			logMessage(LOG_ERROR_LEVEL,
					   "ext4_fremove error in rename: rc = %d\n", r);
			return r;
		}
		logMessage(FS_VRB_LOG_LEVEL, "ext4_fremove OK in rename [tpath=%s]",
				   tpath);
	}

	r = ext4_frename(fpath, tpath);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_frename ERROR = %d\n", r);
		return r;
	}

	if (touch_file(tpath, 0, 1, of ? (ext4_file *)of->f : NULL)) {
		logMessage(LOG_ERROR_LEVEL, "touch_file ERROR (line: %d)\n", __LINE__);
		return BFS_FAILURE;
	}

	logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_rename OK [fpath=%s,tpath=%s]",
			   fpath, tpath);

	return BFS_SUCCESS;
}

/* if opening, need to dyanmically alloc an of struct */
int __do_lwext4_create(void *usr, const char *path, uint32_t mode) {
	int r;
	ext4_file *f = (ext4_file *)malloc(sizeof(ext4_file));

	// Just open with O_CREATE for now, and do not allow trying to create a file
	// (ie with O_TRUNC) if it already exists.
	r = ext4_fopen2(f, path, O_CREAT);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_fopen2 ERROR = %d\n", r);
		return r;
	}

	bfs_lwext4_open_file_t *of = init_open_file(path, f, 0);
	if (!of) {
		logMessage(LOG_ERROR_LEVEL, "init_open_file ERROR");
		return BFS_FAILURE;
	}

	r = ext4_mode_set(of->path, mode, (ext4_file *)of->f);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_mode_set: rc = %d\n", r);
		return r;
	}

	if (touch_file(of->path, 1, 1, (ext4_file *)of->f)) {
		logMessage(LOG_ERROR_LEVEL, "touch_file ERROR (line: %d)\n", __LINE__);
		return BFS_FAILURE;
	}

	logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_create OK [ino=%lu, fh=%d]",
			   ((ext4_file *)of->f)->inode, of->fh);

	return of->fh;
}
int __do_lwext4_ftruncate(void *usr, const char *path, bfs_fh_t fh,
						  uint32_t len) {
	__lock(&open_file_tab_mux);
	bfs_lwext4_open_file_t *of = open_file_tab->at(fh);
	if (!of) {
		logMessage(LOG_ERROR_LEVEL, "Invalid file handle\n");
		return BFS_FAILURE;
	}
	__unlock(&open_file_tab_mux);

	int r = ext4_ftruncate((ext4_file *)of->f, len);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_ftruncate ERROR = %d\n", r);
		return r;
	}

	if (touch_file(path, 1, 1, NULL)) {
		logMessage(LOG_ERROR_LEVEL, "touch_file ERROR (line: %d)\n", __LINE__);
		return BFS_FAILURE;
	}

	logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_ftruncate OK [path=%s]", path);

	return BFS_SUCCESS;
}

int __do_lwext4_chmod(void *usr, const char *path, uint32_t new_mode) {
	int r = ext4_mode_set(path, new_mode, NULL);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_mode_set ERROR = %d\n", r);
		return r;
	}

	if (touch_file(path, 0, 1, NULL)) {
		logMessage(LOG_ERROR_LEVEL, "touch_file ERROR (line: %d)\n", __LINE__);
		return BFS_FAILURE;
	}

	logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_chmod OK [path=%s]", path);

	return BFS_SUCCESS;
}

double total_open_time = 0.;
int __do_lwext4_open(void *usr, const char *path, uint32_t fmode) {
	int r;
	ext4_file *f = (ext4_file *)malloc(sizeof(ext4_file));
	bool exists = false;

	// double open_start = __get_time();

	// Check for both regular and directory files (see man pages for
	// open/opendir/fdopendir/etc. -- we can always open any file and transform
	// the descriptor into a directory descriptor if needed). We don't support
	// all of those funcs yet (eg fopendir), so we allow this but should be fine
	// for now.
	if (!ext4_inode_exist(path, EXT4_DE_REG_FILE) ||
		!ext4_inode_exist(path, EXT4_DE_DIR))
		exists = true;

	// Just always open with these flags for now. Dont need O_CREAT or O_TRUNC
	// because FUSE should generally call create instead of open if the file
	// does not exist.
	r = ext4_fopen2(f, path, O_RDWR);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_fopen2 ERROR = %d\n", r);
		return r;
	}

	bfs_lwext4_open_file_t *of = init_open_file(path, f, 0);
	if (!of) {
		logMessage(LOG_ERROR_LEVEL, "init_open_file ERROR");
		return BFS_FAILURE;
	}

	if (!exists) {
		if (touch_file(of->path, 1, 1, (ext4_file *)of->f)) {
			logMessage(LOG_ERROR_LEVEL, "touch_file ERROR (line: %d)\n",
					   __LINE__);
			return BFS_FAILURE;
		}

		r = ext4_mode_set(of->path, fmode, (ext4_file *)of->f);
		if (r != EOK) {
			logMessage(LOG_ERROR_LEVEL, "ext4_mode_set: rc = %d\n", r);
			return r;
		}
	} else {
		if (touch_file(of->path, 0, 0, (ext4_file *)of->f)) {
			logMessage(LOG_ERROR_LEVEL, "touch_file ERROR (line: %d)\n",
					   __LINE__);
			return BFS_FAILURE;
		}
	}

	// double open_end = __get_time();
	// total_open_time += (open_end - open_start);

	logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_open OK [ino=%lu, fh=%d]",
			   ((ext4_file *)of->f)->inode, of->fh);
	// logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_open total_open_time [%.3f s]",
	// 		   total_open_time / 1e6);

	return of->fh;
}

double total_read_time = 0.;
int __do_lwext4_read(void *usr, bfs_fh_t fh, char *buf, uint64_t rsize,
					 uint64_t off) {
	// double read_start = __get_time();

	__lock(&open_file_tab_mux);
	bfs_lwext4_open_file_t *of = open_file_tab->at(fh);
	if (!of) {
		logMessage(LOG_ERROR_LEVEL, "Invalid file handle\n");
		return BFS_FAILURE;
	}
	__unlock(&open_file_tab_mux);

	// lwext4 code already deals with reads properly (if off>size, it leaves buf
	// as-is), but fseek will still fail, so only seek+read if we know the off
	// is good if (off > ((ext4_file *)of->f)->fsize) { 	int hole_size = off
	// -
	// ((ext4_file *)of->f)->fsize; 	char *fill = (char *)malloc(hole_size);
	// 	logMessage(
	// 		FS_LOG_LEVEL,
	// 		"Trying to fill hole in __do_lwext4_read [off=%d, fsize=%d]\n", off,
	// 		((ext4_file *)of->f)->fsize);
	// 	if (__do_lwext4_write(usr, fh, fill, hole_size,
	// 						  ((ext4_file *)of->f)->fsize) != hole_size) {
	// 		logMessage(LOG_ERROR_LEVEL, "Failed to write to hole\n");
	// 		return BFS_FAILURE;
	// 	}
	// 	logMessage(FS_LOG_LEVEL,
	// 			   "Filled hole in __do_lwext4_read [off=%d, fsize=%d]\n", off,
	// 			   ((ext4_file *)of->f)->fsize);
	// 	free(fill);
	// 	assert(((ext4_file *)of->f)->fsize == off);
	// }

	uint64_t new_off = (off <= ((ext4_file *)of->f)->fsize)
						   ? off
						   : ((ext4_file *)of->f)->fsize;
	int r = ext4_fseek((ext4_file *)of->f, new_off, SEEK_SET);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_fseek ERROR = %d\n", r);
		return r;
	}

	size_t out = 0;
	r = ext4_fread((ext4_file *)of->f, buf, rsize, &out);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL,
				   "ext4_fread ERROR in __do_lwext4_read = %d\n", r);
		return r;
	}

	if (touch_file(of->path, 0, 0, (ext4_file *)of->f)) {
		logMessage(LOG_ERROR_LEVEL, "touch_file ERROR (line: %d)\n", __LINE__);
		return BFS_FAILURE;
	}

	// double read_end = __get_time();
	// total_read_time += (read_end - read_start);

	logMessage(
		FS_VRB_LOG_LEVEL,
		"__do_lwext4_read OK [fh=%d, size=%d, off=%lu, new_off=%lu, out=%d]",
		of->fh, rsize, off, new_off, out);
	// logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_read total_read_time [%.3f s]",
	// 		   total_read_time / 1e6);

	return out;
}

double total_write_time = 0.;
int __do_lwext4_write(void *usr, bfs_fh_t fh, char *buf, uint64_t wsize,
					  uint64_t off) {
	// double write_start = __get_time();

	__lock(&open_file_tab_mux);
	bfs_lwext4_open_file_t *of = open_file_tab->at(fh);
	if (!of) {
		logMessage(LOG_ERROR_LEVEL, "Invalid file handle\n");
		return BFS_FAILURE;
	}
	__unlock(&open_file_tab_mux);

	/*
	 * Have to deal with holes properly for programs like make. As a workaround,
	 * just do a write. The write call will seek back and do the write, so that
	 * the new fpos will should be at EOF once we arrive here (ie fpos should
	 * now be == off).
	 */
	if (off > ((ext4_file *)of->f)->fsize) {
		int hole_size = off - ((ext4_file *)of->f)->fsize;
		char *fill = (char *)malloc(hole_size);
		logMessage(FS_LOG_LEVEL, "Trying to fill hole in __do_lwext4_write\n");
		if (__do_lwext4_write(usr, fh, fill, hole_size,
							  ((ext4_file *)of->f)->fsize) != hole_size) {
			logMessage(LOG_ERROR_LEVEL, "Failed to write to hole\n");
			return BFS_FAILURE;
		}
		logMessage(FS_LOG_LEVEL, "Filled hole in __do_lwext4_write\n");
		free(fill);
		assert(((ext4_file *)of->f)->fsize == off);
	}

	int r = ext4_fseek((ext4_file *)of->f, off, SEEK_SET);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL,
				   "ext4_fseek ERROR in __do_lwext4_write = err=%d, fsize=%d, "
				   "off=%d, fh=%d\n",
				   r, ((ext4_file *)of->f)->fsize, off, fh);
		return r;
	}

	size_t out = 0;
	r = ext4_fwrite((ext4_file *)of->f, buf, wsize, &out);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_fwrite ERROR = %d\n", r);
		return r;
	}

	if (touch_file(of->path, 1, 0, (ext4_file *)of->f)) {
		logMessage(LOG_ERROR_LEVEL, "touch_file ERROR (line: %d)\n", __LINE__);
		return BFS_FAILURE;
	}

	// double write_end = __get_time();
	// total_write_time += (write_end - write_start);

	// logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_write OK [fh=%d]", of->fh);
	logMessage(FS_VRB_LOG_LEVEL,
			   "__do_lwext4_write OK [fh=%d, size=%d, off=%lu, out=%d]", of->fh,
			   wsize, off, out);
	// logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_write total_write_time [%.3f
	// s]", 		   total_write_time / 1e6);

	return out;
}

/**
 * Flushes everything for now. Later we should only flush for the target fh.
 */
int __do_lwext4_fsync(void *usr, bfs_fh_t fh, uint32_t datasync) {
	__lock(&open_file_tab_mux);
	bfs_lwext4_open_file_t *of = open_file_tab->at(fh);
	if (!of) {
		logMessage(LOG_ERROR_LEVEL, "Invalid file handle\n");
		return BFS_FAILURE;
	}
	__unlock(&open_file_tab_mux);

	int r = ext4_cache_flush(of->path);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_cache_flush ERROR = %d\n", r);
		return r;
	}

	if (touch_file(of->path, 0, 0, (ext4_file *)of->f)) {
		logMessage(LOG_ERROR_LEVEL, "touch_file ERROR (line: %d)\n", __LINE__);
		return BFS_FAILURE;
	}

	logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_fsync OK [fh=%d]", of->fh);

	return BFS_SUCCESS;
}

int __do_lwext4_release(void *usr, bfs_fh_t fh) {
	__lock(&open_file_tab_mux);
	bfs_lwext4_open_file_t *of = open_file_tab->at(fh);
	if (!of) {
		logMessage(LOG_ERROR_LEVEL, "Invalid file handle\n");
		return BFS_FAILURE;
	}
	__unlock(&open_file_tab_mux);

	// Try to close as regular or directory file.
	int r = -1;
	if (fh >= (1e6 + START_FD)) {
		r = ext4_dir_close((ext4_dir *)of->f);
		if (r != EOK) {
			logMessage(LOG_ERROR_LEVEL, "ext4_dir_close ERROR = %d\n", r);
			return r;
		}
	} else {
		r = ext4_fclose((ext4_file *)of->f);
		if (r != EOK) {
			logMessage(LOG_ERROR_LEVEL, "ext4_fclose ERROR = %d\n", r);
			return r;
		}
	}

	// int r = ext4_inode_exist(of->path, EXT4_DE_REG_FILE);
	// if (r != EOK) {
	// 	r = ext4_inode_exist(of->path, EXT4_DE_DIR);
	// 	if (r != EOK) {
	// 		// logMessage(LOG_ERROR_LEVEL,
	// 		// 		   "ext4_inode_exist ERROR in release on [path=%s] = %d\n",
	// 		// 		   of->path, r);
	// 		return r;
	// 	}

	// 	r = ext4_dir_close((ext4_dir *)of->f);
	// 	if (r != EOK) {
	// 		logMessage(LOG_ERROR_LEVEL, "ext4_dir_close ERROR = %d\n", r);
	// 		return r;
	// 	}
	// } else {
	// 	r = ext4_fclose((ext4_file *)of->f);
	// 	if (r != EOK) {
	// 		logMessage(LOG_ERROR_LEVEL, "ext4_fclose ERROR = %d\n", r);
	// 		return r;
	// 	}
	// }

	if (touch_file(of->path, 0, 0, (ext4_file *)of->f)) {
		logMessage(LOG_ERROR_LEVEL, "touch_file ERROR (line: %d)\n", __LINE__);
		return BFS_FAILURE;
	}

	if (del_open_file(of->fh) != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "del_open_file ERROR");
		return BFS_FAILURE;
	}

	logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_release OK [fh=%d]", of->fh);

	return BFS_SUCCESS;
}

int __do_lwext4_opendir(void *usr, const char *path) {
	int r;
	ext4_dir *f = (ext4_dir *)malloc(sizeof(ext4_dir));

	r = ext4_dir_open(f, path);
	if (r != EOK) {
		logMessage(LOG_ERROR_LEVEL, "ext4_dir_open ERROR = %d\n", r);
		return r;
	}

	bfs_lwext4_open_file_t *of = init_open_file(path, f, 1);
	if (!of) {
		logMessage(LOG_ERROR_LEVEL, "init_open_file ERROR");
		return BFS_FAILURE;
	}

	logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_open OK [ino=%lu, fh=%d]",
			   ((ext4_dir *)of->f)->f.inode, of->fh);

	if (touch_file(of->path, 0, 0, (ext4_file *)of->f)) {
		logMessage(LOG_ERROR_LEVEL, "touch_file ERROR (line: %d)\n", __LINE__);
		return BFS_FAILURE;
	}

	return of->fh;
}

int __do_lwext4_readdir(void *usr, uint64_t fh, void *_ents) {
	__lock(&open_file_tab_mux);
	bfs_lwext4_open_file_t *of = open_file_tab->at(fh);
	if (!of) {
		logMessage(LOG_ERROR_LEVEL, "Invalid file handle\n");
		return BFS_FAILURE;
	}
	__unlock(&open_file_tab_mux);

	std::vector<DirEntry *> *ents = (std::vector<DirEntry *> *)_ents;
	DirEntry *temp_de = NULL;
	uint64_t fino = 0;
	uint32_t fmode = 0;
	uint64_t fsize = 0;
	uint32_t atime, mtime, ctime;
	int r;
	const ext4_direntry *de;

	de = ext4_dir_entry_next((ext4_dir *)of->f);
	while (de) {
		// TODO: note that this will currently report back the size of the
		// parent dir (..) as the size of the current dir. We dont know the size
		// of the parent dir at this point (without stat'ing it). Seems like
		// client gets it (see client logs), but the output to the terminal
		// shows different size, so FUSE might be caching things and showing a
		// diff size. Should be fine for now.
		if ((strcmp((const char *)de->name, ".") == 0) ||
			(strcmp((const char *)de->name, "..") == 0) ||
			(strcmp((const char *)de->name, "lost+found") == 0)) {
			r = ext4_mode_get(of->path, &fmode, &(((ext4_dir *)of->f)->f));
			if (r != EOK) {
				logMessage(LOG_ERROR_LEVEL,
						   "ext4_mode_get ERROR in __do_lwext4_readdir = %d\n",
						   r);
				return r;
			}

			// same as parent (the dir)
			fino = de->inode;
			fsize = ((ext4_dir *)of->f)->f.fsize;
		} else {
			std::string par_path = std::string(of->path);
			std::string full_de_path = par_path;
			if (par_path.c_str()[par_path.size() - 1] != '/')
				full_de_path += '/';
			full_de_path += std::string((const char *)de->name);
			r = __do_lwext4_getattr(usr, full_de_path.c_str(), NULL, &fino,
									&fmode, &fsize, &atime, &mtime, &ctime);
			if (r != BFS_SUCCESS) {
				logMessage(
					LOG_ERROR_LEVEL,
					"__do_lwext4_getattr ERROR in __do_lwext4_readdir on "
					"[%s] = %d\n",
					de->name, r);
				return r;
			}
		}
		assert(fino == de->inode); // sanity check

		temp_de = new DirEntry(std::string((const char *)de->name), fino, 0, 0,
							   fmode, fsize, atime, mtime, ctime);
		ents->push_back(temp_de);
		de = ext4_dir_entry_next((ext4_dir *)of->f);
	}

	if (touch_file(of->path, 0, 0, (ext4_file *)of->f)) {
		logMessage(LOG_ERROR_LEVEL, "touch_file ERROR (line: %d)\n", __LINE__);
		return BFS_FAILURE;
	}

	logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_readdir OK [path=%s, fh=%d]",
			   of->path, of->fh);

	return BFS_SUCCESS;
}

int __do_lwext4_rmdir(void *usr, const char *path) {
	int r = ext4_dir_rm(path);
	// if (r != EOK && r != ENOENT) {
	if (r != EOK) { // propogate ENOENT back to client
		logMessage(LOG_ERROR_LEVEL, "ext4_fremove ext4_dir_rm: rc = %d\n", r);
		return r;
	}

	logMessage(FS_VRB_LOG_LEVEL, "__do_lwext4_rmdir OK [path=%s]", path);

	return BFS_SUCCESS;
}

int __do_lwext4_destroy(void *usr) {
	delete seen;
	seen = NULL;

	// Now set status to unmounted/uninitialized so that MT can be initialized
	// again for the server.
	status = UNINITIALIZED;

	if (pthread_mutex_destroy(&open_file_tab_mux) != 0)
		abort();
	if (pthread_mutex_destroy(&mp_mux) != 0)
		abort();

	// TODO: other cleanup

	logMessage(FS_LOG_LEVEL, "__do_lwext4_destroy OK");

	return BFS_SUCCESS;
}
