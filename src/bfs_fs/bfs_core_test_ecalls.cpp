/**
 * @file bfs_core_test_ecalls.cpp
 * @brief ECall definitions and helpers for the bfs core unit tests (enclave
 * based tests).
 */

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <string>

#include "bfs_acl.h"
#include "bfs_core.h"
#include "bfs_fs_layer.h"
#include <bfsBlockLayer.h>
#include <bfsCryptoError.h>
#include <bfs_common.h>
#include <bfs_log.h>
#include <bfs_util.h>

#ifdef __BFS_ENCLAVE_MODE
#include "bfs_enclave_core_test_t.h"
#include "sgx_trts.h" /* For generating random numbers */
#elif defined(__BFS_DEBUG_NO_ENCLAVE)
#include "bfs_core_test_ecalls.h"
#endif
#include "bfs_core_ext4_helpers.h"

static void save_lats();
static void write_lats_to_file(std::string, std::string);

static BfsHandle *bfs_handle = NULL; /* Handle to a file system instance */
static BfsUserContext *test_usr = NULL;
static bool fs_initialized = false;
static std::vector<double> write_lats, read_lats;
static std::vector<bfs_vbid_t> blk_accesses[2];

/**
 * @brief Initialize the entire bfs stack for core blk test.
 *
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int ecall_bfs_enclave_init(int type) {
	if (BfsFsLayer::bfsFsLayerInit() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed bfsFsLayerInit\n");
		return BFS_FAILURE;
	}

	// just return if doing blk test
	if ((type == 1) && BfsFsLayer::use_lwext4())
		return BFS_SUCCESS;

	// otherwise if doing file test, keep going
	if (BfsFsLayer::use_lwext4()) {
		if (__do_lwext4_init((void *)&blk_accesses) != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL,
					   "Failed to initalize [lwext4] enclave, aborting.");
			return BFS_FAILURE;
		}
	} else {
		if (bfsBlockLayer::set_vbc(bfsVertBlockCluster::bfsClusterFactory()) !=
			BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL,
					   "Failed to initalize virtual block cluster, aborting.");
			abort();
		}
	}

	BfsACLayer::add_user_context(0);
	test_usr = BfsACLayer::get_user_context(0);

	int32_t ret = 0;
	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	// reset_perf_counters();

	try {
		if (!fs_initialized) {
			if (!BfsFsLayer::use_lwext4())
				bfs_handle = new BfsHandle();

			if (BfsFsLayer::use_lwext4())
				ret = __do_lwext4_mkfs();
			else
				ret = bfs_handle->mkfs();

			if (BfsFsLayer::use_lwext4())
				ret += __do_lwext4_mount();
			else
				ret += bfs_handle->mount();

			fs_initialized = true;
		} else {
			ret = BFS_SUCCESS;
		}
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	return BFS_SUCCESS;
}

static bfs_vbid_t __get_random_value(bfs_vbid_t min, bfs_vbid_t max) {
#ifdef __BFS_ENCLAVE_MODE
	bfs_vbid_t x = 0;
	if (sgx_read_rand((unsigned char *)&x, sizeof(bfs_vbid_t)) == SGX_SUCCESS)
		return (x % (max - min + 1)) + min;
	else {
		logMessage(LOG_ERROR_LEVEL, "Failed generating random value");
		abort();
	}
#else
	return get_random_value(min, max);
#endif
	logMessage(LOG_ERROR_LEVEL, "Failed generating random value");
	return 0;
}

static bfs_vbid_t __get_random_data(char *b, bfs_vbid_t size) {
#ifdef __BFS_ENCLAVE_MODE
	if (sgx_read_rand((unsigned char *)b, size) == SGX_SUCCESS)
		return 0;
	else {
		logMessage(LOG_ERROR_LEVEL, "Failed generating random data");
		abort();
	}
#else
	return get_random_data(b, size);
#endif
	logMessage(LOG_ERROR_LEVEL, "Failed generating random data");
	return 0;
}

/**
 * @brief Start the unit test from within the enclave.
 *
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int ecall_bfs_start_core_blk_test(uint64_t num_test_iterations) {
	if (BfsFsLayer::use_lwext4()) {
		// run the file I/O test
		logMessage(LOG_ERROR_LEVEL, "Starting file I/O test\n");
		if (run_bfs_core_ext4_file_test() != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL, "Failed to run file I/O test\n");
			return BFS_FAILURE;
		}
		logMessage(LOG_ERROR_LEVEL, "Done file I/O test\n");

		// then open the file device for the block I/O test
		logMessage(LOG_ERROR_LEVEL, "Starting block I/O test\n");
		if (init_bfs_core_ext4_blk_test() != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL, "Failed to init file device\n");
			return BFS_FAILURE;
		}
	}

	// init bfs test parameters
	std::map<bfs_vbid_t, const VBfsBlock *> blist; // addr,plaintext data
	double s, e;
	double total_bytes_written = 0., total_bytes_read = 0.;

	bfs_vbid_t b;
	VBfsBlock *blk_builder, *blk_checker = new VBfsBlock(NULL, BLK_SZ, 0, 0, 0),
							*blk_writer = new VBfsBlock(NULL, BLK_SZ, 0, 0, 0);
	char utstr[129];
	bool new_blk;
	int check;
	for (uint32_t i = 0; i < num_test_iterations; i++) {
		logMessage(FS_VRB_LOG_LEVEL, "Starting iteration [%d]", i);
		// pick a block (whether already written or to-be written)
		if (BfsFsLayer::use_lwext4())
			b = __get_random_value(0,
								   BFS_LWEXT4_NUM_BLKS -
									   1); // vbc does not exist
		else
			b = __get_random_value(
				DATA_REL_START_BLK_NUM,
				bfsBlockLayer::get_vbc()->getMaxVertBlocNum() - 1);
		new_blk = false;
		if (blist.find(b) == blist.end()) {
			// init the block and add to map
			new_blk = true;
			blk_builder = new VBfsBlock(NULL, BLK_SZ, 0, 0, b);
			if (__get_random_data(blk_builder->getBuffer(), BLK_SZ) != 0) {
				logMessage(LOG_ERROR_LEVEL, "Failed generating random data");
				return BFS_FAILURE;
			}
			// save plaintext data in block list
			blist[b] = blk_builder;
		}

		// pick an op (read or write); only allow read if the chosen block has
		// been written already so the data validation is consistent
		if ((__get_random_value(0, 1) == 0) || new_blk) { // do write
			try {
				blk_accesses[1].push_back(b);
				logMessage(FS_VRB_LOG_LEVEL, "Starting write block [%lu]", b);

				// prep blk_writer for write
				blk_writer->set_vbid(b);
				blk_writer->resizeAllocation(0, BLK_SZ, 0);
				// copy plaintext data over to the writer buffer so that the
				// encrypted version of the data is not saved in the list (since
				// write_blk encrypts in-place)
				memcpy(blk_writer->getBuffer(), blist[b]->getBuffer(),
					   blk_writer->getLength());

				bufToString(blk_writer->getBuffer(), BLK_SZ, utstr, 128);
				logMessage(FS_VRB_LOG_LEVEL, "blk before write: [%s]", utstr);

				s = __get_time();
				if (BfsFsLayer::use_lwext4())
					__do_file_dev_bwrite(blk_writer);
				else
					bfs_handle->write_blk(*blk_writer, _bfs__O_SYNC);
				e = __get_time();
				write_lats.push_back(e - s);
				total_bytes_written += BLK_SZ;
			} catch (bfsUtilError *err) {
				logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
				delete err;
				return BFS_FAILURE;
			} catch (BfsServerError &se) {
				// log the server error and return fail
				if (se.err().size() > 0)
					logMessage(LOG_ERROR_LEVEL, se.err().c_str());

				return BFS_FAILURE;
			} catch (std::exception &ex) {
				logMessage(LOG_ERROR_LEVEL,
						   "Unknown exception caught from write_blk\n");
			}
			bufToString(blk_writer->getBuffer(), BLK_SZ, utstr, 128);
			logMessage(FS_VRB_LOG_LEVEL, "blk after write: [%s]", utstr);
			logMessage(FS_VRB_LOG_LEVEL, "Successfully wrote block [%lu]",
					   blk_writer->get_vbid());
		} else { // do read
			try {
				blk_accesses[0].push_back(b);
				logMessage(FS_VRB_LOG_LEVEL, "Starting read block [%lu]", b);

				// prep check buf for read
				blk_checker->set_vbid(b);
				// if (BfsFsLayer::use_lwext4())
				// 	blk_checker->resizeAllocation(0, BLK_SZ, 0);
				// else
				// 	blk_checker->resizeAllocation(0, BLK_SZ, 0);
				blk_checker->resizeAllocation(0, BLK_SZ, 0);
				blk_checker->burn();

				s = __get_time();
				if (BfsFsLayer::use_lwext4())
					__do_file_dev_bread(blk_checker);
				else
					bfs_handle->read_blk(*blk_checker);
				e = __get_time();
				read_lats.push_back(e - s);
				total_bytes_read += BLK_SZ;
			} catch (bfsUtilError *err) {
				logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
				delete err;
				return BFS_FAILURE;
			} catch (BfsServerError &se) {
				// log the server error and return fail
				if (se.err().size() > 0)
					logMessage(LOG_ERROR_LEVEL, se.err().c_str());

				return BFS_FAILURE;
			} catch (std::exception &ex) {
				logMessage(LOG_ERROR_LEVEL,
						   "Unknown exception caught from read_blk\n");
			}

			// validate the read data
			if ((check = memcmp(blk_checker->getBuffer(), blist[b]->getBuffer(),
								blk_checker->getLength())) != 0) {
				logMessage(LOG_ERROR_LEVEL,
						   "Retrieved block [%lu] failed match validation, %d "
						   "bytes OK",
						   blist[b]->get_vbid(), check);
				bufToString(blk_checker->getBuffer(), BLK_SZ, utstr, 128);
				logMessage(LOG_ERROR_LEVEL, "Received: [%s]", utstr);
				bufToString(blist[b]->getBuffer(), BLK_SZ, utstr, 128);
				logMessage(LOG_ERROR_LEVEL, "Check: [%s]", utstr);
				return BFS_FAILURE;
			}

			logMessage(FS_VRB_LOG_LEVEL,
					   "Successfully read and validated block [%lu]",
					   blk_checker->get_vbid());
		}
	}

	// cleanup
	delete blk_checker;
	delete blk_writer;
	for (auto it = blist.begin(); it != blist.end(); it++)
		delete it->second;

	if (BfsFsLayer::use_lwext4()) {
		// close file device for the block I/O test
		if (fini_bfs_core_ext4_blk_test() != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL, "Failed to fini core ext4 blk test\n");
			return BFS_FAILURE;
		}
	}

	save_lats();

	// log some stats
	logMessage(FS_LOG_LEVEL, "Summary of block I/O performance for [%s/sgx]:",
			   BfsFsLayer::use_lwext4() ? "lwext4" : "bfs");
	logMessage(FS_LOG_LEVEL, "   > iterations=%lu, #blocks=%lu",
			   num_test_iterations, BFS_LWEXT4_NUM_BLKS);
	logMessage(
		FS_LOG_LEVEL, "   > Write throughput: (%.3f MB / %.3f s) %.3f MB/s",
		total_bytes_written / 1e6,
		std::accumulate(write_lats.begin(), write_lats.end(), 0) / 1e6,
		(total_bytes_written /
		 (std::accumulate(write_lats.begin(), write_lats.end(), 0) / 1e6)) /
			1e6);
	logMessage(
		FS_LOG_LEVEL, "   > Read throughput: (%.3f MB / %.3f s) %.3f MB/s",
		total_bytes_read / 1e6,
		std::accumulate(read_lats.begin(), read_lats.end(), 0) / 1e6,
		(total_bytes_read /
		 (std::accumulate(read_lats.begin(), read_lats.end(), 0) / 1e6)) /
			1e6);

	return BFS_SUCCESS;
}

int ecall_bfs_start_core_file_test_rand(uint64_t num_files,
										uint64_t num_test_iterations,
										uint64_t max_op_sz,
										uint64_t min_op_sz) {
	// Note: enclave_init should be called before this to init things correctly
	// for either bfs or lwext4

	// for result collection
	double total_bytes_written = 0., total_bytes_read = 0.;

	uint64_t ret = 0;
	std::string path = "";
	bfs_fh_t fh = 0;
	std::vector<std::tuple<bfs_fh_t, uint64_t, char *>>
		open_file_data; // handle,size,data
	uint32_t max_file_sz = (NUM_DIRECT_BLOCKS + NUM_BLKS_PER_IB) *
						   BLK_SZ; // single indirection layer

	bfs_uid_t uid = 0;
	bfs_ino_id_t fino = 0;
	uint32_t fmode = 0;
	uint64_t fsize = 0;

	uint64_t wr_fh = 0, wr_off = 0, wr_sz = 0;
	uint64_t r_fh = 0, r_off = 0, r_sz = 0;
	char *data = NULL;
	uint64_t rix = 0;

	// create random files (only fail on fs issues)
	for (uint32_t i = 0; i < num_files; i++) {
		path = "/test" + std::to_string(i);

		try {
			if (BfsFsLayer::use_lwext4())
				fh = __do_lwext4_create(test_usr, path.c_str(), 0777);
			else
				fh = bfs_handle->bfs_create(test_usr, path, 0777);

			if (fh < START_FD) {
				logMessage(LOG_ERROR_LEVEL, "Error creating file [path=%s]\n",
						   path.c_str());
				goto CLEANUP_FAIL;
			} else {
				logMessage(FS_VRB_LOG_LEVEL,
						   "Successful created/open file [path=%s, fh=%d]\n",
						   path.c_str(), fh);
			}
			open_file_data.emplace_back(std::make_tuple(fh, 0, (char *)NULL));
		} catch (BfsAccessDeniedError &ade) {
			if (ade.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, ade.err().c_str());
			goto CLEANUP_FAIL;
		} catch (BfsClientRequestFailedError &rfe) {
			// let client handle fs errors, just log what happened
			if (rfe.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, rfe.err().c_str());
			goto CLEANUP_FAIL;
		} catch (BfsServerError &se) {
			// log the server error and return fail
			if (se.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, se.err().c_str());
			goto CLEANUP_FAIL;
		}
	}

	// stat the files to verify they exist
	uint32_t atime, mtime, ctime;
	for (uint32_t i = 0; i < num_files; i++) {
		path = "/test" + std::to_string(i);

		try {
			if (BfsFsLayer::use_lwext4())
				ret =
					__do_lwext4_getattr(test_usr, path.c_str(), &uid, &fino,
										&fmode, &fsize, &atime, &mtime, &ctime);
			else
				ret = bfs_handle->bfs_getattr(test_usr, path, &uid, &fino,
											  &fmode, &fsize);

			if (ret != BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL, "Error on file getattr [path=%s]\n",
						   path.c_str());
				goto CLEANUP_FAIL;
			} else {
				logMessage(FS_VRB_LOG_LEVEL,
						   "Successful file getattr [path=%s, uid=%d, fino=%d, "
						   "fmode=%d, fsize=%d]\n",
						   path.c_str(), uid, fino, fmode, fsize);
			}
		} catch (BfsAccessDeniedError &ade) {
			if (ade.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, ade.err().c_str());
			goto CLEANUP_FAIL;
		} catch (BfsClientRequestFailedError &rfe) {
			// let client handle fs errors, just log what happened
			if (rfe.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, rfe.err().c_str());
			goto CLEANUP_FAIL;
		} catch (BfsServerError &se) {
			// log the server error and return fail
			if (se.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, se.err().c_str());
			goto CLEANUP_FAIL;
		}
	}

	// do random writes
	// struct timeval writes_start_time, writes_end_time;
	// gettimeofday(&writes_start_time, NULL);
	double writes_start_time, writes_end_time;
	writes_start_time = __get_time();
	for (uint32_t i = 0; i < num_test_iterations; i++) {
		// get a random fh from vector (handle,size,data)
		rix = __get_random_value(0, (uint32_t)open_file_data.size() - 1);
		auto r = open_file_data.at(rix);
		wr_fh = std::get<0>(r);
		// get a random offset (allow write of at least 1 byte using `- 1`)
		if (std::get<1>(r) > 0) {
			wr_off = __get_random_value(0, (uint32_t)std::get<1>(r) - 1 - 1);
		} else {
			wr_off = 0;
		}
		// get a random write size (at least 1 byte)
		wr_sz = __get_random_value(min_op_sz, max_op_sz);

		// dont let file get too big for now; just try to another file
		if ((wr_off + wr_sz) > max_file_sz) {
			continue;
		}

		// get random data and put in the check buffers (in open_file_data)
		data = (char *)malloc(wr_sz);
		__get_random_data(data, wr_sz);
		if ((wr_off + wr_sz) > std::get<1>(r)) {
			std::get<2>(r) = (char *)realloc(std::get<2>(r), wr_off + wr_sz);
			std::get<1>(r) = wr_off + wr_sz; // update file size
		}
		memcpy(&((std::get<2>(r))[wr_off]), data, wr_sz); // copy check data in
		open_file_data.at(rix) = r;

		try {
			if (BfsFsLayer::use_lwext4())
				ret = __do_lwext4_write(test_usr, wr_fh, data, wr_sz, wr_off);
			else
				ret =
					bfs_handle->bfs_write(test_usr, wr_fh, data, wr_sz, wr_off);

			if (ret != wr_sz) {
				logMessage(LOG_ERROR_LEVEL,
						   "Write fail [iteration=%d, fh=%d, size=%d, off=%d, "
						   "fsize=%d].\n",
						   i, wr_fh, wr_sz, wr_off, std::get<1>(r));
				goto CLEANUP_FAIL;
			} else {
				logMessage(FS_VRB_LOG_LEVEL,
						   "Write success [iteration=%d, fh=%d, size=%d, "
						   "off=%d, fsize=%d].\n",
						   i, wr_fh, wr_sz, wr_off, std::get<1>(r));
				total_bytes_written += (double)ret;
			}
		} catch (BfsAccessDeniedError &ade) {
			if (ade.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, ade.err().c_str());
			goto CLEANUP_FAIL;
		} catch (BfsClientRequestFailedError &rfe) {
			// let client handle fs errors, just log what happened
			if (rfe.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, rfe.err().c_str());
			goto CLEANUP_FAIL;
		} catch (BfsServerError &se) {
			// log the server error and return fail
			if (se.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, se.err().c_str());
			goto CLEANUP_FAIL;
		}

		free(data);
	}
	// gettimeofday(&writes_end_time, NULL);
	writes_end_time = __get_time();

	// do random reads
	// struct timeval reads_start_time, reads_end_time;
	// gettimeofday(&reads_start_time, NULL);
	double reads_start_time, reads_end_time;
	reads_start_time = __get_time();
	for (uint32_t i = 0; i < num_test_iterations; i++) {
		// get a random fh from vector (handle,size,data)
		auto r = open_file_data.at(
			__get_random_value(0, (uint32_t)open_file_data.size() - 1));
		r_fh = std::get<0>(r);

		// get a random offset (try to allow read of at least 1 with `- 1`)
		if (std::get<1>(r) > 0) {
			if ((uint32_t)std::get<1>(r) == 1)
				r_off = __get_random_value(0, (uint32_t)std::get<1>(r) - 1);
			else
				r_off = __get_random_value(0, (uint32_t)std::get<1>(r) - 1 - 1);
		} else {
			// unlike write, which can just start writing at offset 0, in read
			// we should not try to read file if it's empty
			continue;
		}

		// dont try to read empty file
		if (std::get<1>(r) == 0) {
			continue;
		}

		// get a random read size (at least 1 byte)
		r_sz = __get_random_value(min_op_sz, max_op_sz);

		data = (char *)malloc(r_sz);
		memset(data, 0x0, r_sz);

		try {
			// dont care if read goes past end of file; should result in a short
			// read
			if (BfsFsLayer::use_lwext4())
				ret = __do_lwext4_read(test_usr, r_fh, data, r_sz, r_off);
			else
				ret = bfs_handle->bfs_read(test_usr, r_fh, data, r_sz, r_off);

			if (ret == 0) {
				logMessage(
					LOG_ERROR_LEVEL,
					"Read failed no bytes read [iteration=%d, fh=%d, size=%d, "
					"off=%d, fsize=%d].\n",
					r_fh, i, r_sz, r_off, std::get<1>(r));
				return -1;
			} else if (ret < r_sz) {
				logMessage(
					FS_VRB_LOG_LEVEL,
					"Read short [iteration=%d, fh=%d, size=%d, ret=%d, off=%d, "
					"fsize=%d].\n",
					i, r_fh, r_sz, ret, r_off, std::get<1>(r));
			} else {
				logMessage(FS_VRB_LOG_LEVEL,
						   "Read success [iteration=%d, fh=%d, size=%d, "
						   "off=%d, fsize=%d].\n",
						   i, r_fh, r_sz, r_off, std::get<1>(r));
			}
			total_bytes_read += (double)ret;

			// only compare for however many bytes were read
			char *check = &(std::get<2>(r)[r_off]);
			if (memcmp(check, data, ret) != 0) {
				logMessage(LOG_ERROR_LEVEL, "Invalid write/read compare.\n");
				goto CLEANUP_FAIL;
			} else {
				logMessage(FS_VRB_LOG_LEVEL, "write/read compare success.\n");
			}
		} catch (BfsAccessDeniedError &ade) {
			if (ade.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, ade.err().c_str());
			goto CLEANUP_FAIL;
		} catch (BfsClientRequestFailedError &rfe) {
			// let client handle fs errors, just log what happened
			if (rfe.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, rfe.err().c_str());
			goto CLEANUP_FAIL;
		} catch (BfsServerError &se) {
			// log the server error and return fail
			if (se.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, se.err().c_str());
			goto CLEANUP_FAIL;
		}

		free(data);
	}
	// gettimeofday(&reads_end_time, NULL);
	reads_end_time = __get_time();

	// close files
	for (auto of : open_file_data) {
		try {
			if (BfsFsLayer::use_lwext4())
				ret = __do_lwext4_release(test_usr, std::get<0>(of));
			else
				ret = bfs_handle->bfs_release(test_usr, std::get<0>(of));

			if (ret != BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL, "Error during release [fh=%d].\n",
						   std::get<0>(of));
				goto CLEANUP_FAIL;
			} else {
				logMessage(FS_VRB_LOG_LEVEL,
						   "Successful file release [fh=%d].\n",
						   std::get<0>(of));
			}
		} catch (BfsAccessDeniedError &ade) {
			if (ade.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, ade.err().c_str());
			goto CLEANUP_FAIL;
		} catch (BfsClientRequestFailedError &rfe) {
			// let client handle fs errors, just log what happened
			if (rfe.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, rfe.err().c_str());
			goto CLEANUP_FAIL;
		} catch (BfsServerError &se) {
			// log the server error and return fail
			if (se.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, se.err().c_str());
			goto CLEANUP_FAIL;
		}
	}

	// delete files
	for (uint32_t i = 0; i < num_files; i++) {
		try {
			path = "/test" + std::to_string(i);

			if (BfsFsLayer::use_lwext4())
				ret = __do_lwext4_unlink(test_usr, path.c_str());
			else
				ret = bfs_handle->bfs_unlink(test_usr, path);

			if (ret != BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL, "Error during unlink [path=%s].\n",
						   path.c_str());
				goto CLEANUP_FAIL;
			} else {
				logMessage(FS_VRB_LOG_LEVEL,
						   "Successful file unlink [path=%s].\n", path.c_str());
			}
		} catch (BfsAccessDeniedError &ade) {
			if (ade.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, ade.err().c_str());
			goto CLEANUP_FAIL;
		} catch (BfsClientRequestFailedError &rfe) {
			// let client handle fs errors, just log what happened
			if (rfe.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, rfe.err().c_str());
			goto CLEANUP_FAIL;
		} catch (BfsServerError &se) {
			// log the server error and return fail
			if (se.err().size() > 0)
				logMessage(LOG_ERROR_LEVEL, se.err().c_str());
			goto CLEANUP_FAIL;
		}
	}

	goto CLEANUP_OK;

CLEANUP_FAIL:
	if (BfsFsLayer::use_lwext4()) {
		// TODO
	} else {
		delete bfs_handle;
		bfs_handle = NULL;
	}

	return BFS_FAILURE;

CLEANUP_OK:
	logMessage(FS_LOG_LEVEL, "Summary of file I/O performance for [%s/sgx]:",
			   BfsFsLayer::use_lwext4() ? "lwext4" : "bfs");
	if (!BfsFsLayer::use_lwext4()) {
		logMessage(FS_LOG_LEVEL, "   > Dentry cache hit rate: %.2f%%\n",
				   bfs_handle->get_dentry_cache().get_hit_rate() * 100.0);
		logMessage(FS_LOG_LEVEL, "   > Inode cache hit rate: %.2f%%\n",
				   bfs_handle->get_ino_cache().get_hit_rate() * 100.0);
	}
	logMessage(FS_LOG_LEVEL,
			   "   > Write throughput: (%.3f MB / %.3f s) %.3f MB/s",
			   total_bytes_written / 1e6,
			   ((double)(writes_end_time - writes_start_time) / 1e6),
			   (total_bytes_written /
				((double)(writes_end_time - writes_start_time) / 1e6)) /
				   1e6);
	logMessage(FS_LOG_LEVEL,
			   "   > Read throughput: (%.3f MB / %.3f s) %.3f MB/s",
			   total_bytes_read / 1e6,
			   ((double)(reads_end_time - reads_start_time) / 1e6),
			   (total_bytes_read /
				((double)(reads_end_time - reads_start_time) / 1e6)) /
				   1e6);

	if (!BfsFsLayer::use_lwext4()) {
		delete bfs_handle;
		bfs_handle = NULL;
	}

	return BFS_SUCCESS;
}

/* Simpler sequential/random I/O test to measure raw achieavble TP */
// Note that we have 4+ file based tests now (non-enclave mode in
// bfs_core_test.cpp, enclave mode random test above, enclave mode seq/random
// test below, and non-enclave mode seq/random test in rw-latency.cpp)
static int num_samples = 0;
static const int num_files2 = 1;
static const std::string fstem = "/test/";
// static const int fsz = 1073741824;
static int fsz = 0;
static int op_sz = 0;
static int iterations_per_sample = 0;
static int write_sz = 0, read_sz = 0;
static int fds[num_files2] = {0};
static double total_write_time = 0.0, total_read_time = 0.0,
			  total_open_time_r = 0.0, total_close_time_r = 0.0,
			  total_open_time_w = 0.0, total_close_time_w = 0.0,
			  total_stat_time = 0.0;
