////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsUtilLayer.cpp
//  Description   : This is the class implementing the util layer interface
//                  for the bfs file system.  Note that this is static class
//                  in which there are no objects.
//
//  Author  : Patrick McDaniel
//  Created : Fri 30 Apr 2021 05:12:47 PM EDT
//

// Include files

// Project include files
#include <bfsConfigLayer.h>
#include <bfsCryptoLayer.h>
#include <bfsUtilLayer.h>
#include <bfs_log.h>
#include <bfs_util.h>

#ifdef __BFS_DEBUG_NO_ENCLAVE
/* For non-enclave testing; just directly call the ecall function */
#include "bfs_util_ecalls.h"
#elif defined(__BFS_NONENCLAVE_MODE)
/* For making legitimate ecalls */
#include "bfs_enclave_u.h"
#include "sgx_urts.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <libgen.h>
#include <sstream>
static sgx_enclave_id_t eid = 0; // for testing with enclave
#endif

//
// Class Data

// Create the static class data
unsigned long bfsUtilLayer::bfsUtilLogLevel = (unsigned long)0;
unsigned long bfsUtilLayer::bfsVerboseUtilLogLevel = (unsigned long)0;
long bfsUtilLayer::bfs_cache_sz_limit = (long)0;
bool bfsUtilLayer::bfs_cache_enabled = false;
bool bfsUtilLayer::bfs_perf_test = false;
bool bfsUtilLayer::use_merkle_tree = false;
bool bfsUtilLayer::_journal_enabled = false;

// Static initializer, make sure this is idenpendent of other layers
bool bfsUtilLayer::bfsUtilLayerInitialized = false;

//
// Class Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::bfsUtilLayerInit
// Description  : Initialize the util layer
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int bfsUtilLayer::bfsUtilLayerInit(void) {
	// TODO: read this from config
	// TODO: do util init for nonenclave and enclave code
	bfsCfgItem *config;
	bool utillog, vrblog;

	if (bfsUtilLayerInitialized)
		return BFS_SUCCESS;

	// Always init config first so that the rest of the layers can read the
	// system config file
	if (bfsConfigLayer::bfsConfigLayerInit() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed initialized config layer");
		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	const long flag_max_len = 10;
	char log_enabled_flag[flag_max_len] = {0},
		 log_verbose_flag[flag_max_len] = {0},
		 cache_enabled_flag[flag_max_len] = {0},
		 perf_test_flag[flag_max_len] = {0}, mt_flag[flag_max_len] = {0},
		 journal_flag[flag_max_len] = {0};
	bfsCfgItem *subcfg;
	int64_t ret = 0, ocall_status = 0, _cache_sz_limit = -1;

	if (((ocall_status = ocall_getConfigItem(&ret, BFS_UTILLYR_CONFIG,
											 strlen(BFS_UTILLYR_CONFIG) + 1)) !=
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
				   "Unable to find util configuration in system config "
				   "(ocall_bfsCfgItemType) : %s",
				   BFS_UTILLYR_CONFIG);
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
									flag_max_len)) != SGX_SUCCESS) ||
		(ret != BFS_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
		return (-1);
	}
	utillog = (std::string(log_enabled_flag) == "true");
	bfsUtilLogLevel = registerLogLevel("UTIL_LOG_LEVEL", utillog);

	subcfg = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&subcfg, (int64_t)config, "log_verbose",
			  strlen("log_verbose") + 1)) != SGX_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
		return (-1);
	}
	if (((ocall_status =
			  ocall_bfsCfgItemValue(&ret, (int64_t)subcfg, log_verbose_flag,
									flag_max_len)) != SGX_SUCCESS) ||
		(ret != BFS_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
		return (-1);
	}
	vrblog = (std::string(log_verbose_flag) == "true");
	bfsVerboseUtilLogLevel = registerLogLevel("UTIL_VRBLOG_LEVEL", vrblog);

	subcfg = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&subcfg, (int64_t)config, "cache_sz_limit",
			  strlen("cache_sz_limit") + 1)) != SGX_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
		return (-1);
	}
	_cache_sz_limit = -1;
	if (((ocall_status = ocall_bfsCfgItemValueLong(
			  &_cache_sz_limit, (int64_t)subcfg)) != SGX_SUCCESS) ||
		(_cache_sz_limit == -1)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValueLong");
		return (-1);
	}
	bfs_cache_sz_limit = _cache_sz_limit;

	subcfg = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&subcfg, (int64_t)config, "cache_enabled",
			  strlen("cache_enabled") + 1)) != SGX_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
		return (-1);
	}
	if (((ocall_status =
			  ocall_bfsCfgItemValue(&ret, (int64_t)subcfg, cache_enabled_flag,
									flag_max_len)) != SGX_SUCCESS) ||
		(ret != BFS_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
		return (-1);
	}
	bfs_cache_enabled = (std::string(cache_enabled_flag) == "true");

	if (((ocall_status = ocall_getConfigItem(&ret, BFS_COMMON_CONFIG,
											 strlen(BFS_COMMON_CONFIG) + 1)) !=
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
				   "Unable to find util configuration in system config "
				   "(ocall_bfsCfgItemType) : %s",
				   BFS_COMMON_CONFIG);
		return (-1);
	}

	subcfg = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&subcfg, (int64_t)config, "perf_test",
			  strlen("perf_test") + 1)) != SGX_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
		return (-1);
	}
	if (((ocall_status = ocall_bfsCfgItemValue(&ret, (int64_t)subcfg,
											   perf_test_flag, flag_max_len)) !=
		 SGX_SUCCESS) ||
		(ret != BFS_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
		return (-1);
	}
	bfs_perf_test = (std::string(perf_test_flag) == "true");

	subcfg = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&subcfg, (int64_t)config, "merkle_tree",
			  strlen("merkle_tree") + 1)) != SGX_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
		return (-1);
	}
	if (((ocall_status = ocall_bfsCfgItemValue(&ret, (int64_t)subcfg, mt_flag,
											   flag_max_len)) != SGX_SUCCESS) ||
		(ret != BFS_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
		return (-1);
	}
	use_merkle_tree = (std::string(mt_flag) == "true");

	subcfg = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&subcfg, (int64_t)config, "journal",
			  strlen("journal") + 1)) != SGX_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
		return (-1);
	}
	if (((ocall_status = ocall_bfsCfgItemValue(&ret, (int64_t)subcfg,
											   journal_flag, flag_max_len)) !=
		 SGX_SUCCESS) ||
		(ret != BFS_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
		return (-1);
	}
	_journal_enabled = (std::string(journal_flag) == "true");
#else
	config = bfsConfigLayer::getConfigItem(BFS_UTILLYR_CONFIG);
	if (config->bfsCfgItemType() != bfsCfgItem_STRUCT) {
		logMessage(LOG_ERROR_LEVEL,
				   "Unable to find util configuration in system config : %s",
				   BFS_UTILLYR_CONFIG);
		return (-1);
	}

	// Setup the logging
	utillog =
		(config->getSubItemByName("log_enabled")->bfsCfgItemValue() == "true");
	bfsUtilLogLevel = registerLogLevel("UTIL_LOG_LEVEL", utillog);
	vrblog =
		(config->getSubItemByName("log_verbose")->bfsCfgItemValue() == "true");
	bfsVerboseUtilLogLevel = registerLogLevel("UTIL_VRBLOG_LEVEL", vrblog);

	bfs_cache_sz_limit =
		config->getSubItemByName("cache_sz_limit")->bfsCfgItemValueLong();
	bfs_cache_enabled =
		(config->getSubItemByName("cache_enabled")->bfsCfgItemValue() ==
		 "true");

	// Get common configs
	config = bfsConfigLayer::getConfigItem(BFS_COMMON_CONFIG);
	if (config->bfsCfgItemType() != bfsCfgItem_STRUCT) {
		logMessage(LOG_ERROR_LEVEL,
				   "Unable to find configuration in system config: %s",
				   BFS_COMMON_CONFIG);
		return BFS_FAILURE;
	}

	bfs_perf_test =
		(config->getSubItemByName("perf_test")->bfsCfgItemValue() == "true");

	use_merkle_tree =
		(config->getSubItemByName("merkle_tree")->bfsCfgItemValue() == "true");

	_journal_enabled =
		(config->getSubItemByName("journal")->bfsCfgItemValue() == "true");
#endif
	if (bfsCryptoLayer::bfsCryptoLayerInit() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed initialized crypto layer");
		return BFS_FAILURE;
	}

	// Log the util layer being initialized, return successfully
	bfsUtilLayerInitialized = true;
	logMessage(UTIL_LOG_LEVEL, "bfsUtilLayer initialized. ");

	return BFS_SUCCESS;
}

