#ifndef CFGCFGERROR_INCLUDED
#define CFGCFGERROR_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCfgError.h
//  Description   : This file contains the class definition for the error 
//                  exception to throw new when problems arise within the 
//                  configuration system.
//
//   Author       : Patrick McDaniel
//   Created      : Sun 11 Apr 2021 02:43:42 PM EDT
//

// Include files

// Project incluides

// C++/STL Isms
#include <string>
using namespace std;

// Definitions

// 
// An exception class to throw new when things error
struct bfsCfgError : public std::exception {

public: 

	// A basic construcrtor
	bfsCfgError( string str ) {
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