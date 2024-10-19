#ifndef CFGSYMBOL_INCLUDED
#define CFGSYMBOL_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCfgParserSymbol.h
//  Description   : This file contains the class definition context free grammar
//                  symbol class.  The object contains all of the information
//                  needed to define a symbol (termonal or non-terminal) in the
//                  grammar.
//
//   Author       : Patrick McDaniel
//   Created      : Wed Dec 30 09:29:54 EST 2020
//

// Include files

// Project incluides
#include <bfsRegExpression.h>

// C++/STL Isms
#include <string>
#include <vector>
using namespace std;

// Definitions

typedef enum {
	CFG_TERMINAL    = 0,  // A terminal symbol
	CFG_NONTERMINAL = 1,  // A non-terminal symbol
	CFG_UNKNOWN     = 2   // A guard value
} bfsCfgParserSymbolType_t;

class bfsCfgParserSymbol;
typedef vector<bfsCfgParserSymbol *> cfgSymbols_t;  // Set of symbol defintions

//
// Class Definition
class bfsCfgParserSymbol {

public:

	//
	// Constructors and destructors

    // Non-terminal attribute constructor
	bfsCfgParserSymbol( string sym ) :
        symType( CFG_NONTERMINAL ),
        symName( sym ),
        symbolMatcher(),
        noMatch( true ) {

    }

    // Terminal attribute constructor
	bfsCfgParserSymbol( string sym, string re );

    // Special symbol constructor
	bfsCfgParserSymbol( string sym, bool nomatch ) :
        symType( CFG_TERMINAL ),
        symName( sym ),
        symbolMatcher(),
        noMatch( nomatch ) {

    }

    // Destructor
	virtual ~bfsCfgParserSymbol() {
    }

	//
	// Access Methods

    // Get the symbol type for the object
    bfsCfgParserSymbolType_t getSymbolType( void ) {
        return( symType );
    }

    // Get the symbol name
    string getName( void ) {
        return( symName );
    }

    int isSymbol( string );
        // See if a string is of this type

	//
	// Class Methods

private:

	//
	// Private Methods

    // Default constructor (should never be called)
	bfsCfgParserSymbol( void ) :
        symType( CFG_UNKNOWN ),
        symbolMatcher( NULL ) {
    }

	// 
	// Class Data

    bfsCfgParserSymbolType_t symType;
      // The type of symbol (terminal/non-terminal)

    string symName;
      // This is the symbol name (used in productions)

    bfsRegExpression symbolMatcher;
      // A regular expression to match the symbol (terminal only)

    bool noMatch;
      // Flag indicating that this terminal symbol should not match ever

	//
	// Static class data

};

#endif


