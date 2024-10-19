/**
 * @file bfs_fs_test.cpp
 * @brief Unit test definitions for bfs server and fs methods.
 */

#include <arpa/inet.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <pthread.h>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "bfs_acl.h"
#include "bfs_core.h"
#include "bfs_fs_layer.h"
#include "bfs_server.h"
#include <bfsBlockLayer.h>
#include <bfsConfigLayer.h>
#include <bfs_common.h>
#include <bfs_log.h>
#include <bfs_util.h>

#ifdef __BFS_DEBUG_NO_ENCLAVE
/* For non-enclave testing; just directly calls the ecall functions */
#include "bfs_core_test_ecalls.h"
#elif defined(__BFS_NONENCLAVE_MODE)
/* For making legitimate ecalls */
// #include <bfs_enclave_u.h>
#include "bfs_enclave_core_test_u.h"
#include <sgx_urts.h>
static sgx_enclave_id_t eid = 0;
#endif

#define CORE_TEST_LOG_LEVEL bfs_core_test_log_level
#define CORE_TEST_VRB_LOG_LEVEL bfs_core_test_vrb_log_level
#define CORE_TEST_CONFIG "bfsFsLayerTest"

static uint64_t bfs_core_test_log_level = 0;
static uint64_t bfs_core_test_vrb_log_level = 0;

// test parameters
static bool do_mkfs = false;
static uint64_t num_files = 0;
static uint64_t num_test_iterations = 0;
static uint64_t max_op_sz = 0, min_op_sz = 0;

static int bfs_unit__bfs_core_init() {
	bfsCfgItem *config;
	bool fstlog, fstvlog, log_to_file;
	std::string logfile;

	try {
		if (bfsUtilLayer::bfsUtilLayerInit() != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL, "Failed initialized util layer");
			return BFS_FAILURE;
		}

		config = bfsConfigLayer::getConfigItem(CORE_TEST_CONFIG);

		if (config->bfsCfgItemType() != bfsCfgItem_STRUCT) {
			logMessage(LOG_ERROR_LEVEL,
					   "Unable to find configuration in system config: %s",
					   CORE_TEST_CONFIG);
			return BFS_FAILURE;
		}

		fstlog = (config->getSubItemByName("log_enabled")->bfsCfgItemValue() ==
				  "true");
		bfs_core_test_log_level =
			registerLogLevel("CORE_TEST_LOG_LEVEL", fstlog);
		fstvlog = (config->getSubItemByName("log_verbose")->bfsCfgItemValue() ==
				   "true");
		bfs_core_test_vrb_log_level =
			registerLogLevel("CORE_TEST_VRB_LOG_LEVEL", fstvlog);
		log_to_file =
			(config->getSubItemByName("log_to_file")->bfsCfgItemValue() ==
			 "true");

		if (log_to_file) {
			logfile = config->getSubItemByName("logfile")->bfsCfgItemValue();
			initializeLogWithFilename(logfile.c_str());
		} else {
			initializeLogWithFilehandle(STDOUT_FILENO);
		}

		do_mkfs =
			(config->getSubItemByName("do_mkfs")->bfsCfgItemValue() == "true");

		num_files =
			config->getSubItemByName("num_files")->bfsCfgItemValueLong();
		num_test_iterations = config->getSubItemByName("num_test_iterations")
								  ->bfsCfgItemValueLong();
		max_op_sz =
			config->getSubItemByName("max_op_sz")->bfsCfgItemValueLong();

		min_op_sz =
			config->getSubItemByName("min_op_sz")->bfsCfgItemValueLong();

		logMessage(CORE_TEST_LOG_LEVEL, "Core test initialized.");
	} catch (bfsCfgError *e) {
		logMessage(LOG_ERROR_LEVEL, "Failure reading system config: %s",
				   e->getMessage().c_str());
		return BFS_FAILURE;
	}

	return BFS_SUCCESS;
}

