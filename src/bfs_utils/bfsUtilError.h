#ifndef BFS_UTILERROR_INCLUDED
#define BFS_UTILERROR_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsUtilError.h
//  Description   : This file contains the class definition for the error 
//                  exception to throw new when problems arise within the 
//                  utility functions of the bfs filesystem.
//
//   Author       : Patrick McDaniel
//   Created      : Fri 23 Apr 2021 07:12:13 AM EDT
//

// Include files

// Project incluides

// C++/STL Isms
#include <string>
using namespace std;

// Definitions

// 
// An exception class to throw new when things error
struct bfsUtilError : public std::exception {

public: 

	// A basic construcrtor
	bfsUtilError( string str ) {
		message = str;
	}

	// A basic construcrtor (character version)
	bfsUtilError( const char * str ) {
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