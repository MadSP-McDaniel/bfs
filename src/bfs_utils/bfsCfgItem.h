#ifndef BFS_CFG_ITEM_INCLUDED
#define BFS_CFG_ITEM_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCfgItem.h
//  Description   : This file contains the class definition for configuration
//                  item as parsed from the configuration file.
//
//   Author       : Patrick McDaniel
//   Created      : Fri 09 Apr 2021 08:54:24 PM PDT
//

// Include files

// Project incluides
#ifdef __BFS_ENCLAVE_MODE
#include "bfs_enclave_t.h" /* For ocalls */
#endif

// #include <bfsCfgStore.h>
#include <bfsCfgError.h>
 
// C++/STL Isms
#include <vector>
#include <string>

// Definitions

// Types

typedef enum {
    bfsCfgItem_VALUE   = 0,  // A normal value (number, word)
    bfsCfgItem_LIST    = 1,  // A list of items
    bfsCfgItem_STRUCT  = 2,  // A structure of sub-configurations
    bfsCfgItem_MAXTYPE = 3   // A guard value
} bfsCfgItemType_t;

class bfsCfgItem;
typedef vector<bfsCfgItem *> bfsCfgItemList_t;

// Parser Class Definition

class bfsCfgItem {

public:

	//
	// Constructors and destructors

  bfsCfgItem( bfsCfgItemType_t t, string name, string val = "" );
      // The attribute constructor

	virtual ~bfsCfgItem( void );
      // Destructor

	//
	// Access Methods

    // Get the item type 
    bfsCfgItemType_t bfsCfgItemType( void ) {
        return( itype );
    }

    // Get the item name
    const string & bfsCfgItemName( void ) {
        return( configName );
    }

    // Get the item value
    const string & bfsCfgItemValue( void ) {
        return( value );
    }

    int64_t bfsCfgItemValueLong( void );
      // Get the item value (int)

    uint64_t bfsCfgItemValueUnsigned( void );
      // Get the item value (unsigned)

    double bfsCfgItemValueFloat( void );
      // Get the item value (floating point)

    // Get the number of subitems
    int bfsCfgItemNumSubItems( void ) {
        return( (int)subItems.size() );
    }

	//
	// Class Methods

    void addSubItem( bfsCfgItem *itm );
      // Add a sub-item to this compound configuration.

    bfsCfgItem * getSubItemByName( string cfgnm );
      // Get a subitem from this configuration

    bfsCfgItem * getSubItemByIndex( int idx );
      // Get a subitem from this configuration

    bfsCfgItem * queryConfig( string cfgtag );
      // Query the configutation for a specific item

    string toString( int indent = 0 );
      // To string (create a string with the configuration)

    //
    // Static class methods

    // Return a string associated with the configuration type
    static const char * getItemTypeString( bfsCfgItemType_t t ) {
        if( (t >= 0) && (t < bfsCfgItem_MAXTYPE) ) {
            return( cfg_item_type_strings[t] );
        }
        return( "[BAD CONFIG TYPE]");
    }

private:

    //
    // Private interfaces

    // Private default constructor (prevents calling default)
    bfsCfgItem( void ) {
    }

	// 
	// Class Data

    bfsCfgItemType_t itype;
      // The type of item that this 

    string configName;
      // The name of the configuration item (NOT the value)

    string value;
      // The value of this configuration item (value item)

    bfsCfgItemList_t subItems;
      // The list of subitems (compound item)

	//
	// Static class data

    static const char * cfg_item_type_strings[bfsCfgItem_MAXTYPE];
      // Strings describing the configuration types

};

#endif
