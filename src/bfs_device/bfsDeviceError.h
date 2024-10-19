#ifndef BFS_DEVICE_ERROR_INCLUDED
#define BFS_DEVICE_ERROR_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsDeviceError.h
//  Description   : This file contains the class definition for the error 
//                  exception to throw new when problems arise within the 
//                  bfs device layer.
//
//   Author       : Patrick McDaniel
//   Created      : Sun 09 May 2021 06:53:31 AM PDT
//

// Include files

// Project incluides

// C++/STL Isms
#include <string>
using namespace std;

// Definitions

// 
// An exception class to throw new when things error
struct bfsDeviceError : public std::exception {

public: 

	// A basic construcrtor
	bfsDeviceError( string & str ) {
		message = str;
	}

	// A char basic construcrtor
	bfsDeviceError( const char * str ) {
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