static int random_flag = 0;

static void run_write(int);
static void run_read(int);

int ecall_bfs_start_core_file_test_simple(int _random, uint64_t _num_it,
										  uint64_t _fsz, uint64_t _op_sz) {
	// configure whether R/W are random or not
	random_flag = _random;

	logMessage(FS_LOG_LEVEL,
			   "blk_accesses[0].size() after mkfs and before test begin = %d",
			   blk_accesses[0].size());
	logMessage(FS_LOG_LEVEL,
			   "blk_accesses[1].size() after mkfs and before test begin = %d",
			   blk_accesses[1].size());

	if (!_num_it || !_fsz || !_op_sz) {
		logMessage(LOG_ERROR_LEVEL, "args are bad [%lu,%lu,%lu]\n", _num_it,
				   _fsz, _op_sz);
		abort();
	} else {
		num_samples = _num_it;
		fsz = _fsz;
		op_sz = _op_sz;
		iterations_per_sample = fsz / op_sz;
		write_sz = op_sz;
		read_sz = op_sz;
	}

	for (int s = 0; s < num_samples; s++)
		run_write(s);

	for (int s = 0; s < num_samples; s++)
		run_read(s);

	double total_MB =
		(op_sz * num_samples * num_files2 * iterations_per_sample) / 1e6;

	save_lats();

	logMessage(FS_LOG_LEVEL,
			   "Results for [%s (%s, num_samples=%d, fsz=%d, op_sz=%d, "
			   "iterations_per_sample=%d)]\n",
			   fstem.c_str(), random_flag ? "rand" : "seq", num_samples, fsz,
			   op_sz, iterations_per_sample);
	logMessage(FS_LOG_LEVEL, "Write throughput: (%.3f MB / %.3f s) %.3f MB/s\n",
			   total_MB, total_write_time / 1e6,
			   total_MB / (total_write_time / 1e6));
	logMessage(FS_LOG_LEVEL, "Read throughput: (%.3f MB / %.3f s) %.3f MB/s\n",
			   total_MB, total_read_time / 1e6,
			   total_MB / (total_read_time / 1e6));
	logMessage(FS_LOG_LEVEL, "Open/create latency (w): %.3f ms\n",
			   total_open_time_w / 1e3 / num_samples);
	logMessage(FS_LOG_LEVEL, "Close latency (w): %.3f ms\n",
			   total_close_time_w / 1e3 / num_samples);
	logMessage(FS_LOG_LEVEL, "Open latency (r): %.3f ms\n",
			   total_open_time_r / 1e3 / num_samples);
	logMessage(FS_LOG_LEVEL, "Close latency (r): %.3f ms\n",
			   total_close_time_r / 1e3 / num_samples);
	logMessage(FS_LOG_LEVEL, "Stat latency (w): %.3f ms\n",
			   total_stat_time / 1e3 / num_samples);

	return BFS_SUCCESS;
}

