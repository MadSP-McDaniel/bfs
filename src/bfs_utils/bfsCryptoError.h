#ifndef BFS_CRYPTO_ERROR_INCLUDED
#define BFS_CRYPTO_ERROR_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCryptoError.h
//  Description   : This file contains the class definition for the error 
//                  exception to throw new when problems arise within the 
//                  crypto system.
//
//   Author       : Patrick McDaniel
//   Created      : Sat 01 May 2021 05:15:38 AM PDT
//

// Include files

// Project incluides

// C++/STL Isms
#include <string>
using namespace std;

// Definitions

// 
// An exception class to throw new when things error
struct bfsCryptoError : public std::exception {

public: 

	// A basic construcrtor
	bfsCryptoError( string & str ) {
		message = str;
	}

	// A constrant string constructor
	bfsCryptoError( const char * str ) {
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