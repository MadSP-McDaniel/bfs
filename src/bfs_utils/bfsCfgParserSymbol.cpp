////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsRegExpression.cpp
//  Description   : This file contains the implementation of the regular
//                  expression matcher for the BFS utilities.
//
//  Author        : Patrick McDaniel
//  Created       : Wed May 19 07:00

// Include Files
#include <bfsCfgParserSymbol.h>

//
// Class methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::bfsCfgParserSymbol
// Description  : Terminal attribute constructor
//
// Inputs       : sym - the symbol name
//                re - the string regular expression
// Outputs      : none

bfsCfgParserSymbol::bfsCfgParserSymbol(string sym, string re) :
    symType(CFG_TERMINAL), 
    symName(sym), 
    symbolMatcher(re),
    noMatch(false) {
        
      // Return, no return code
      return;
  }

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsRegExpression::isSymbol
// Description  : Match a string to a symbol
//
// Inputs       : str - the string to match (is it this symbol)
// Outputs      : 1 if match, 

int bfsCfgParserSymbol::isSymbol(string str) {
  return( (noMatch) ? 0 : symbolMatcher.match(str) );
}
