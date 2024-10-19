////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfs_dev_utest.cpp 
//  Description   : This is the main function for the BFS device unit test
//                  which provides a memory storage device for the BFS system.
//
//   Author        : Patrick McDaniel
//   Last Modified : Wed 17 Mar 2021 03:40:50 PM EDT
//

// Include Files
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// STL Includes
#include <algorithm>
using namespace std;

// Project Include Files
#include <bfs_log.h>
#include <bfs_util.h>
#include <bfsDeviceLayer.h>
#include <bfsDeviceError.h>
#include <bfsConfigLayer.h>

// Defines
#define BFSDEVICEUT_ARGUMENTS "vhl:p:d:b:"
#define USAGE \
	"USAGE: bfs_device [-h] [-v] [-l <logfile>]\n" \
	"\n" \
	"where:\n" \
	"    -h - help mode (display this message)\n" \
	"    -v - verbose output\n" \
	"    -l - write log messages to the filename <logfile>\n" \
	"\n" 
#define BFS_DEV_UNIT_TEST_SLOTS 256
#define BFS_DEV_UNIT_TEST_ITERATIONS 1024
#define BFS_DEV_UTEST_SLOTS 10
#define BFS_UTEST_UNUSED (uint16_t)-1

// Global data

// Functional Prototypes
int bfsDeviceLayerUnitTest( void );

// 
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : main
// Description  : The main function for the BFS device unit test.
//
// Inputs       : argc - the number of command line parameters
//                argv - the parameters
// Outputs      : 0 if successful, -1 if failure

