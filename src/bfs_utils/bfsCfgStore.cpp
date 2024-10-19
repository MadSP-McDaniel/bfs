////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCfgStore.cpp
//  Description   : This file contains the class implementation for the
//                  configuration system and related objects.
//  Author		  : Patrick McDaniel
//  Created	      : Fri 09 Apr 2021 06:42:23 PM PDT
//

// Includes

// Project Includes
#include <bfsCfgStore.h>
#include <bfsConfigLayer.h>

//
// Class Data

//
// Class Methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgStore::bfsCfgStore
// Description  : Base constructor
//
// Inputs       : none
// Outputs      : none

bfsCfgStore::bfsCfgStore( void ) :
    parser( NULL ),
    configs( NULL ) {
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgStore::bfsCfgStore
// Description  : Base constructor
//
// Inputs       : none
// Outputs      : none

bfsCfgStore::~bfsCfgStore( void ) {
    // TODO : clean up the storage
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgStore::bfsCfgStore
// Description  : Load a configuration from a file
//
// Inputs       : fl - the file(name) to read and parse
// Outputs      : true if successful, false otherwise

 bool bfsCfgStore::loadConfigurationFile( string fl ) {

    // Local variables
	parseTree_t * ptree;

    // Chek if we need to create the parser
    if ( parser == NULL ) {
        createParser();
    }

	// Parse the data file, print tree
	ptree = ((bfsCfgParser *)parser)->parseDataFile( fl );
    logMessage( CONFIG_VRBLOG_LEVEL, "PTree %s", ((bfsCfgParser *)parser)->parseTreeToString(ptree).c_str() );
    configs = new bfsCfgItem( bfsCfgItem_STRUCT, "START" );
    createConfiguration( ptree, configs );
    logMessage( CONFIG_VRBLOG_LEVEL, "Config:%s", configs->toString().c_str() );

    // Return successfully
    return( true );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgStore::createConfiguration
// Description  : do a recursive processing to get configuration data 
//
// Inputs       : tree - the tree to free
//                context - the configuration context
//                prefix - the prefix name (for long names)
// Outputs      : none

bfsCfgItem * bfsCfgStore::createConfiguration( parseTree_t * tree, bfsCfgItem *context, string prefix ) {

	// Local variables
	vector<struct pTree_t *>::iterator it, it2;
    vector<string> listValues;
	string tag, value, fqtag, values, subtag;
    parseTree_t * el;
    bfsCfgItem *item, *litem;
    bool done;
    int idx;

    //  Check if this is the CONFIG non-terminal
    if ( tree->symbol->getName() == "CONFIG" ) {

        // First get the configuration name
        it = tree->ntmatch.begin();
        if ( (*it)->symbol->getName() != "WORD" ) {
            throw new bfsCfgParserError( "Bad parse tree" );  //TODO: make these more meaningful
        }
        tag = get<0>((*it)->matched);
        fqtag = ((prefix.length() > 0) ? prefix + "." : "") + tag;
        logMessage( CONFIG_VRBLOG_LEVEL, "Tag %s", tag.c_str() );

        // Figure out what kind of configuration this is
        it ++;
        if ( (*it)->symbol->getName() == ":" ) {

            // A simple  configuration
            if ( tree->ntmatch.size() != 3 ) {
                throw new bfsCfgParserError( "Bad parse tree" );
            }
            value = get<0>(tree->ntmatch.at(2)->matched);
            logMessage( CONFIG_VRBLOG_LEVEL, "Value %30s %s", fqtag.c_str(), value.c_str() );
            item = new bfsCfgItem( bfsCfgItem_VALUE, tag, value );
            if ( context != NULL ) {
                context->addSubItem( item );
            }

        } else if ( ((*it)->symbol->getName() == "CFGLIST") && ((*it)->ntmatch.at(1)->symbol->getName() == "WORDLIST") ) {

            // Expect a list of values (walk the parse elements)
            item = new bfsCfgItem( bfsCfgItem_LIST, tag );
            context->addSubItem( item );
            el = (*it)->ntmatch.at(1);
            done = false;
            values = "";
            idx = 0;
            while ( ! done ) {

                // Create the special configuration (list values)
                subtag = tag + "[" + to_string(idx) + "]";
                value = get<0>(el->ntmatch.at(0)->matched);
                litem = new bfsCfgItem( bfsCfgItem_VALUE, subtag, value );
                item->addSubItem( litem );
                idx ++;

                // Just save some logging information
                values += (values.length() > 0) ? " " + value : value;
                listValues.push_back( value );

                // Move to the next item
                if ( el->ntmatch.size() > 1 ) {
                    el = el->ntmatch.at( 1 );
                } else {
                    done = true;
                }
            }
            logMessage( CONFIG_VRBLOG_LEVEL, "Value %30s %s", fqtag.c_str(), values.c_str() );

        } else if ( (*it)->symbol->getName() == "CFGLIST" ) {

            // Process the list as a new context
            item = new bfsCfgItem( bfsCfgItem_LIST, tag );
            logMessage( CONFIG_VRBLOG_LEVEL, "List begin : %s", tag.c_str() );
            context->addSubItem( item );
            createConfiguration( *it, item, prefix );
            logMessage( CONFIG_VRBLOG_LEVEL, "List end : %s", tag.c_str() );

        } else if ( (*it)->symbol->getName() == "CFGSTRUCT" ) {

            // Process the struct as a new context
            item = new bfsCfgItem( bfsCfgItem_STRUCT, tag );
            logMessage( CONFIG_VRBLOG_LEVEL, "Struct begin : %s", tag.c_str() );
            context->addSubItem( item );
            createConfiguration( *it, item, prefix );
            logMessage( CONFIG_VRBLOG_LEVEL, "Struct end : %s", tag.c_str() );

        } else {

            // throw new an error, crazy parse information
            throw new bfsCfgParserError( "Bad parse tree, weird config" + (*it)->symbol->getName() );
        }

    } else {

        // Walk the elements looking for configs
        for ( it=tree->ntmatch.begin(); it!=tree->ntmatch.end(); it++ ) {
            createConfiguration( *it, context, prefix );
        }
    }

    return( item );
}

//
// Static class methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgStore::createParser
// Description  : Create the parser and load the grammar
//
// Inputs       : none
// Outputs      : none

bool bfsCfgStore::createParser( void ) {
    bfsCfgParser *_parser;

    // Check if the parser exists, bail if so
    if ( parser != NULL ) {
        return( false );
    }

    // Create and setup the configuration parser
	try {

        // Create the parser 
    	parser = new bfsCfgParser();
        _parser = (bfsCfgParser *)parser;

		// Now add the terminal symbols (with regexp matchers)
		_parser->addTerminalSymbol( ":", ":" );
		_parser->addTerminalSymbol( "[", "\\[" );
		_parser->addTerminalSymbol( "]", "\\]" );
		_parser->addTerminalSymbol( "{", "\\{" );
		_parser->addTerminalSymbol( "}", "\\}" );
		_parser->addTerminalSymbol( "WORD", "[\\w\\.\\+\\/=]+" );

		// Add the non-terminal symbols
		_parser->addNonTerminalSymbol( "CONFIGS" );
		_parser->addNonTerminalSymbol( "CONFIG" );
		_parser->addNonTerminalSymbol( "CFGLIST" );
		_parser->addNonTerminalSymbol( "CFGSTRUCT" );
		_parser->addNonTerminalSymbol( "WORDLIST" );

		// Add the productions
		_parser->addProduction( "START", {"CONFIGS", "END"} );
		_parser->addProduction( "CONFIGS", {"CONFIG", "CONFIGS"} );
		_parser->addProduction( "CONFIGS", {"CONFIG"} );
		_parser->addProduction( "CONFIG", {"WORD", "CFGLIST"} );
		_parser->addProduction( "CONFIG", {"WORD", "CFGSTRUCT"} );
		_parser->addProduction( "CONFIG", {"WORD", ":", "WORD"} );
		_parser->addProduction( "CFGLIST", {"[", "WORDLIST", "]"} );
		_parser->addProduction( "CFGLIST", {"[", "CONFIGS", "]"} );
		_parser->addProduction( "CFGSTRUCT", {"{", "CONFIGS", "}"} );
		_parser->addProduction( "WORDLIST", {"WORD", "WORDLIST"} );
		_parser->addProduction( "WORDLIST", {"WORD"} );

		// Log the grammar
		logMessage( CONFIG_VRBLOG_LEVEL, "%s", _parser->grammarToString().c_str() );


	} catch (bfsCfgParserError * e) {

		// Catch and process error
		logMessage( CONFIG_VRBLOG_LEVEL, "Error : %s", e->getMessage().c_str() );
		delete (bfsCfgParser *)parser;
		return( -1 );
	}

    // Return successfully
    return( true );
}