#ifdef __BFS_NONENCLAVE_MODE
/**
 * @brief Gets latency measurement for enclave bridge functions.
 *
 * @return int: 0 if success, -1 if failure
 */
int bfsUtilLayer::bridge_latency_test() {
	/* For collecting perf results */
	const int NUM_ITERS = 1000;
	static std::vector<long> ecall_latencies;
	struct timeval ecall_start_time, ecall_end_time;

	// Create enclave
	sgx_launch_token_t tok = {0};
	int tok_updated = 0;
	if (sgx_create_enclave(
			(std::string(getenv("BFS_HOME")) + std::string("/build/bin/") +
			 std::string(BFS_UTIL_TEST_ENCLAVE_FILE))
				.c_str(),
			SGX_DEBUG_FLAG, &tok, &tok_updated, &eid, NULL) != SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to initialize enclave.");
		return BFS_SUCCESS;
	}

	logMessage(LOG_INFO_LEVEL, "Enclave successfully initialized.\n"
							   "Starting bfs enclave latency test...\n");

	// Run tests
	int64_t ecall_status = 0;

	for (int i = 0; i < NUM_ITERS; i++) {
		gettimeofday(&ecall_start_time, NULL);

		if ((ecall_status = empty_util_ecall(eid)) != SGX_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL,
					   "Failed during empty_util_ecall. Error code: %d\n",
					   ecall_status);
			return BFS_FAILURE;
		}

		gettimeofday(&ecall_end_time, NULL);
		ecall_latencies.push_back(
			compareTimes(&ecall_start_time, &ecall_end_time));
	}

	// Cleanup
	sgx_status_t enclave_status = SGX_SUCCESS;
	if ((enclave_status = sgx_destroy_enclave(eid)) != SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to destroy enclave: %d\n",
				   enclave_status);
		return BFS_FAILURE;
	}

	// Log results
	std::string read_lats = vec_to_str(ecall_latencies);
	std::string read_lats_fname(getenv("BFS_HOME"));
	read_lats_fname += "/benchmarks/micro/output/ecall_lats.csv";
	std::ofstream read_lats_f;
	read_lats_f.open(read_lats_fname.c_str(), std::ios::trunc);
	read_lats_f << read_lats.c_str();
	read_lats_f.close();
	logMessage(LOG_INFO_LEVEL, "Ecall latencies (us, %lu records):\n[%s]\n",
			   ecall_latencies.size(), read_lats.c_str());

	logMessage(
		LOG_INFO_LEVEL,
		"\033[93mBfs latency unit test completed successfully.\033[0m\n");
	return BFS_SUCCESS;
}
#endif
