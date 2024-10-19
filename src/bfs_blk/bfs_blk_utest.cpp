////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfs_blk_utest.cpp
//  Description   : This is the main function for the BFS block layer test
//                  which provides a memory storage device for the BFS system.
//
//   Author        : Patrick McDaniel
//   Last Modified : Tue 30 Mar 2021 07:41:43 AM EDT
//

// Include Files
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Project Include Files
#include <bfsBlockLayer.h>
#include <bfs_log.h>
#include <bfs_util.h>

// Defines
#define BFSBLOCKUT_ARGUMENTS "vhl:p:d:b:"
#define USAGE                                                                  \
	"USAGE: bfs_device [-h] [-v] [-l <logfile>]\n"                             \
	"\n"                                                                       \
	"where:\n"                                                                 \
	"    -h - help mode (display this message)\n"                              \
	"    -v - verbose output\n"                                                \
	"    -l - write log messages to the filename <logfile>\n"                  \
	"\n"

// Global data

// Functional Prototypes

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : main
// Description  : The main function for the BFS block layer unit test.
//
// Inputs       : argc - the number of command line parameters
//                argv - the parameters
// Outputs      : 0 if successful, -1 if failure

int main(int argc, char *argv[]) {

	// Local variables
	int ch, verbose = 0, log_initialized = 0;

	// Process the command line parameters
	while ((ch = getopt(argc, argv, BFSBLOCKUT_ARGUMENTS)) != -1) {

		switch (ch) {
		case 'h': // Help, print usage
			fprintf(stderr, USAGE);
			return (-1);

		case 'v': // Verbose Flag
			verbose = 1;
			break;

		case 'l': // Set the log filename
			initializeLogWithFilename(optarg);
			log_initialized = 1;
			break;

		default: // Default (unknown)
			fprintf(stderr, "Unknown command line option, aborting.\n");
			return (-1);
		}
	}

	// Setup the log as needed
	if (!log_initialized) {
		initializeLogWithFilehandle(STDERR_FILENO);
	}
	if (verbose) {
		enableLogLevels(LOG_INFO_LEVEL);
	}

	// bfsBlockLayer::bfsBlockLayerInit();

	// Call the UNIT test code, check for error
// #if 0
    if ( bfsBlockLayer::bfsBlockLayerUtest() ) {
        logMessage( LOG_ERROR_LEVEL, "BFS block layer failed, aborting." );
        return( -1 );
    }
// #endif

	// Return successfully
	return (0);
}
