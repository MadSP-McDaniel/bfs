#ifndef BFS_CONFIG_LAYER_INCLUDED
#define BFS_CONFIG_LAYER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsConfigLayer.h
//  Description   : This is the class describing config layer interface
//                  for the bfs file system.  Note that this is static class
//                  in which there are no objects.
//
//  Author  : Patrick McDaniel
//  Created : Thu 15 Apr 2021 03:56:12 PM EDT
//

// STL-isms

// Project Includes
#include <bfs_log.h>
#include <bfsCfgStore.h>

//
// Class definitions
#define BFS_BASEDIR_ENVVAR "BFS_HOME"
#define BFS_DEFAULT_SYSCONFIG "/config/bfs_system_config.cfg"
#define CONFIG_LOG_LEVEL bfsConfigLayer::getConfigLayerLogLevel()
#define CONFIG_VRBLOG_LEVEL bfsConfigLayer::getVerboseConfigLayerLogLevel()
#define BFS_CFGLYR_CONFIG "bfsConfigLayer"
//
// Class Definition

class bfsConfigLayer {

public:

	//
	// Static methods

	static int loadSystemConfiguration( void );
	  // Load a config configuration

    static bfsCfgItem * getConfigItem( string cfgtag );
      // Get a configuration item from the config tag

    static int getConfigItemValue( string cfgtag, string & val );
      // Get a configuration value from the config tag (returns 0 if found),
      // and places the value in the "val" ref

	static int bfsConfigLayerInit( void );
	  // Initialize the config layer state 

	//
	// Static Class Methods

	static bool systemConfigLoaded( void ) {
		return( (bfsConfigLayerInitialized) && (systemConfig != NULL) );
	}

	// Layer log level
	static unsigned long getConfigLayerLogLevel( void ) {
		return( bfsConfigLogLevel );
	}

	// Verbose log level
	static unsigned long getVerboseConfigLayerLogLevel( void ) {
		return( bfsVerboseConfigLogLevel );
	}

	// Get the system base directory
	static string getSystemBaseDirectory( void ) {
		return( systemBaseDir );
	}

	static int bfsConfigLayerUtest( void );
	  // Test the implementation of the configuration engine


private:

	//
    // Private class methods

	bfsConfigLayer( void ) {}
	  // Default constructor (prevents creation of any instance)

	//
	// Static Class Variables

	static unsigned long bfsConfigLogLevel;
	  // The log level for all of the config information

	static unsigned long bfsVerboseConfigLogLevel;
	  // The log level for all of the config information

	static bfsCfgStore * systemConfig;
      // This is the system wide configuration object.

	static string systemBaseDir;
	  // The base directory of the file system configuration. (env BFS_HOME)

	static bool bfsConfigLayerInitialized;
      // The flag indicating the configuration layer is initialized

};

#endif
