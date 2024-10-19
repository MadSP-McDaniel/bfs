#ifndef CFGERROR_INCLUDED
#define CFGERROR_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCfgParser.h
//  Description   : This file contains the class definition for the error 
//                  exception to throw new when provblems arise.
//
//   Author       : Patrick McDaniel
//   Created      : Sun 04 Apr 2021 10:56:48 AM EDT
//

// Include files

// Project incluides

// C++/STL Isms
#include <string>
using namespace std;

// Definitions

// 
// An exception class to throw new when things error
struct bfsCfgParserError : public std::exception {

public: 

	// A basic construcrtor
	bfsCfgParserError( string str ) {
		message = str;
	}

	// Get the message associated with the error
	string & getMessage( void ) {
		return( message );
	}

private:

	string message;
	  // String describing the error

};

#endif