////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfs_device_main.cpp 
//  Description   : This is the main function for the BFS device implementation
//                  which provides a memory storage device for the BFS system.
//
//   Author        : Patrick McDaniel
//   Last Modified : Wed 17 Mar 2021 03:40:50 PM EDT
//

// Include Files
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

// Project Include Files
#include <bfs_log.h>
#include <bfsNetworkDevice.h>
#include <bfsDeviceError.h>
#include <bfsConfigLayer.h>

// Defines
#define BFSDEVICE_ARGUMENTS "vhl:d:"
#define USAGE \
	"USAGE: bfs_device [-h] [-v] [-l <logfile>] -p <port> -d <did> b <blocks>\n" \
	"\n" \
	"where:\n" \
	"    -h - help mode (display this message)\n" \
	"    -v - verbose output\n" \
	"    -l - write log messages to the filename <logfile>\n" \
	"    -d - the device ID (mandatory, must be unique).\n" \
	"\n" \

// Global data

// Functional Prototypes

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : main
// Description  : The main function for the BFS block device.
//
// Inputs       : argc - the number of command line parameters
//                argv - the parameters
// Outputs      : 0 if successful, -1 if failure

int main( int argc, char *argv[] ) {

	// Local variables
	int ch, verbose = 0, log_initialized = 0;
	bfs_device_id_t did  = 0;
	bfsNetworkDevice * device;

	// Process the command line parameters
	while ((ch = getopt(argc, argv, BFSDEVICE_ARGUMENTS)) != -1) {

		switch (ch) {
		case 'h': // Help, print usage
			fprintf( stderr, USAGE );
			return( -1 );

		case 'v': // Verbose Flag
			verbose = 1;
			break;

		case 'l': // Set the log filename
			initializeLogWithFilename( optarg );
			log_initialized = 1;
			break;

		case 'd': // Set the device ID
			if ( sscanf(optarg, "%u", &did) != 1 ) {
				fprintf( stderr, "Bad device identifier [%s]", optarg );
				return(-1);
			}
			break;

		default:  // Default (unknown)
			fprintf( stderr, "Unknown command line option, aborting.\n" );
			return( -1 );
		}
	}

	// Setup the log as needed
	if ( ! log_initialized ) {
		initializeLogWithFilehandle( STDERR_FILENO );
	}
	if ( verbose ) {
		enableLogLevels( LOG_INFO_LEVEL );
	}

	// Confirm we have enough information to proceed
	if ( did == 0 ) {
		fprintf( stderr, "Missing device ID parameter [-d], cannot execute bfs_device, aborting.\n" );
		fprintf( stderr, USAGE );
		return( -1 );
	}

	// Now execute the device implementation
	try {

		// Load the system configuration
		bfsDeviceLayer::bfsDeviceLayerInit();
		if ( bfsConfigLayer::systemConfigLoaded() == false ) {
			fprintf( stderr, "Failed to load system configuration, aborting.\n" );
			return( -1 );
		}

		// Now create the device and execute it
		device = new bfsNetworkDevice( did );
		device->execute();
        logMessage(DEVICE_LOG_LEVEL, "Device shut down complete.");
		
	} catch (bfsDeviceError * e) {
		logMessage( LOG_ERROR_LEVEL, "BFS device threw device exception [%s], aborting", e->getMessage().c_str() );
		delete e;
		exit( -1 );
	}

	// Return successfully
	return( 0 );
}
