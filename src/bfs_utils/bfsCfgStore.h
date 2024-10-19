#ifndef BFS_CFG_STORE_INCLUDED
#define BFS_CFG_STORE_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCfgStore.h
//  Description   : This file contains the class definition for configuration
//                  system and related objects.
//
//   Author       : Patrick McDaniel
//   Created      : Fri 09 Apr 2021 06:28:11 PM PDT
//

// Include files

// Project incluides
#include <bfsCfgParser.h> /* For parseTree_t */
#include <bfsCfgError.h>
#include <bfsCfgItem.h>

// C++/STL Isms

// Definitions

// Types

// Parser Class Definition

class bfsCfgStore {

public:

	//
	// Constructors and destructors

	bfsCfgStore( void );
      // Base constructor

	virtual ~bfsCfgStore();
	  // Destructor

	//
	// Access Methods

	//
	// Class Methods

    bool loadConfigurationFile( string fl );
      // Load a configuration from a file

	// Query the configuration with this tag
	bfsCfgItem * queryConfig( string cfgtag ) {
	    return( configs->queryConfig(cfgtag) );
	}

private:

    //
    // Private class methods

    bool createParser( void );
      // Create the parser and load the grammar
 
	bfsCfgItem * createConfiguration( parseTree_t * tree, bfsCfgItem *context = NULL, string prefix = "" );
	  //  do a recursive processing to get configuration data

	// 
	// Class Data

    void *parser;
      // The parser for the configuration store.

	bfsCfgItem *configs;
	  // This is the recursively defined object with all configurations

	//
	// Static class data

};

#endif