static void run_write(int sample) {
	char *buf = new char[op_sz]; // sized to read/write 1 raw eff block
	std::string fname;

	// open the files
	double curr_open_start_time, curr_open_end_time;
	for (int f = 0; f < num_files2; f++) {
		fname = fstem + std::to_string(f);

		curr_open_start_time = __get_time();
		// fds[f] = open(fname.c_str(),
		// 			  O_RDWR | O_CREAT | O_DIRECT | O_SYNC | O_TRUNC, 0777);
		fds[f] = __do_lwext4_create(NULL, fname.c_str(), 0777);
		if (fds[f] < 0) {
			logMessage(FS_LOG_LEVEL, "Error opening file: %s\n",
					   strerror(errno));
			return;
		}
		curr_open_end_time = __get_time();
		total_open_time_w += curr_open_end_time - curr_open_start_time;
	}

	// stat the files
	double curr_stat_start_time, curr_stat_end_time;
	for (int f = 0; f < num_files2; f++) {
		fname = fstem + std::to_string(f);

		curr_stat_start_time = __get_time();
		// fds[f] = stat(fname.c_str(),
		// 			  O_RDWR | O_CREAT | O_DIRECT | O_SYNC | O_TRUNC, 0777);
		uint64_t fino;
		uint32_t fmode;
		uint64_t fsize;
		uint32_t atime, mtime, ctime;
		int r = __do_lwext4_getattr(NULL, fname.c_str(), NULL, &fino, &fmode,
									&fsize, &atime, &mtime, &ctime);
		if (r != 0) {
			logMessage(FS_LOG_LEVEL, "Error stat'ing file: %s\n",
					   strerror(errno));
			return;
		}
		curr_stat_end_time = __get_time();
		total_stat_time += curr_stat_end_time - curr_stat_start_time;
	}

	// preselect some offsets
	// iterations_per_sample basically determines the max possible byte
	// offset, so just make sure the return from get_rand_value is within that
	int *offs = new int[iterations_per_sample];
	for (int i = 0; i < iterations_per_sample; i++) {
		if (random_flag)
			offs[i] = op_sz * __get_random_value(0, iterations_per_sample - 1);
		else
			offs[i] = op_sz * i;
		// logMessage(
		// 	FS_LOG_LEVEL,
		// 	"fsz=%lu, op_sz=%lu, offs[i]=%lu, iterations_per_sample=%d\n", fsz,
		// 	op_sz, op_sz * __get_random_value(0, iterations_per_sample),
		// 	iterations_per_sample);
	}

	// do writes
	double curr_writes_start_time = __get_time();
	int wret = 0;
	for (int f = 0; f < num_files2; f++) {
		for (int i = 0; i < iterations_per_sample; i++) {
			// write_start_time =
			// 	std::chrono::time_point_cast<std::chrono::microseconds>(
			// 		std::chrono::high_resolution_clock::now())
			// 		.time_since_epoch()
			// 		.count();
			// logMessage(FS_LOG_LEVEL,"WRITE: sample [%d], iteration [%d]\n",
			// sample, i);
			wret = __do_lwext4_write(NULL, fds[f], buf, write_sz, offs[i]);
			if (wret != write_sz) {
				logMessage(FS_LOG_LEVEL,
						   "Error during write: %d bytes written, %s\n", wret,
						   strerror(errno));
				return;
			}
			// write_end_time =
			// 	std::chrono::time_point_cast<std::chrono::microseconds>(
			// 		std::chrono::high_resolution_clock::now())
			// 		.time_since_epoch()
			// 		.count();
			// // logMessage(FS_LOG_LEVEL,"Write latency: %.3fμs\n",
			// // 	   write_end_time - write_start_time);
			// write_latencies.push_back(write_end_time - write_start_time);
			// write_lats.push_back(write_end_time - write_start_time);
		}
	}
	double curr_writes_end_time = __get_time();
	total_write_time += (curr_writes_end_time - curr_writes_start_time);
	delete[] offs;

	// // close the files
	double curr_close_start_time = __get_time();
	for (int f = 0; f < num_files2; f++) {
		if (__do_lwext4_release(NULL, fds[f]) != 0) {
			logMessage(FS_LOG_LEVEL, "Error closing file\n");
			abort();
		}
	}
	double curr_close_end_time = __get_time();
	total_close_time_w += (curr_close_end_time - curr_close_start_time);

	// delete them for next sample
	// for (int f = 0; f < num_files2; f++) {
	// 	fname = fstem + std::to_string(f);
	// 	if (__do_lwext4_unlink(NULL, fname.c_str()) != 0) {
	// 		logMessage(FS_LOG_LEVEL, "Error deleting file\n");
	// 		abort();
	// 	}
	// }
	delete buf;
}

