 /**
 * 
 * @file   bfsLocalDevice.cpp
 * @brief  This is the class implementation for the storage device interface for
           the bfs file system (non-device side).  This is the server side, where 
           eachobject represents a device on the network/in a process.
 * 
 */

/* Include files  */
#include <string.h>

/* Project include files */
#include <bfsLocalDevice.h>
#include <bfsDeviceLayer.h>
#include <bfs_log.h>

/* Macros */

/* Globals  */


//
// Class Data

//
// Class Functions


/**
 * @brief The attribute constructor for the class 
 * 
 * @param path - the path to the storage file
 * @param blks - the number of blocks in the device
 * @return int : 0 is success, -1 is failure 
 */

bfsLocalDevice::bfsLocalDevice( bfs_device_id_t did, string path, uint64_t blks )
  : deviceID( did ),
    storagePath( path ),
    numBlocks( blks ),
    secContext( NULL ),
    storage( NULL ) {

    // Return, no return code
    return;
}

/**
 * @brief The destructor function for the class
 * 
 * @param none
 */

bfsLocalDevice::~bfsLocalDevice( void )  {


    delete secContext;

    // Return, no return code
    return;
}

/**
 * @brief Initialize the device
 * 
 * @param none
 * @return int : 0 is success, -1 is failure 
 */

int bfsLocalDevice::bfsDeviceInitialize( void ) {

    // Create the storage device, log, return successfull
    storage = new bfsDeviceStorage( deviceID, numBlocks );
    return( 0 );
}


/**
 * @brief De-initialze the device 
 * 
 * @param none
 * @return int : 0 is success, -1 is failure 
 */

int bfsLocalDevice::bfsDeviceUninitialize( void ) {

    // Clean up the device objects
    if ( storage != NULL ) {
        delete storage;
        storage = NULL;
    }

    // Return successfully
    logMessage( DEVICE_LOG_LEVEL, "Local device disconnected (%lu).", deviceID );
    return( 0 );
}

/**
 * @brief Get a block from the device
 * 
 * @param blks - the list of blocks to get
 * @return int : 0 is success, -1 is failure 
 */

int bfsLocalDevice::getBlocks( bfs_block_list_t & blks ) {

    // Local variables
    bfs_block_list_t::iterator it;
    char bbuf[128];
    string msg;

    // Walk the blocks and get them
    for ( it=blks.begin(); it!=blks.end(); it++ ) {
        if ( storage->getBlock( it->first, it->second->getBuffer() ) == NULL ) {
            throw new bfsDeviceError( "Failed putting block in local device" );
        }
    }

    // Log, possibly list blocks
    if ( levelEnabled(DEVICE_LOG_LEVEL) ) {
        msg = "";
        for ( it=blks.begin(); it!=blks.end(); it++ ) {
            bufToString(it->second->getBuffer(), 2, bbuf, 128);
            msg += " : " + to_string(it->first) + " (" + bbuf + ")";
        }
    }
    logMessage( DEVICE_LOG_LEVEL, "Get blocks sent to device %lu, %u blocks%s", deviceID, blks.size(), msg.c_str() );

    // Return successfully
    return( 0 );
}

/**
 * @brief Get the blocks associated with the IDS
 * 
 * @param blkid - the block ID for the block to put (at block id)
 * @param buf - buffer to copy contents into (NULL no copy)
 * @return int : 0 is success, -1 is failure 
 */

int bfsLocalDevice::putBlocks( bfs_block_list_t & blks ) {

    // Local variables
    bfs_block_list_t::iterator it;
    char bbuf[128];
    string msg;

    // Walk the blocks and get them
    for ( it=blks.begin(); it!=blks.end(); it++ ) {
        if ( storage->putBlock( it->first, it->second->getBuffer() ) == NULL ) {
            throw new bfsDeviceError( "Failed putting block in local device" );
        }
    }

    // Log, possibly list blocks
    if ( levelEnabled(DEVICE_LOG_LEVEL) ) {
        msg = "";
        for ( it=blks.begin(); it!=blks.end(); it++ ) {
            bufToString(it->second->getBuffer(), 2, bbuf, 128);
            msg += " : " + to_string(it->first) + " (" + bbuf + ")";
        }
    }
    logMessage( DEVICE_LOG_LEVEL, "Put blocks sent to device %lu, %u blocks%s", deviceID, blks.size(), msg.c_str() );

    // Return successfully
    return( 0 );
}
