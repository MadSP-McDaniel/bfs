#ifndef BFS_BLOCK_ERROR_INCLUDED
#define BFS_BLOCK_ERROR_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsBlockError.h
//  Description   : This file contains the class definition for the error 
//                  exception to throw new when problems arise within the 
//                  bfs block layer.
//
//   Author       : Patrick McDaniel
//   Created      : Fri 14 May 2021 02:58:41 PM EDT
//

// Include files

// Project incluides

// C++/STL Isms
#include <string>
using namespace std;

// Definitions

// 
// An exception class to throw new when things error
struct bfsBlockError : public std::exception {

public: 

	// A basic construcrtor
	bfsBlockError( string & str ) {
		message = str;
	}

	// A char basic construcrtor
	bfsBlockError( const char * str ) {
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