#ifdef __BFS_DEBUG_NO_ENCLAVE
static uint32_t __bfs_unit__bfs_core_file() {
	// initialize the entire stack
	if (BfsFsLayer::bfsFsLayerInit() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed bfsFsLayerInit\n");
		return BFS_FAILURE;
	}

	// then get the device cluster running
	if (bfsBlockLayer::set_vbc(bfsVertBlockCluster::bfsClusterFactory()) !=
		BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed to initalize virtual block cluster, aborting.");
		return BFS_FAILURE;
	}

	if (!bfsConfigLayer::systemConfigLoaded()) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed to load system configuration, aborting.\n");
		return BFS_FAILURE;
	}

	BfsHandle *bfs_handle = new BfsHandle();

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

	BfsACLayer::add_user_context(0);
	BfsUserContext *test_usr = BfsACLayer::get_user_context(0);

	try {
		if (do_mkfs && (bfs_handle->mkfs() != BFS_SUCCESS)) {
			logMessage(LOG_ERROR_LEVEL, "Error during mkfs.\n");
			goto CLEANUP_FAIL;
		}

		while (bfs_handle->get_status() != FORMATTED)
			;

		if (bfs_handle->mount() != BFS_SUCCESS) {
			logMessage(CORE_TEST_LOG_LEVEL, "Error during mount.\n");
			goto CLEANUP_FAIL;
		}

		while (bfs_handle->get_status() != MOUNTED)
			;
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

	// create random files (only fail on fs issues)
	for (uint32_t i = 0; i < num_files; i++) {
		path = "/test" + std::to_string(i);

		try {
			fh = bfs_handle->bfs_create(test_usr, path, 0777);

			if (fh < START_FD) {
				logMessage(LOG_ERROR_LEVEL, "Error creating file [path=%s]\n",
						   path.c_str());
				goto CLEANUP_FAIL;
			} else {
				logMessage(CORE_TEST_VRB_LOG_LEVEL,
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
	for (uint32_t i = 0; i < num_files; i++) {
		path = "/test" + std::to_string(i);

		try {
			ret = bfs_handle->bfs_getattr(test_usr, path, &uid, &fino, &fmode,
										  &fsize);

			if (ret != BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL, "Error on file getattr [path=%s]\n",
						   path.c_str());
				goto CLEANUP_FAIL;
			} else {
				logMessage(CORE_TEST_VRB_LOG_LEVEL,
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
	struct timeval writes_start_time, writes_end_time;
	gettimeofday(&writes_start_time, NULL);
	for (uint32_t i = 0; i < num_test_iterations; i++) {
		// get a random fh from vector (handle,size,data)
		rix = get_random_value(0, (uint32_t)open_file_data.size() - 1);
		auto r = open_file_data.at(rix);
		wr_fh = std::get<0>(r);
		// get a random offset (allow write of at least 1 byte using `- 1`)
		if (std::get<1>(r) > 0) {
			wr_off = get_random_value(0, (uint32_t)std::get<1>(r) - 1 - 1);
		} else {
			wr_off = 0;
		}
		// get a random write size (at least 1 byte)
		wr_sz = get_random_value(min_op_sz, max_op_sz);

		// dont let file get too big for now; just try to another file
		if ((wr_off + wr_sz) > max_file_sz) {
			continue;
		}

		// get random data and put in the check buffers (in open_file_data)
		data = (char *)malloc(wr_sz);
		get_random_data(data, (uint32_t)wr_sz);
		if ((wr_off + wr_sz) > std::get<1>(r)) {
			std::get<2>(r) = (char *)realloc(std::get<2>(r), wr_off + wr_sz);
			std::get<1>(r) = wr_off + wr_sz; // update file size
		}
		memcpy(&((std::get<2>(r))[wr_off]), data, wr_sz); // copy check data in
		open_file_data.at(rix) = r;

		try {
			if ((ret = bfs_handle->bfs_write(test_usr, wr_fh, data, wr_sz,
											 wr_off)) != wr_sz) {
				logMessage(LOG_ERROR_LEVEL,
						   "Write fail [iteration=%d, fh=%d, size=%d, off=%d, "
						   "fsize=%d].\n",
						   i, wr_fh, wr_sz, wr_off, std::get<1>(r));
				goto CLEANUP_FAIL;
			} else {
				logMessage(CORE_TEST_VRB_LOG_LEVEL,
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
	gettimeofday(&writes_end_time, NULL);

	// do random reads
	struct timeval reads_start_time, reads_end_time;
	gettimeofday(&reads_start_time, NULL);
	for (uint32_t i = 0; i < num_test_iterations; i++) {
		// get a random fh from vector (handle,size,data)
		auto r = open_file_data.at(
			get_random_value(0, (uint32_t)open_file_data.size() - 1));
		r_fh = std::get<0>(r);

		// get a random offset (try to allow read of at least 1 with `- 1`)
		if (std::get<1>(r) > 0) {
			if ((uint32_t)std::get<1>(r) == 1)
				r_off = get_random_value(0, (uint32_t)std::get<1>(r) - 1);
			else
				r_off = get_random_value(0, (uint32_t)std::get<1>(r) - 1 - 1);
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
		r_sz = get_random_value(min_op_sz, max_op_sz);

		data = (char *)malloc(r_sz);
		memset(data, 0x0, r_sz);

		try {
			// dont care if read goes past end of file; should result in a short
			// read
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
					CORE_TEST_VRB_LOG_LEVEL,
					"Read short [iteration=%d, fh=%d, size=%d, ret=%d, off=%d, "
					"fsize=%d].\n",
					i, r_fh, r_sz, ret, r_off, std::get<1>(r));
			} else {
				logMessage(CORE_TEST_VRB_LOG_LEVEL,
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
				logMessage(CORE_TEST_VRB_LOG_LEVEL,
						   "write/read compare success.\n");
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
	gettimeofday(&reads_end_time, NULL);

	// close files
	for (auto of : open_file_data) {
		try {
			if ((ret = bfs_handle->bfs_release(test_usr, std::get<0>(of))) !=
				BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL, "Error during release [fh=%d].\n",
						   std::get<0>(of));
				goto CLEANUP_FAIL;
			} else {
				logMessage(CORE_TEST_VRB_LOG_LEVEL,
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
			if ((ret = bfs_handle->bfs_unlink(test_usr, path)) != BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL, "Error during unlink [path=%s].\n",
						   path.c_str());
				goto CLEANUP_FAIL;
			} else {
				logMessage(CORE_TEST_VRB_LOG_LEVEL,
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
	delete bfs_handle;
	bfs_handle = NULL;
	return BFS_FAILURE;

CLEANUP_OK:
	logMessage(CORE_TEST_LOG_LEVEL,
			   "Summary of file I/O performance for [bfs/non-sgx]:");
	logMessage(CORE_TEST_LOG_LEVEL, "   > Dentry cache hit rate: %.2f%%\n",
			   bfs_handle->get_dentry_cache().get_hit_rate() * 100.0);
	logMessage(CORE_TEST_LOG_LEVEL, "   > Inode cache hit rate: %.2f%%\n",
			   bfs_handle->get_ino_cache().get_hit_rate() * 100.0);
	logMessage(
		CORE_TEST_LOG_LEVEL,
		"   > Write throughput: (%.3f MB / %.3f s) %.3f MB/s",
		total_bytes_written / 1e6,
		((double)compareTimes(&writes_start_time, &writes_end_time) / 1e6),
		(total_bytes_written /
		 ((double)compareTimes(&writes_start_time, &writes_end_time) / 1e6)) /
			1e6);
	logMessage(
		CORE_TEST_LOG_LEVEL,
		"   > Read throughput: (%.3f MB / %.3f s) %.3f MB/s",
		total_bytes_read / 1e6,
		((double)compareTimes(&reads_start_time, &reads_end_time) / 1e6),
		(total_bytes_read /
		 ((double)compareTimes(&reads_start_time, &reads_end_time) / 1e6)) /
			1e6);

	delete bfs_handle;
	bfs_handle = NULL;

	return BFS_SUCCESS;
}

static void *do_start_server(void *) {
	if (server_init() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to init server for test\n");
		abort();
	}

	if (start_server() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to start server for test\n");
		abort();
	}

	return NULL;
}

/**
 * @brief Test client connecting to server and executing remote commands.
 *
 * @return uint32_t 0 if success, -1 if failure
 */
static uint32_t bfs_unit__bfs_server() {
	// start bfs server
	pthread_t server_thr;
	pthread_create(&server_thr, NULL, do_start_server, NULL);
	pthread_join(server_thr, NULL);

	// setup mux and connect to server

	return BFS_SUCCESS;
}
#endif

/**
 * @brief Unit test for bfs file operation methods. Creates random files,
 * executes random writes, then executes random reads and verifies the read
 * data against expected data.
 *
 * @return uint32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static uint32_t bfs_unit__bfs_core_file(int _random, uint64_t num_it,
										uint64_t fsz, uint64_t op_sz) {
	if (bfs_unit__bfs_core_init() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Error during bfs_unit__bfs_core_init.\n");
		return BFS_FAILURE;
	}

	logMessage(CORE_TEST_LOG_LEVEL, "Starting bfs_unit__bfs_core_file()...\n");

	// init enclave
	int ret = -1;
#ifdef __BFS_NONENCLAVE_MODE
	sgx_launch_token_t tok = {0};
	int tok_updated = 0;
	if (sgx_create_enclave(
			(std::string(getenv("BFS_HOME")) + std::string("/build/bin/") +
			 std::string(BFS_CORE_TEST_ENCLAVE_FILE))
				.c_str(),
			SGX_DEBUG_FLAG, &tok, &tok_updated, &eid, NULL) != SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to initialize enclave.");
		return BFS_FAILURE;
	}

	int64_t ecall_status = 0;
	if (((ecall_status = ecall_bfs_enclave_init(eid, &ret, 0)) !=
		 SGX_SUCCESS) ||
		(ret == BFS_FAILURE)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed during ecall_bfs_enclave_init. Error code: %d\n",
				   ecall_status != SGX_SUCCESS ? ecall_status : ret);
		return BFS_FAILURE;
	}
	logMessage(CORE_TEST_LOG_LEVEL, "Enclave successfully initialized. "
									"Starting bfs core file unit test...\n");

	// start the unit test from within the enclave
	ret = -1;
	ecall_status = 0;
	if (((ecall_status = ecall_bfs_start_core_file_test_simple(
			  eid, &ret, _random, num_it, fsz, op_sz)) != SGX_SUCCESS) ||
		(ret == BFS_FAILURE)) {
		logMessage(
			LOG_ERROR_LEVEL,
			"Failed during ecall_bfs_start_core_file_test. Error code: %d\n",
			ecall_status != SGX_SUCCESS ? ecall_status : ret);
		return BFS_FAILURE;
	}

	// When we have a shutdown method, we will add it here
	// TODO: add layer shutdowm method
	sgx_status_t enclave_status = SGX_SUCCESS;
	if ((enclave_status = sgx_destroy_enclave(eid)) != SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to destroy enclave: %d\n",
				   enclave_status);
		return BFS_FAILURE;
	}

	logMessage(CORE_TEST_LOG_LEVEL, "Destroyed enclave successfully.\n");
#elif defined(__BFS_DEBUG_NO_ENCLAVE)
	// start the unit test in debug mode
	logMessage(CORE_TEST_LOG_LEVEL, "Starting bfs core file unit test...\n");
	ret = __bfs_unit__bfs_core_file();
#endif

	logMessage(CORE_TEST_LOG_LEVEL, "Server shut down complete.");

	return ret;
}

/**
 * @brief Test reading/writing blocks from fs code (using merkle tree etc.).
 * Only supports testing in enclave mode for now.
 *
 * @return uint32_t 0 if success, -1 if failure
 */
int bfs_unit__bfs_core_blk() {
	// init stuff for logging
	if (bfs_unit__bfs_core_init() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Error during bfs_unit__bfs_core_init.\n");
		return BFS_FAILURE;
	}

	logMessage(CORE_TEST_LOG_LEVEL, "Starting bfs_unit__bfs_core_blk()...\n");

	// init enclave
#ifdef __BFS_NONENCLAVE_MODE
	sgx_launch_token_t tok = {0};
	int tok_updated = 0;
	if (sgx_create_enclave(
			(std::string(getenv("BFS_HOME")) + std::string("/build/bin/") +
			 std::string(BFS_CORE_TEST_ENCLAVE_FILE))
				.c_str(),
			SGX_DEBUG_FLAG, &tok, &tok_updated, &eid, NULL) != SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to initialize enclave.");
		return BFS_FAILURE;
	}

	int ret = -1;
	int64_t ecall_status = 0;
	if (((ecall_status = ecall_bfs_enclave_init(eid, &ret, 1)) !=
		 SGX_SUCCESS) ||
		(ret == BFS_FAILURE)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed during ecall_bfs_enclave_init. Error code: %d\n",
				   ecall_status != SGX_SUCCESS ? ecall_status : ret);
		return BFS_FAILURE;
	}
	logMessage(CORE_TEST_LOG_LEVEL, "Enclave successfully initialized. "
									"Starting bfs core blk unit test...\n");

	// start the unit test from within the enclave
	ret = -1;
	ecall_status = 0;
	if (((ecall_status = ecall_bfs_start_core_blk_test(
			  eid, &ret, (uint64_t)num_test_iterations)) != SGX_SUCCESS) ||
		(ret == BFS_FAILURE)) {
		logMessage(
			LOG_ERROR_LEVEL,
			"Failed during ecall_bfs_start_core_blk_test. Error code: %d\n",
			ecall_status != SGX_SUCCESS ? ecall_status : ret);
		return BFS_FAILURE;
	}

	// When we have a shutdown method, we will add it here
	// TODO: add layer shutdowm method
	sgx_status_t enclave_status = SGX_SUCCESS;
	if ((enclave_status = sgx_destroy_enclave(eid)) != SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to destroy enclave: %d\n",
				   enclave_status);
		return BFS_FAILURE;
	}

	logMessage(CORE_TEST_LOG_LEVEL, "Destroyed enclave successfully.\n");
#else
	// start the unit test in debug mode
	logMessage(CORE_TEST_LOG_LEVEL, "Starting bfs core blk unit test...\n");
	// TODO
#endif

	logMessage(CORE_TEST_LOG_LEVEL, "Server shut down complete.");

	return BFS_SUCCESS;
}

int main(int argc, char **argv) {
	const char *BFS_CORE_TEST_ARGS = "csbrn:f:o:";
	int ch = 0, ret = 0;
	bool do_core_test = false, do_server_test = false, do_core_blk_test = false;
	int _random = 0;
	(void)do_core_test;
	(void)do_server_test;
	(void)do_core_blk_test;

	uint64_t num_it = 0, fsz = 0, op_sz = 0;

	// Process the command line parameters
	while ((ch = getopt(argc, argv, BFS_CORE_TEST_ARGS)) != -1) {
		switch (ch) {
		case 'c': // core test flag
			do_core_test = true;
			break;
		case 's': // Server test flag
			do_server_test = true;
			break;
		case 'b': // Server test flag
			do_core_blk_test = true;
			break;
		case 'r': // Random test flag
			_random = 1;
			break;
		case 'n':
			num_it = atoi(optarg);
			break;
		case 'f':
			fsz = atoi(optarg);
			break;
		case 'o':
			op_sz = atoi(optarg);
			break;
		default: // Default (unknown)
			fprintf(stderr, "Unknown command line option (%c), aborting.", ch);
			exit(-1);
		}
	}

	if (do_core_test) {
		if ((ret = bfs_unit__bfs_core_file(_random, num_it, fsz, op_sz)) !=
			BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL,
					   "\033[91mBfs core unit test failed.\033[0m\n");
		} else {
			logMessage(
				CORE_TEST_LOG_LEVEL,
				"\033[93mBfs core unit test completed successfully.\033[0m\n");
		}
	}

#ifdef __BFS_DEBUG_NO_ENCLAVE
	if (do_server_test) {
		if ((ret = bfs_unit__bfs_server()) != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL,
					   "\033[91mBfs server unit test failed.\033[0m\n");
		} else {
			logMessage(CORE_TEST_LOG_LEVEL, "\033[93mBfs server unit test "
											"completed successfully.\033[0m\n");
		}
	}
#endif

	if (do_core_blk_test) {
		if ((ret = bfs_unit__bfs_core_blk()) != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL,
					   "\033[91mBfs core block unit test failed.\033[0m\n");
		} else {
			logMessage(CORE_TEST_LOG_LEVEL, "\033[93mBfs core block unit test "
											"completed successfully.\033[0m\n");
		}
	}

	return ret;
}