static void run_read(int sample) {
	char *buf = new char[op_sz]; // sized to read/write 1 raw eff block
	std::string fname;

	// open the files
	double curr_open_start_time, curr_open_end_time;
	for (int f = 0; f < num_files2; f++) {
		fname = fstem + std::to_string(f);

		curr_open_start_time = __get_time();
		// fds[f] = open(fname.c_str(),
		// 			  O_RDWR | O_CREAT | O_DIRECT | O_SYNC | O_TRUNC, 0777);
		// fds[f] = __do_lwext4_create(NULL, fname.c_str(), 0777);
		// Note: If not removing files after writes are done, then just open
		// them here for reads
		fds[f] = __do_lwext4_open(NULL, fname.c_str(), 0777);
		if (fds[f] < 0) {
			logMessage(FS_LOG_LEVEL, "Error opening file: %s\n",
					   strerror(errno));
			return;
		}
		curr_open_end_time = __get_time();
		total_open_time_r += curr_open_end_time - curr_open_start_time;
	}

	// write data to new file
	// int wret = 0;
	// for (int f = 0; f < num_files2; f++) {
	// 	for (int i = 0; i < iterations_per_sample; i++) {
	// 		wret = __do_lwext4_write(NULL, fds[f], buf, write_sz, op_sz * i);
	// 		if (wret != write_sz) {
	// 			logMessage(
	// 				FS_LOG_LEVEL,
	// 				"Error during prealloc write: %d bytes written, %s\n", wret,
	// 				strerror(errno));
	// 			return;
	// 		}
	// 	}
	// }

	// preselect some offsets
	// iterations_per_sample basically determines the max possible byte
	// offset, so just make sure the return from get_rand_value is within that
	int *offs = new int[iterations_per_sample];
	for (int i = 0; i < iterations_per_sample; i++) {
		if (random_flag)
			offs[i] = op_sz * __get_random_value(0, iterations_per_sample - 1);
		else
			offs[i] = op_sz * i;
		// logMessage(
		// 	FS_LOG_LEVEL,
		// 	"fsz=%lu, op_sz=%lu, offs[i]=%lu, iterations_per_sample=%d\n", fsz,
		// 	op_sz, op_sz * __get_random_value(0, iterations_per_sample),
		// 	iterations_per_sample);
	}

	// do reads
	double curr_reads_start_time = __get_time();
	int rret = 0;
	for (int f = 0; f < num_files2; f++) {
		for (int i = 0; i < iterations_per_sample; i++) {
			// read_start_time =
			// 	std::chrono::time_point_cast<std::chrono::microseconds>(
			// 		std::chrono::high_resolution_clock::now())
			// 		.time_since_epoch()
			// 		.count();
			// logMessage(FS_LOG_LEVEL,"READ: sample [%d], iteration [%d]\n",
			// sample, i);
			rret = __do_lwext4_read(NULL, fds[f], buf, read_sz, offs[i]);
			if (rret != read_sz) {
				logMessage(FS_LOG_LEVEL, "Error during read: %s\n",
						   strerror(errno));
				return;
			}
			// read_end_time =
			// 	std::chrono::time_point_cast<std::chrono::microseconds>(
			// 		std::chrono::high_resolution_clock::now())
			// 		.time_since_epoch()
			// 		.count();
			// // logMessage(FS_LOG_LEVEL,"Read latency: %.3fμs\n",
			// read_end_time -
			// // read_start_time);
			// read_latencies.push_back(read_end_time - read_start_time);
			// read_lats.push_back(read_end_time - read_start_time);
		}
	}
	double curr_reads_end_time = __get_time();
	total_read_time += (curr_reads_end_time - curr_reads_start_time);
	delete[] offs;

	// // close the files
	double curr_close_start_time = __get_time();
	for (int f = 0; f < num_files2; f++) {
		if (__do_lwext4_release(NULL, fds[f]) != 0) {
			logMessage(FS_LOG_LEVEL, "Error closing file\n");
			abort();
		}
	}
	double curr_close_end_time = __get_time();
	total_close_time_r += (curr_close_end_time - curr_close_start_time);

	// delete them for next sample
	// for (int f = 0; f < num_files2; f++) {
	// 	fname = fstem + std::to_string(f);
	// 	if (__do_lwext4_unlink(NULL, fname.c_str()) != 0) {
	// 		logMessage(FS_LOG_LEVEL, "Error deleting file\n");
	// 		abort();
	// 	}
	// }
	delete buf;
}

