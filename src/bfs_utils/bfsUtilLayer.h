#ifndef BFS_UTIL_LAYER_INCLUDED
#define BFS_UTIL_LAYER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsUtilLayer.h
//  Description   : This is the class describing util layer interface
//                  for the bfs file system.  Note that this is static class
//                  in which there are no objects.
//
//  Author  : Patrick McDaniel
//  Created : Fri 30 Apr 2021 05:12:47 PM EDT
//

// STL-isms
#include <vector>

// Project Includes
//
// Class definitions
#define UTIL_LOG_LEVEL bfsUtilLayer::getUtilLayerLogLevel()
#define UTIL_VRBLOG_LEVEL bfsUtilLayer::getVerboseUtilLayerLogLevel()
#define UTIL_CACHE_MAX_SZ bfsUtilLayer::getUtilLayerCacheSizeLimit()
#define BFS_UTILLYR_CONFIG "bfsUtilLayer"
#ifdef __BFS_NONENCLAVE_MODE
#define BFS_UTIL_TEST_ENCLAVE_FILE "libbfs_util_test_enclave.signed.so"
#endif

//
// Class Definition

class bfsUtilLayer {

public:
	//
	// Static methods

	static int bfsUtilLayerInit(void);
	// Initialize the util layer

	//
	// Static Class Variables

	static long getUtilLayerCacheSizeLimit(void) {
		return (bfs_cache_sz_limit);
	}

	// Layer log level
	static unsigned long getUtilLayerLogLevel(void) {
		return (bfsUtilLogLevel);
	}

	// Verbose log level
	static unsigned long getVerboseUtilLayerLogLevel(void) {
		return (bfsVerboseUtilLogLevel);
	}

	static bool cache_enabled() { return bfs_cache_enabled; }

	static bool perf_test() { return bfs_perf_test; }

	static bool use_mt() { return use_merkle_tree; }

    static bool journal_enabled() { return _journal_enabled; }

	static int bridge_latency_test();

private:
	//
	// Private class methods

	bfsUtilLayer(void) {}
	// Default constructor (prevents creation of any instance)

	//
	// Static Class Variables

	static unsigned long bfsUtilLogLevel;
	// The log level for all of the util information

	static unsigned long bfsVerboseUtilLogLevel;
	// The log level for all of the util information

	static bool bfsUtilLayerInitialized;
	// The flag indicating the util layer is initialized

	static bool bfs_cache_enabled;
	// Flag indicating if caches are enabled or not in the system

	static bool bfs_perf_test;
	static bool use_merkle_tree;
    static bool _journal_enabled;
	// Flag indicating if the system should run in performance testing mode
	// (collects timing measurements)

	static long bfs_cache_sz_limit;
	// The maximum number of cache entries
};

#endif