int main( int argc, char *argv[] ) {

	// Local variables
	int ch, verbose = 0, log_initialized = 0;

	// Process the command line parameters
	while ((ch = getopt(argc, argv, BFSDEVICEUT_ARGUMENTS)) != -1) {

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

    try {

        // Check to make sure we were able to load the configuration
        bfsDeviceLayer::bfsDeviceLayerInit();
        if ( bfsConfigLayer::systemConfigLoaded() == false ) {
            fprintf( stderr, "Failed to load system configuration, aborting.\n" );
            return( -1 );
        }
        // TODO: MUCH OF THIS BLOCK NEED TO BE MOVED TO A GLOBAL INIT FUNCTION
        
        // Call the UNIT test code, check for error
        if ( bfsDeviceLayerUnitTest() ) {
            logMessage( LOG_ERROR_LEVEL, "BFS device layer failed, aborting." );
            return( -1 );
        }

   	} catch (bfsDeviceError * e) {
		logMessage( LOG_ERROR_LEVEL, "BFS device utest threw device exception [%s], aborting", e->getMessage().c_str() );
		delete e;
		exit( -1 );
	}

	// Return successfully
	return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsDeviceLayerUnitTest
// Description  : The function implementing the BFS device unit test.
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int bfsDeviceLayerUnitTest( void ) {

    // Local variables
    bfs_block_list_t blist;
    bfs_device_list_t devList;
    bfs_device_list_t::iterator it, it2;
    bfsDevice *device;
    bfs_device_id_t tdev;
    bfs_block_id_t tblk, blkid;
    int slot, devat, idx, start;
    char utstr[129];
    size_t blocks, i;
    vector<int> slots;  
    vector<int>::iterator sit;

    // Setup a place to hold the unit test data
    typedef struct {
        bfs_device_id_t dev;           // The device to use
        bfs_block_id_t  blk;           // The block number used
        char            block[BLK_SZ]; // The block of data to hold
    } unit_test_blocks_t;

    typedef struct {
        bfs_device_id_t dev;  // Device ID
        uint64_t        blks; // Number of blocks
        uint16_t       *map;  // Map of slot use, only works BFS_DEV_UNIT_TEST_SLOTS < 2^16
    } unit_test_block_map_t;

    // These are the major structures holding unit test state
    unit_test_block_map_t *umap = NULL;
    unit_test_blocks_t utblks[BFS_DEV_UNIT_TEST_SLOTS];

    // Call the layer manifest
    logMessage( LOG_INFO_LEVEL, "Starting bfs device unit test ..." );
    if ( bfsDeviceLayer::getDeviceManifest(devList) ) {
        logMessage( LOG_ERROR_LEVEL, "Unable to get device manifest data, aborting" );
        return( -1 );
    }

    // Walk the device list printing out detail on the device geometry, saving IDs
    umap = (unit_test_block_map_t *)malloc( sizeof(unit_test_block_map_t) * devList.size() );
    idx = 0;
    for ( it=devList.begin(); it!=devList.end(); it++ ) {

        // Map the particulars
        logMessage( LOG_INFO_LEVEL, "Device found: did=%lu, blocks=%lu", \
            it->second->getDeviceIdenfier(), it->second->getNumBlocks() );

        // Setup the block use map
        umap[idx].dev = it->second->getDeviceIdenfier();
        umap[idx].blks = it->second->getNumBlocks();
        umap[idx].map = (uint16_t *)malloc( umap[idx].blks *sizeof(uint16_t) );
        for ( i=0; i<umap[idx].blks; i++ ) {
            umap[idx].map[i] = BFS_UTEST_UNUSED;
        }
        idx ++;

    }

    // Clear unit test data, cycle through a number of iterations
    memset( utblks, 0x0, sizeof(unit_test_blocks_t)*BFS_DEV_UNIT_TEST_SLOTS );
    for ( i=0; i<BFS_DEV_UNIT_TEST_ITERATIONS; i++ ) {

        // Cleanup, set number of blocks to attempt
        blist.clear();
        slots.clear();
        blocks = get_random_value(1, 10);

        // Put or get blocks
        if ( get_random_value(0, 1) ) {

            // 
            // *** Put blocks ***

            // Pick a set of unique slots to PUT
            while ( slots.size() < blocks ) {
                slot = get_random_value( 0, BFS_DEV_UNIT_TEST_SLOTS-1 );
                if ( std::find(slots.begin(), slots.end(), slot) == slots.end() ) {
                    slots.push_back( slot );
                }
            }

            // Pick a device, block at random, fill with random data
            devat = get_random_value( 0, (uint32_t)devList.size()-1 );
            device = devList[umap[devat].dev];
            tdev = device->getDeviceIdenfier();

            // Create a bunch of blocks (on that device)
            for( sit=slots.begin(); sit!=slots.end(); sit++ ) {
              
                // Now look at the map to see if the block is being used by another slot
                tblk = get_random_value( 0, (uint32_t)device->getNumBlocks()-1 );
                if ( umap[devat].map[tblk] != BFS_UTEST_UNUSED ) {
                    utblks[umap[devat].map[tblk]].dev = 0; // Forget previous put
                }

                // Now create new randomized block
                utblks[*sit].dev = tdev;
                utblks[*sit].blk = tblk;
                get_random_data( utblks[*sit].block, BLK_SZ );
                umap[devat].map[tblk] = (uint16_t)*sit;

                // Add to list of blocks to push
				blist[utblks[*sit].blk] =
					new PBfsBlock(utblks[*sit].block, BLK_SZ, 0, 0, utblks[*sit].blk, device);
			}

            // Check whether we are getting one or more blocks
            if ( blist.size() == 1 ) {
                // Now put the block into device
                slot = slots[0];
                if ( device->putBlock(*blist[utblks[slot].blk]) ) {
                    logMessage( LOG_ERROR_LEVEL, "Failed putting block [%lu] into device [%lu], error (aborting).", 
                        utblks[slot].blk, utblks[slot].dev );
                    return( -1 );
                }
                logMessage( LOG_INFO_LEVEL, "Successful put block [%lu] on device [%lu]", utblks[slot].blk, utblks[slot].dev );
            } else {
                // Now put the blocks into device
                if ( device->putBlocks(blist) ) {
                    logMessage( LOG_ERROR_LEVEL, "Failed putting blocks [%lu] into device [%lu], error (aborting).", 
                        utblks[slot].blk, utblks[slot].dev );
                    return( -1 );
                }
                logMessage( LOG_INFO_LEVEL, "Successful put [%u] blocks on device [%lu]", blocks, utblks[slot].dev );
            }

        } else {

            // 
            // *** Get blocks ***

            // Pick a set of unique slots to PUT
            devat = get_random_value( 0, (uint32_t)devList.size()-1 );
            device = devList[umap[devat].dev];
            start = slot = get_random_value( 0, BFS_DEV_UNIT_TEST_SLOTS-1 );
            while ( slots.size() < blocks ) {
                if ( device->getDeviceIdenfier() == utblks[slot].dev ) {
                    slots.push_back( slot );
                }

                slot = (slot == BFS_DEV_UNIT_TEST_SLOTS-1) ? 0 : slot+1;
                if ( slot == start ) {
                    blocks = slots.size();
                }
            }

            // Now if there are enough slots to do something
            if ( blocks > 0 ) {

				// Set up the block list (and location to stick data)
				for (i = 0; i < slots.size(); i++) {
					blkid = utblks[slots.at(i)].blk;
					blist[blkid] = new PBfsBlock(NULL, BLK_SZ, 0, 0, blkid, device);
				}

				if ( slots.size() == 1 ) {
                    // Get the block from the device
                    slot = slots[0];
                    if ( device->getBlock(*blist[utblks[slot].blk]) ) {
                        logMessage( LOG_ERROR_LEVEL, "Failed getting block [%lu] from device [%lu]", 
                            utblks[slot].blk, utblks[slot].dev );
                        return( -1 );
                    }
                } else {
                    // Get the block from the device
                    if ( device->getBlocks(blist)) {
                        logMessage( LOG_ERROR_LEVEL, "Failed getting [%u] blocks from device [%lu]", blocks, utblks[slot].dev );
                        return( -1 );
                    }
                }

                // Validate the blocks we submitted were the same as returned
                for ( i=0; i<slots.size(); i++ ) {
                    slot = slots[i];
					if (memcmp( utblks[slots[i]].block, blist[utblks[slots[i]].blk]->
                        getBuffer(), BLK_SZ) != 0) {
						logMessage( LOG_ERROR_LEVEL, "Retrieved block [%lu] (from device [%lu]) failed match validation.",
                            utblks[slot].blk, utblks[slot].dev );
                        bufToString( utblks[slot].block, BLK_SZ, utstr, 128 );
                        logMessage( LOG_ERROR_LEVEL, "Failed stored  : [%s]", utstr );
                        bufToString( blist[utblks[slots[i]].blk]->getBuffer(), BLK_SZ, utstr, 128 );
                        logMessage( LOG_ERROR_LEVEL, "Failed recevied: [%s]", utstr );
                        return( -1);
					}
				}

                // Log the unit test thing
                logMessage( LOG_INFO_LEVEL, "Successful get and validate block [%lu] on device [%lu]",
                        utblks[slot].blk, utblks[slot].dev );

            }
        }
    }

    // When we have a shutdown method, we will add it here
    // TODO: add layer shutdowm method

    // Log saluation, return succesfully
    logMessage( LOG_INFO_LEVEL, "Completed bfs device unit test successfull, exiting." );
    return( 0 );
}