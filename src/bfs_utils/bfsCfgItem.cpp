////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCfgItem.cpp
//  Description   : This file contains the class definition for configuration
//                  item as parsed from the configuration file.
//
//   Author       : Patrick McDaniel
//   Created      : Tue 13 Apr 2021 08:27:25 AM EDT
//

// Includes

// Project Includes
#include <bfsCfgStore.h>
#include <bfsConfigLayer.h>

//
// Class Data

// Descriptor strings for item types
const char * bfsCfgItem::cfg_item_type_strings[bfsCfgItem_MAXTYPE] = {
    "bfsCfgItem_VALUE", "bfsCfgItem_LIST", "bfsCfgItem_STRUCT"
};

//
// Class Methods


////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgItem::bfsCfgItem
// Description  : Attribute constructor
//
// Inputs       : t - type of configuration item
//                name - name of configuration item
//                val - the value of the configuration item, as needed
// Outputs      : none

bfsCfgItem::bfsCfgItem( bfsCfgItemType_t t, string name, string val ) :
    itype( t ),
    configName( name ),
    value( val ) {

    logMessage( CONFIG_VRBLOG_LEVEL, "Creating config item %s, type %s, value [%s]\n", 
        name.c_str(), getItemTypeString(t), val.c_str() );

    // Sanity check the values/type
    if ( (t != bfsCfgItem_VALUE) && (val.length() > 0) ) {
        throw new bfsCfgError( "Setting value in compount configuation :" + name );
    }
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgItem::~bfsCfgItem
// Description  : Destructor
//
// Inputs       : none
// Outputs      : none

bfsCfgItem::~bfsCfgItem() {
    // TODO: cleanup the configurations
}

//
// Class methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgItem::bfsCfgItemValueLong
// Description  : Get the item value (int)
//
// Inputs       : none
// Outputs      : the value (uint64_t)

int64_t bfsCfgItem::bfsCfgItemValueLong( void ) {

    // Local variables
    uint64_t ld;
    string message;
    char *endptr = NULL;

    // Convert and check
    ld = strtol( value.c_str(), &endptr, 10 );
    if ( endptr == value.c_str() ) {
        message = "Getting non-integer value as long :" + configName;
        throw new bfsCfgError( message );
    }
    return( ld );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgItem::bfsCfgItemValueUnsigned
// Description  : Get the item value (int)
//
// Inputs       : none
// Outputs      : the value (uint64_t)

uint64_t bfsCfgItem::bfsCfgItemValueUnsigned( void ) {

    // Local variables
    uint64_t ld;
    string message;
    char *endptr = NULL;

    // Convert and check
    ld = strtoul( value.c_str(), &endptr, 10 );
    if ( endptr == value.c_str() ) {
        message = "Getting non-integer value as unsigned :" + configName;
        throw new bfsCfgError( message );
    }
    return( ld );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgItem::bfsCfgItemValueFloat
// Description  : Get the item value (floating point)
//
// Inputs       : none
// Outputs      : the value (double floating point)

double bfsCfgItem::bfsCfgItemValueFloat( void ) {

    // Local variables
    double ld;
    string message;
    char *endptr = NULL;

    // Convert and check
    ld = strtod( value.c_str(), &endptr );
    if ( endptr == value.c_str() ) {
        message = "Getting non-float value as long :" + configName;
        throw new bfsCfgError( message );
    }
    return( ld );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgItem::addSubItem
// Description  : Add a sub-item to this compound configuration
//
// Inputs       : itm - the configuration item to save
// Outputs      : none

void bfsCfgItem::addSubItem( bfsCfgItem *itm ) {

    // Sanity check the incoming configuration item
    string message;
    if (  itm == NULL ) {
        message = "Adding NULL subitem to config : " + configName;
        throw new bfsCfgError( message );
    }
    if ( itype == bfsCfgItem_VALUE ) {
        message = "Adding sub-item to value config " + configName + ", Adding : " + itm->configName;
        throw new bfsCfgError( message );
    }

    // Just add to the back of the item list
    subItems.push_back( itm );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgItem::getSubItemByName
// Description  : Get a subitem from this configuration
//
// Inputs       : cfgnm - the name of the configuration to find
// Outputs      : the item or throw news exception if fail

bfsCfgItem * bfsCfgItem::getSubItemByName( string cfgnm ) {

    // Local variables
    bfsCfgItemList_t::iterator it;
    string message;

    // Search the list of items in the subitems
    for ( it=subItems.begin(); it!=subItems.end(); it++ ) {
        if ( (*it)->configName == cfgnm ) {
            return( *it );
        }
    }

    // Not found just throw new exception
    message = "Subvalue name not found getSubItem : " + cfgnm + " in " + configName;
    throw new bfsCfgError( message );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgItem::getSubItemByIndex
// Description  : Get a subitem from this configuration
//
// Inputs       : idx - the index to find 
// Outputs      : the item or throw news exception if fail

bfsCfgItem * bfsCfgItem::getSubItemByIndex( int idx ) {

    // Sanity check
    string message;
    if ( (idx < 0) || (idx >= (int)subItems.size()) ) {
        message = "Index out of range in getSubItem : " + to_string(idx) + " in " + configName;
        throw new bfsCfgError( message );
    }

    // return the item by indexing the vector
    return( subItems.at(idx) );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgItem::queryConfig
// Description  : Query the configutation for a specific item
//
// Inputs       : cfgtag - the configuration tag to query
// Outputs      : the item or NULL if not found

bfsCfgItem * bfsCfgItem::queryConfig( string cfgtag ) {

    // Local variables
    size_t pos;
    string search, idxstr, message;
    bfsCfgItem *subitem, *indexitem;
    int idx;

    // Check for compound config    
    if ( (pos = cfgtag.find_first_of(".[")) != string::npos ) {
    
        // Ok find the next occurence of the struct element
        if ( cfgtag.at(pos) == '.' ) {

            // Find the subitem
            search = cfgtag.substr( 0, pos );
            if ( (subitem = getSubItemByName(search)) == NULL ) {
                return( NULL );
            }

            // Now see if we need to contine
            cfgtag = cfgtag.substr( pos+1 );
            if ( cfgtag.length() > 0 ) {
                return( subitem->queryConfig(cfgtag) );
            } else {
                return( subitem );
            }

        } else 

        // Ok find the next occurence of the list element
        if ( cfgtag.at(pos) == '[' ) {

            // Get the subitem (list), then index item
            search = cfgtag.substr( 0, pos );

            idx = stoi(cfgtag.substr(pos+1));
        
            if ( (subitem = getSubItemByName(search)) == NULL ) {
                return( NULL );
            }
            if ( (indexitem = subitem->getSubItemByIndex(idx)) == NULL ) {
                return( NULL );
            }

            // Now see if we need to contine
            pos = cfgtag.find( "]" );
            cfgtag = cfgtag.substr( pos+1 );
            if ( cfgtag.length() > 0 ) {

                // Check the case where you are accessing a config as index
                if ( cfgtag.at(0) == '.' ) {

                    // Just skip the alias name in the configuration
                    cfgtag = cfgtag.substr(1);
                    if ( cfgtag.find(indexitem->bfsCfgItemName()) != 0 ) {
                        throw new bfsCfgError( "Name mismatch in array index" );
                    }
                    cfgtag = cfgtag.substr( indexitem->bfsCfgItemName().length() );
                    if ( cfgtag.length() == 0 ) {
                        return( indexitem );
                    } else if ( cfgtag.at(0) == '.' ) {
                        cfgtag = cfgtag.substr(1);
                    }

                // Not supporting nested arrays 
                } else if ( cfgtag.at(0) == '[' ) {
                    throw new bfsCfgError( "Next arrays not currently supported." );
                }

                // Call the recursive query
                return( indexitem->queryConfig(cfgtag) );
            } else {
                return( indexitem );
            }

        } 
    }

    // Return not found
    return( getSubItemByName(cfgtag) );
}   

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgItem::toString
// Description  : To string (create a string with the configuration)
//
// Inputs       : indent - indent level, -1 mean no indent
// Outputs      : the string

string bfsCfgItem::toString( int indent ) {

    // Local variables
    string padding, str;
    bfsCfgItemList_t::iterator it;

    // Setup the indent (only if used)
    if ( indent != -1 ) {
        padding = string( indent*2, ' ' );
    }

    // Now process the kind of item it is
    switch ( itype ) {
        case bfsCfgItem_VALUE: // A normal value (number, word)
            str = padding + configName + " : " + value + "\n";
            break;
    
        case bfsCfgItem_LIST:  // A list of items
            str = padding + configName + " [ \n";
            for ( it=subItems.begin(); it!=subItems.end(); it++ ) {
                str += (*it)->toString( indent+1 );
            }
            str += padding + "]\n";
            break;

        case bfsCfgItem_STRUCT: // A structure of sub-configurations
            str = padding + configName + " {\n";
            for ( it=subItems.begin(); it!=subItems.end(); it++ ) {
                str += (*it)->toString( indent+1 );
            }
            str += padding + "}\n";
            break;

        default:
            str = "BAD CONFIG ITEM";
            break;
    }

    // Return the created string
    return( str );
}

