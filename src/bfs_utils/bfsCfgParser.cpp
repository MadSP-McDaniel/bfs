////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCfgParser.cpp
//  Description   : This file contains the class definition for the PDM Life
//                  object, which is the standard game from John Conway (1970).
//  Author		  : Patrick McDaniel
//  Created	      : Wed Jan  1 10:56:39 EST 2014
//

// Includes
#include <fstream>
#include <sstream>
#include <string.h>

// Project Includes
#include <bfsCfgParser.h>
#include <bfsCfgParserProduction.h>
#include <bfsConfigLayer.h>
#include <bfsRegExpressionError.h>

//
// Class Data

//
// Class Methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgParser::bfsCfgParser
// Description  : Attribute constructor
//
// Inputs       : apCode - airport code
//                sLog - log to print report to
// Outputs      : none

bfsCfgParser::bfsCfgParser( void ) {

	// Add the START non-terminal and END terminal symbols
	startSymbol = new bfsCfgParserSymbol( "START" );
	symbolTable.push_back( startSymbol );
	endSymbol = new bfsCfgParserSymbol( "END", true );
	symbolTable.push_back( endSymbol );
	
	// Return, no return code
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgParser::~bfsCfgParser
// Description  : Destructor
//
// Inputs       : none
// Outputs      : none

bfsCfgParser::~bfsCfgParser( void ) {

	// TODO CLEAN UP STRUCTURES

	// Return, no return code
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgParser::addNonTerminalSymbol
// Description  : Add a new non-terminal symbol to the grammar
//
// Inputs       : sym - the symbol to add
//                start - flag indicating that this is the START symobl
// Outputs      : none

void bfsCfgParser::addNonTerminalSymbol( string sym ) {

	// Create symbol and add to the symbol table
	bfsCfgParserSymbol * psym = new bfsCfgParserSymbol( sym );
	symbolTable.push_back( psym );

	// Return, no return code
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgParser::addNonTerminalSymbol
// Description  : Add a new terminal symbol to the grammar
//
// Inputs       : sym - the symbol to add
//                regexp - the regular expression to match
// Outputs      : none

void bfsCfgParser::addTerminalSymbol( string sym, string regexp ) {

	// Create symbol and add to the symbol table
	try {
		bfsCfgParserSymbol * psym = new bfsCfgParserSymbol( sym, regexp );
		symbolTable.push_back( psym );
	} catch (bfsRegExpressionError & e) {
		// throw new a fit
		string message = "Parser error adding symbol \"" + sym + "\", bad regxp : " + e.getMessage();
		throw new bfsCfgParserError( message );
	}

	// Return, no return code
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgParser::addNonTerminalSymbol
// Description  : Add a production to the list of productions in the 
//
// Inputs       : sym - the symbol to add
//                regexp - the regular expression to match
// Outputs      : none

void bfsCfgParser::addProduction( string lhs, vector<string> rhs ) {

	// Local variables
	bfsCfgParserSymbol *sym;
	vector<string>::iterator it;
	string message;
	bfsCfgParserProduction *prod;

	// Check to make sure the left hand side is known and defined
	if ( (sym = getSymbolDef(lhs)) == NULL ) {
		message = "Parser error adding production \"" + lhs + "\", not defined.";
		throw new bfsCfgParserError( message );		
	}
	if ( sym->getSymbolType() != CFG_NONTERMINAL ) {
		message = "Parser error left hand side of production not non-termainl \"" + lhs + "\"";
		throw new bfsCfgParserError( message );		
	}

	// Create, add new left hand side of the production
	prod = new bfsCfgParserProduction();
	prod->setLeftHandSide( sym );

	// Now walk the right hand side symbols
 	for ( it=rhs.begin(); it!=rhs.end(); it++ ) {
		// Check to make sure the left hand side is known and defined
		if ( (sym = getSymbolDef(*it)) == NULL ) {
			delete prod;
			message = "Parser error adding production \"" + lhs + "\", not defined.";
			throw new bfsCfgParserError( message );		
		}
		prod->addRightHandSide( sym );
	}

	// Add the production, return (no return code)
	productions.push_back( prod );
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgParser::parseData
// Description  : Parse the input data
//
// Inputs       : input - the input data to parse
// Outputs      : none

parseTree_t * bfsCfgParser::parseData( const string & inp ) {

	// Local variables
	parseTree_t *ptree;
	tokStream_t toked;

	// Tokenize and then parse the data
	tokenizeData( inp, toked );
	executeParser( startSymbol, toked, 0, 0, ptree );

	// Return the parse tree
	return( ptree );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgParser::parseDataFile
// Description  : Parse and input file with the assigned grammar
//
// Inputs       : filename - the file/path to read and parse
// Outputs      : none

parseTree_t * bfsCfgParser::parseDataFile( const string & filename ) {

	// Local variables
	string message;
	ifstream ifile;
    stringstream sstream;
	parseTree_t *ptree;

	// Open the file, read all of the context into a string stream
    ifile.open( filename );
	if ( ifile.fail() ) {
		message = "Parse error: file \"" + filename + "\" open failed, " + strerror(errno);
		throw new bfsCfgParserError( message );
	}

	// Redirect call and return, no return code
    sstream << ifile.rdbuf();
    ptree = parseData( sstream.str() );
	return( ptree );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgParser::freeParseTree
// Description  : do the recursive freeing of a parse tree
//
// Inputs       : tree - the tree to free
// Outputs      : none

void bfsCfgParser::freeParseTree( parseTree_t * tree ) {

	// Local variables
	parseTree_t *bk;

	// Check the kind of element we are in
	if ( tree->symbol->getSymbolType() == CFG_TERMINAL ) {
		return;
	} 

	// Walk the list of parse tree elements, freeing structures
	while ( ! tree->ntmatch.empty() ) {
		bk = tree->ntmatch.back();
		tree->ntmatch.pop_back();
		freeParseTree( bk );
		delete bk;
	}

	// Return, no return value
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgParser::grammarToString
// Description  : Make the whole grammar into a single string.
//
// Inputs       : none
// Outputs      : multiline string containing grammar

string bfsCfgParser::grammarToString( void ) {

    // Provide a string describing the production
	cfgProductions_t::iterator it;
	string str = "Grammar:\n";
	for ( it=productions.begin(); it!=productions.end(); it++ ) {
		str += "  " + (*it)->toString() + "\n";
	}

	// Return the gramar strings
	return( str );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgParser::parseTreeToString
// Description  : do a recursive constructon of PT string
//
// Inputs       : tree - the tree to free
// Outputs      : none

string bfsCfgParser::parseTreeToString( parseTree_t * tree, int dep ) {

	// Local variables
	vector<struct pTree_t *>::iterator it;
	string str;

	// Check the kind of element we are in
	if ( tree->symbol->getSymbolType() == CFG_TERMINAL ) {
		str = string( dep*2, ' ' );
		str += tree->symbol->getName() + " -> " + get<0>(tree->matched) + "\n";
	}  else {

		// Walk the non terminals
		str = str += string( dep*2, ' ' ) + tree->symbol->getName() + "\n" ;
		for ( it=tree->ntmatch.begin(); it!=tree->ntmatch.end(); it++ ) {
			str += parseTreeToString( *it, dep+1 );
		}

	} 

	// Return, no return value
	return( str );
}

//
// Private methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgParser::tokenizeData
// Description  : Process the raw data into tokenized stream
//
// Inputs       : rawdata - the data to split into tokens
//                toked - the place to place the tokenized stream
// Outputs      : number of tokens or -1 if failure

void bfsCfgParser::tokenizeData( string rawdata, tokStream_t & toked ) {

	// Local variables
	int idx;
	char ch;
	string working, message, comment;
	cfgSymbols_t::iterator it;
	bool tokReady;

	// Process all of the characters in the input string
	idx = 0;
	tokReady = false;
	while ( idx<(int)rawdata.length() ) {

		/* the token should end on_exit
			(a) whitespace
			(b) comment
			(c) EOF
		*/

		// Get next character and process it
		ch = rawdata.at(idx);
		if ( isspace(ch) ) {
			tokReady = (working.length() > 0);
		} else if ( ch == '#' ) {
			// This is a comment, skip until end of line
			comment = "";
			while ( (idx<(int)rawdata.length()) && (rawdata.at(idx) != '\n') ) {
				comment += rawdata.at(idx);
				idx++;
			}
			logMessage( CONFIG_VRBLOG_LEVEL, "Skipped comment : %s", comment.c_str() );
			tokReady = (working.length() > 0);
		} else {
			// Just add the character
			working += ch;
		} 
		
		// Check for EOF (this is last character, add if non WS)
		if ( idx+1 >= (int)rawdata.length() ) {
			tokReady = (working.length() > 0);
		}

		// If a symbol/token read, process it
		if ( tokReady == true ) {

			// Iterate through the symbols until you find the one
			it = symbolTable.begin();
			while ( (it != symbolTable.end()) && (!(*it)->isSymbol(working)) ) {
				it++;
			}

			// Did we find it?
			if ( it == symbolTable.end() ) {
				// throw new a fit
				message = "Parse error : symbol \"" + working + "\" does not match any symbol in grammar.";
				throw new bfsCfgParserError( message );

			} else {
				// Push the tuple onto the tokenized stream
				logMessage( CONFIG_VRBLOG_LEVEL, "SYMBOL FOUND : %s [%s]\n", (*it)->getName().c_str(), working.c_str() );
				toked.push_back( make_tuple(working, (*it)) );
			}

			// Clear the working string
			working.clear();
			tokReady = false;
		} 

		// Increment the index in the tokenizing
		idx ++;
	}

	// Add the end symbol and return, no return code
	logMessage( CONFIG_VRBLOG_LEVEL, "SYMBOL FOUND : END [END]" );
	toked.push_back( make_tuple("END", endSymbol) );
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCfgParser::executeParser
// Description  : do the recursive execution of the parser (leftmost BFS)
//
// Inputs       : res - the symbol we are currently resolving
//                toks - token stream (previous parsed)
//                curs - cursor index into the stream of tokens
//                dep - the depth of the current recusion 
// Outputs      : number of tokens consumed, 0 of no match or -1 if failure

int bfsCfgParser::executeParser( bfsCfgParserSymbol *res, tokStream_t & toks, int idx, int dep, parseTree_t * & match ) {

	// Local variables
	parseTree_t *ptree, *submatch;
	cfgProductions_t::iterator it;
	cfgSymbols_t::iterator it2;
	cfgSymbols_t right;
	string message, token;
	int nidx, syms, i;
	bool failed;
	vector<parseTree_t *> submatches;

	// Print what we are trying to do 
	logMessage( CONFIG_VRBLOG_LEVEL, "%*sTrying symbol [%s]", dep*2, "", res->getName().c_str() );
	if ( dep > MAX_bfsCfgParser_RECURSION_DEPTH ) {
		message = "Max recursion depth reached in parser, aborting parser.";
		throw new bfsCfgParserError( message );
	}

	// Setup the parse tree structure
	ptree = new parseTree_t;
	ptree->symbol = res;

	// If this is a non-terminal, see if we can parse the productions
	if ( res->getSymbolType() == CFG_NONTERMINAL ) {

		// Walk the productions of that type
		for ( it=productions.begin(); it!=productions.end(); it++ ) {
			if ( res->getName() == (*it)->getLeft()->getName() ) {

				// Walk the right hand side of the production
				logMessage( CONFIG_VRBLOG_LEVEL, "%*sTrying production [%s]", dep*2, "", (*it)->toString().c_str() );
				failed = false;
				right = (*it)->getRight();
				nidx = idx;
				for ( it2=right.begin(); (!failed) && (it2!=right.end()); it2++ ) {
					if ( (syms = executeParser((*it2), toks, nidx, dep+1, submatch)) > 0 ) {
						nidx += syms;
						submatches.push_back( submatch );
					} else {
						failed = true;
						submatches.clear(); // TODO - clean up these
					}
				}

				// Check if we were succesful or not
				if ( ! failed ) {

					// Log the success
					message = "";
					for ( i=idx; i<nidx; i++ ) {
						token = get<0>(toks.at(i));
						message += " " + token;
					}
					logMessage( CONFIG_VRBLOG_LEVEL, "%*sSuccess [%s], [%d symbols]: %s ", 
						dep*2, "", (*it)->toString().c_str(), nidx-idx, message.c_str() );

					// Setup the parse tree, return
					ptree->ntmatch = submatches;
					match = ptree;
					return( nidx-idx ) ;
				}
			}
		}

		// Did not match, cleanup, return zero
		freeParseTree( ptree );
		match = NULL;
		return( 0 );
	}

	// Now test if this is not reasonable
	if ( res->getSymbolType() != CFG_TERMINAL ) {
		message = "Bad symbol type in parser : " + res->getName();
		throw new bfsCfgParserError( message );
	}

	// Check if it matches
	if ( get<1>(toks.at(idx))->getName() == res->getName() ) {
		logMessage( CONFIG_VRBLOG_LEVEL, "%*sNon-terminal match (at index %d): %s == %s", dep*2, "", idx,
			get<1>(toks.at(idx))->getName().c_str(), res->getName().c_str() );		
		ptree->matched = toks.at(idx);
		match = ptree;
		return( 1 );
	} else {
		logMessage( CONFIG_VRBLOG_LEVEL, "%*sNon-terminal not match (at index %d): %s != %s", dep*2, "", idx,
			get<1>(toks.at(idx))->getName().c_str(), res->getName().c_str() );
	}

	// Does not match, return no match
	return( 0 );
}



