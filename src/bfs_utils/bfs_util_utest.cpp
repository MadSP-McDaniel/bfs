////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfs_util_utest.cpp
//  Description   : This is the main program for utility unit tests.
//
//  Author        : Patrick McDaniel
//  Last Modified : Sat 01 May 2021 10:33:05 AM EDT
//

// Include Files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Project Includes
#include <bfsConfigLayer.h>
#include <bfsCryptoError.h>
#include <bfsCryptoKey.h>
#include <bfsCryptoLayer.h>
#include <bfsFlexibleBuffer.h>
#include <bfsRegExpression.h>
#include <bfsRegExpressionError.h>
#include <bfsUtilLayer.h>
#include <bfs_base64.h>
#include <bfs_cache.h>
#include <bfs_log.h>
#include <bfs_util.h>

// Defines
#define BFSUTILTEST_ARGUMENTS "hvucfpxkrl"
#define USAGE                                                                  \
	"USAGE: bfs_unit_utest [-h] [-v] [-c|f|r]>n"                               \
	"\n"                                                                       \
	"where:\n"                                                                 \
	"    -h - help mode (display this message)\n"                              \
	"    -v - verbose mode\n"                                                  \
	"    -c - do cache unit test\n"                                            \
	"    -f - do flex buffer unit test\n"                                      \
	"    -p - do config unit test\n"                                           \
	"    -x - do crypto unit test\n"                                           \
	"    -k - generate a random key and display in b64 (using crypto utils)\n" \
	"    -r - do regular expression unit test\n"                               \
	"    -l - do latency test\n"                                               \
	"\n"

//
// Functional Prototypes

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : main
// Description  : The main function for the AV weather program
//
// Inputs       : argc - the number of command line parameters
//                argv - the parameters
// Outputs      : 0 if successful test, -1 if failure

int main(int argc, char *argv[]) {
	// Local variables
	bfsCryptoKey *key;
	int ch;
	bool verbose = false, do_cache_test = false, do_flex_test = false,
		 do_config_test = false, do_crypto_test = false, do_regexp_test = false,
		 do_bridge_latency_test = false;

	(void)do_cache_test;
	(void)do_flex_test;
	(void)do_config_test;
	(void)do_crypto_test;
	(void)do_regexp_test;
	(void)do_bridge_latency_test;

	// Process the command line parameters
	while ((ch = getopt(argc, argv, BFSUTILTEST_ARGUMENTS)) != -1) {

		switch (ch) {
		case 'h': // Help, print usage
			fprintf(stderr, USAGE);
			return (-1);

		case 'v': // verbose flag
			verbose = true;
			break;

		case 'c': // cache unit test
			do_cache_test = true;
			break;

		case 'f': // flex buffer unit test
			do_flex_test = true;
			break;

		case 'p': // config unit test
			do_config_test = true;
			break;

		case 'x': // crypto unit test
			do_crypto_test = true;
			break;

		case 'l': // ecall latency test
			do_bridge_latency_test = true;
			break;

		case 'k': // Key generation flag (using crypto utils)
			key = bfsCryptoKey::createRandomKey();
			fprintf(stdout, "Generated key : %s\n", key->toBase64().c_str());
			exit(0);

		case 'r': // regular expression
			do_regexp_test = true;
			break;

		default: // Default (unknown)
			fprintf(stderr, "Unknown command line option (%c), aborting.", ch);
			fprintf(stderr, USAGE);
			exit(-1);
		}
	}

	// If verbose turn on the config layer verbose flag
	initializeLogWithFilehandle(0);
	if (verbose) {
		enableLogLevels(LOG_INFO_LEVEL);
	}

	// Now do the unit tests
	try {

		bfsUtilLayer::bfsUtilLayerInit();

		logMessage(LOG_INFO_LEVEL, "Executing utility unit tests.");

#ifdef __BFS_DEBUG_NO_ENCLAVE
		if (do_regexp_test && !bfsRegExpression::unitTest()) {
			logMessage(LOG_ERROR_LEVEL,
					   "bfs cache unit tests failed, aborting.");
			return (-1);
		}

		logMessage(LOG_ERROR_LEVEL, "Fix this");

		if (do_cache_test && !BfsCache::unitTest()) {
			logMessage(LOG_ERROR_LEVEL,
					   "bfs cache unit tests failed, aborting.");
			return (-1);
		}

		if (do_flex_test && ((bfsFlexibleBuffer::flexBufferUTest() != 0) ||
							 (bfs_base64_utest() != 0))) {
			logMessage(LOG_ERROR_LEVEL,
					   "bfs flex buffer tests failed, aborting.");
			return (-1);
		}

		if (do_config_test && (bfsConfigLayer::bfsConfigLayerUtest() != 0)) {
			logMessage(LOG_ERROR_LEVEL,
					   "bfs config unit tests failed, aborting.");
			return (-1);
		}
#endif

		if (do_crypto_test) {
			try {

#ifdef __BFS_DEBUG_NO_ENCLAVE
				if (bfsCryptoLayer::bfsCryptoLayerUtest() != 0) {
					logMessage(LOG_ERROR_LEVEL,
							   "bfs Crypto layer unit tests failed, aborting.");
					return (-1);
				}
#elif defined(__BFS_NONENCLAVE_MODE)
				if (bfsCryptoLayer::bfsCryptoLayerUtest__enclave() != 0) {
					logMessage(LOG_ERROR_LEVEL, "bfs Crypto layer enclave unit "
												"tests failed, aborting.");
					return (-1);
				}
#endif

			} catch (bfsCryptoError *err) {
				logMessage(LOG_ERROR_LEVEL,
						   "Crypto layer utest threw exception [%s], aborting",
						   err->getMessage().c_str());
				delete err;
				return (-1);
			}
		}

#ifdef __BFS_NONENCLAVE_MODE
		if (do_bridge_latency_test) {
			if (bfsUtilLayer::bridge_latency_test() != BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL,
						   "bfs Crypto layer unit tests failed, aborting.");
				return (-1);
			}
		}
#endif

	} catch (bfsRegExpressionError *err) {
		logMessage(LOG_ERROR_LEVEL,
				   "Regular expression utest threw exception [%s], aborting",
				   err->getMessage().c_str());
		delete err;
		return (-1);
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL,
				   "Flex buffer utest threw exception [%s], aborting",
				   err->getMessage().c_str());
		delete err;
		return (-1);
	} catch (exception *e) {
		logMessage(LOG_ERROR_LEVEL, "utest threw exception, aborting");
		delete e;
		return (-1);
	}

	// Log, exit succesfully
	logMessage(LOG_INFO_LEVEL, "Utility unit tests completed successfully.");
	return (0);
}
