#ifndef BFS_BASE64_INCLUDED
#define BFS_BASE64_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfs_base64.h
//  Description   : This file contains the definition of a collection of utility
//                  functions that implement a base 64 encoder/decoder.  See
//                  (RFC 4648) for more details on encoding.
//
//  Author        : Patrick McDaniel
//  Created       : Sat 01 May 2021 08:31:09 AM EDT
//

// Include files
#include <bfsFlexibleBuffer.h>

// Defines
#define BFS_BASE64_UTEST_ITERATIONS 10
#define BASE64_DIGITS "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
#define BASE64_LEGAL_CHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/="

// Functions

int bfs_fromBase64( const string & encoded, bfsFlexibleBuffer & buf );
  // Convert the b64 encoded string to binary (RFC 4648)

int bfs_toBase64( bfsFlexibleBuffer & buf, string & encoded );
  // Convert the buffer to a base 64 representation (RFC 4648)

int bfs_base64_utest( void );
  // Execute a unit test on the base64 implementation.

//
// Support functions

char bfs_base64Encoding( unsigned idx );
  // Compute the base 64 encoded characetr

int bfs_fromB64Digit( char ch );
  // Convert a base 64 digit into its int representation


#endif