// double __get_time() {
// 	double s = 0.0;
// 	if (ocall_get_time2(&s) != SGX_SUCCESS) {
// 		logMessage(LOG_ERROR_LEVEL, "Failed __get_time, aborting");
// 		abort();
// 	}
// 	return s;
// }

static void save_lats() {
	std::string __read_lats = vec_to_str<double>(read_lats);
	std::string __read_lats_fname("read_lats");
	write_lats_to_file(__read_lats, __read_lats_fname);
	logMessage(FS_LOG_LEVEL, "Read latencies (µs, count=%lu):\n[%s]\n",
			   read_lats.size(), __read_lats.c_str());

	std::string __write_lats = vec_to_str<double>(write_lats);
	std::string __write_lats_fname("write_lats");
	write_lats_to_file(__write_lats, __write_lats_fname);
	logMessage(FS_LOG_LEVEL, "Write latencies (µs, count=%lu):\n[%s]\n",
			   write_lats.size(), __write_lats.c_str());

	std::string __blk_accesses_r = vec_to_str<bfs_vbid_t>(blk_accesses[0]);
	std::string __blk_accesses_r_fname("blk_accesses_r");
	write_lats_to_file(__blk_accesses_r, __blk_accesses_r_fname);
	logMessage(FS_LOG_LEVEL, "Block accesses (reads, count=%lu):\n[%s]\n",
			   blk_accesses[0].size(), __blk_accesses_r.c_str());

	std::string __blk_accesses_w = vec_to_str<bfs_vbid_t>(blk_accesses[1]);
	std::string __blk_accesses_w_fname("blk_accesses_w");
	write_lats_to_file(__blk_accesses_w, __blk_accesses_w_fname);
	logMessage(FS_LOG_LEVEL, "Block accesses (writes, count=%lu):\n[%s]\n",
			   blk_accesses[1].size(), __blk_accesses_w.c_str());
}

