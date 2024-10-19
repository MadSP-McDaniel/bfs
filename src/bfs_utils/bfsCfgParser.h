#ifndef bfsCfgParser_INCLUDED
#define bfsCfgParser_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCfgParser.h
//  Description   : This file contains the class definition for the language
//                  parser for simple context free grammars.
//
//   Author       : Patrick McDaniel
//   Created      : Wed Dec 30 09:29:54 EST 2020
//

// Include files

// Project incluides
#include <bfsCfgParserSymbol.h>
#include <bfsCfgParserProduction.h>

// C++/STL Isms
#include <string>
#include <tuple>
#include <vector>
using namespace std;

// Definitions
#define MAX_bfsCfgParser_RECURSION_DEPTH 100

// Types
typedef tuple<string, bfsCfgParserSymbol *> matchedSymbol_t; // A symbol
typedef vector<matchedSymbol_t> tokStream_t; // A tokenized stream

// Parse tree structure
typedef struct pTree_t {
	bfsCfgParserSymbol	         *symbol;   // The matched symbol
	vector<struct pTree_t *>  ntmatch;  // If non-terminal
	matchedSymbol_t           matched;  // If terminal 
} parseTree_t;

//
// Parser Class Definition

class bfsCfgParser {

public:

	//
	// Constructors and destructors

	bfsCfgParser( void );
	  // Attribute constructor

	virtual ~bfsCfgParser();
	  // Destructor

	//
	// Access Methods

	// Get the symbol definition by name	
	bfsCfgParserSymbol * getSymbolDef( string snm ) {
		cfgSymbols_t::iterator it;
	 	for ( it=symbolTable.begin(); it!=symbolTable.end(); it++ ) {
			 if ( (*it)->getName() == snm ) {
				 return( *it );
			 }
		}
		return( NULL );
	}

	//
	// Class Methods

	void addNonTerminalSymbol( string sym );
	  // Add a new non-terminal symbol to the grammar

	void addTerminalSymbol( string sym, string regexp );
	  // Add a new terminal symbol to the grammar

	void addProduction( string lhs, vector<string> rhs );
	  // Add a production to the list of productions in the 

	parseTree_t * parseData( const string & inp );
	  // Parse the input data

	parseTree_t * parseDataFile( const string & filename );
	  // Parse and input file with the assigned grammar

	void freeParseTree( parseTree_t * tree );
	  // Do the recursive freeing of a parse tree

	string grammarToString( void );
	  // Make the whole grammar into a single string.

	string parseTreeToString( parseTree_t * tree, int dep  = 0 );
	  // do a recursive constructon of PT string

private:

	//
	// Private Methods

	void tokenizeData( string rawdata, tokStream_t & toked );
	  // Process the raw data into tokenized stream

	int executeParser( bfsCfgParserSymbol *res, tokStream_t & toks, int idx, int dep, parseTree_t * & match );
	  // do the recursive execution of the parser

	// 
	// Class Data

	cfgSymbols_t symbolTable;
	  // The table of symbols for the grammar

	bfsCfgParserSymbol *startSymbol;
	  // This is the start symbol for the grammar (non-terminal)

	bfsCfgParserSymbol *endSymbol; 
	  // This is the END of input symbol (terminal)

	cfgProductions_t productions;
	  // The table of productions for the grammar

	//
	// Static class data

};

#endif
