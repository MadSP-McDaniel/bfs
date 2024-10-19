#ifndef CFGPROD_INCLUDED
#define CFGPROD_INCLUDED

/////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCfgParserProduction.h
//  Description   : This file contains the class definition for the context free 
//                  grammar production class.  The object contains all of the 
//                  information needed to define a production of symbols in the 
//                  grammar.
//
//   Author       : Patrick McDaniel
//   Created      : Sun Apr  4 10:36:11 EDT 2021
//

// Include files

// Project incluides
#include <bfsCfgParserError.h>

// C++/STL Isms

// Definitions
class bfsCfgParserProduction;
typedef vector<bfsCfgParserProduction *> cfgProductions_t;  // Set of symbol defintions

//
// Class Definition
class bfsCfgParserProduction {

public:

	//
	// Constructors and destructors

    // Defatul constructor
	bfsCfgParserProduction( void) {
    }

    // Destructor
	virtual ~bfsCfgParserProduction() {
    }

	//
	// Access Methods

    // Get the left hand side of the production
    bfsCfgParserSymbol * getLeft( void ) {
        return( left );
    }

    // Get the left hand side of the production
    cfgSymbols_t & getRight( void ) {
        return( right );
    }

    // Set the left hand side of the production
    void setLeftHandSide( bfsCfgParserSymbol * sym ) {
        if ( sym == NULL ) {
            string message = "NULL value in setting LHS of production.";
            throw new bfsCfgParserError( message );
        }
        left = sym;
        return;
    }

    // Add a symbol to the right hand side of the production
    void addRightHandSide( bfsCfgParserSymbol * sym ) {
        if ( sym == NULL ) {
            string message = "NULL value in adding RHS of production.";
            throw new bfsCfgParserError( message );
        }
        right.push_back( sym );
        return;
    }

	//
	// Class Methods

    // Provide a string describing the production
    string toString( void )  {
      	cfgSymbols_t::iterator it;
        string str = left->getName() + " ->";
        for ( it=right.begin(); it!=right.end(); it++ ) {
            str += " " + (*it)->getName();
        }
        return( str );
    }


private:

	//
	// Private Methods
	// 
	// Class Data

    bfsCfgParserSymbol * left;
      // Left hand side symbol (non-terminal)

    cfgSymbols_t right;
      // The right hand symbols

	//
	// Static class data

};

#endif