static void write_lats_to_file(std::string l, std::string l_fname) {
	int ocall_ret = -1;
	if ((ocall_write_to_file(&ocall_ret, (uint32_t)l_fname.size() + 1,
							 l_fname.c_str(), (uint32_t)l.size() + 1,
							 l.c_str()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Error in ocall_write_to_file for [%s]",
				   l.c_str());
		abort();
	}
}

// void reset_perf_counters() {
// 	if (!bfsUtilLayer::perf_test())
// 		return;

// #ifdef __BFS_ENCLAVE_MODE
// 		// num_reads = 0;
// 		// num_writes = 0;
// #endif
// }

// void collect_measurements(int optype) {
// 	if (!bfsUtilLayer::perf_test())
// 		return;

// #ifdef __BFS_ENCLAVE_MODE
// 	switch (optype) {
// 	case CLIENT_READ_OP:
// 		e_blk_reads_per_fs_read_counts.push_back(num_reads);
// 		e_blk_writes_per_fs_read_counts.push_back(num_writes);
// 		break;
// 	case CLIENT_WRITE_OP:
// 		e_blk_reads_per_fs_write_counts.push_back(num_reads);
// 		e_blk_writes_per_fs_write_counts.push_back(num_writes);
// 		break;
// 	default:
// 		e_blk_reads_per_fs_other_counts.push_back(num_reads);
// 		e_blk_writes_per_fs_other_counts.push_back(num_writes);
// 		break;
// 	}

// 	reset_perf_counters();
// #endif
// }
