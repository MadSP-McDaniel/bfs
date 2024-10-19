////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfs_base64.cpp
//  Description   : This file contains a collection of utility functions that
//                  implement a base 64 encoder/decoder.
//
//  Author        : Patrick McDaniel
//  Created       : Sat 01 May 2021 08:31:09 AM EDT
//

// Include files
#include <ctype.h>

// Project include files
#include <bfs_log.h>
#include <bfs_util.h>
#include <bfs_base64.h>
#include <bfsUtilError.h>

// Static data

// This is the encoding of digits for the library.
static const string BASE64_ENCODING = BASE64_DIGITS;

// Functions
        
///////////////////////////////////////////////////////////////////////////////
//
// Function     : bfs_fromBase64
// Description  : Convert the b64 encoded string to binary (RFC 4648)
//
// Inputs       : encoded - the base 64 encoded string
//                buf - the buffer to place the binary data in
// Outputs      : the length of the buffer or throws exception

int bfs_fromBase64( const string & encoded, bfsFlexibleBuffer & buf ) {

	// Local variables
	bfs_size_t idx = 0, bidx = 0, len;
	int digit;
	string message;

	// Check for empty buffer, return empty string
	if ( encoded.length() == 0 ) {
		buf.burn();
		return( 0 );
    }

    // Sanity check length (look for non /4 length)
    if ( encoded.length()%4 != 0 ) {
		// Error weird length
		message = "Illegal base 64 encoded string, bad length " + to_string(encoded.length()) + "%4 != 0",
		throw new bfsUtilError( message );
    }

	// Sanity check characters (look for bad ones)
	size_t pos;
	if ( (pos=encoded.find_first_not_of(BASE64_LEGAL_CHARS)) != string::npos ) {
		throw new bfsUtilError("Illegal base 64 encoded string : " + encoded );
	}

	// See how long the output should be, setup buffer to receive it
	len = ((bfs_size_t)encoded.length()/4)*3;
	len -= (encoded[encoded.length()-1] == '=');
	len -= (encoded[encoded.length()-2] == '=');
	buf.resetWithAlloc(len);

	// Walk the string decoding, process the base 64 digits
	while ( idx < (bfs_size_t)encoded.length() ) {
		digit = (bfs_fromB64Digit(encoded[idx])<<2)|(bfs_fromB64Digit(encoded[idx+1])>>4);
		buf.getBuffer()[bidx++] = (char)digit;
		if ( bidx < len ) {
			digit = (bfs_fromB64Digit(encoded[idx+1])<<4)|(bfs_fromB64Digit(encoded[idx+2])>>2);
			buf.getBuffer()[bidx++] = (char)digit;
		}
		if ( bidx < len ) {
			digit = (bfs_fromB64Digit(encoded[idx+2])<<6)|(bfs_fromB64Digit(encoded[idx+3]));
			buf.getBuffer()[bidx++] = (char)digit;
		}
		idx += 4; // Increment the index
	}

	// Return the buffer
	return( buf.getLength() );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfs_toBase64
// Description  : Convert the buffer to a base 64 representation (RFC 4648)
//
// Inputs       : buf - buffer to encode into string
//                encoded - string to encode into
// Outputs      : length of the string or throws exception

int bfs_toBase64( bfsFlexibleBuffer & buf, string & encoded ) {

	// Local variables
	unsigned char one, two, three, four, cbuf[3];
	bfsFlexibleBuffer nbuf;
	int idx = 0, padbytes = 0;
	char pad = 0x0;

	// Check for empty buffer
	if ( buf.getLength() == 0 ) {
		encoded = "";
		return( 0 );
	}

	// Pad out the buffer copy
	// nbuf.setData( buf.getBuffer(), buf.getLength() );
	nbuf = buf;
	while ( nbuf.getLength() % 3 != 0 ) {
		nbuf.addTrailer( pad );
		padbytes ++;
	}

	// Clear string, do the processing of characters
	encoded = "";
	while ( idx < (int)nbuf.getLength() ) {

		// Convert to unsigned characters, do the conversion
		memcpy( cbuf, &nbuf.getBuffer()[idx], 3 );
		one = (unsigned char)(cbuf[0]>>2);
		encoded += bfs_base64Encoding(one);
		two = (unsigned char)(((cbuf[0]&3)<<4)|(cbuf[1]>>4));
		encoded += bfs_base64Encoding(two);

		// Make sure you handle the pad characters
		if ( (padbytes > 1) and (idx+3 >= (int)nbuf.getLength()) ) {
			encoded += '=';
		} else {
			three = (unsigned char)(((cbuf[1]&15)<<2)|(cbuf[2]>>6));
			encoded += bfs_base64Encoding(three);
		}
		if ( (padbytes > 0) and (idx+3 >= (int)nbuf.getLength()) ) {
			encoded += '=';
		} else {
			four = (cbuf[2]&63);
			encoded += bfs_base64Encoding(four);
		}

		// Move to next block of the buffer
		idx += 3;
	}

	// Return the encoded string length
	return( (int)encoded.length() );
}

// 
// Support functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfs_base64Encoding
// Description  : Get the based 64 encoded character
//
// Inputs       : idx - value to encode
// Outputs      : char encoding or throws exception on error

char bfs_base64Encoding( unsigned idx ) {
	
	// Local variables
	string message;

	// Sanity check the input	
	if ( idx > BASE64_ENCODING.length() ) {
		// Error writing too much data
		message = "Illegal base 64 encoded character (" + to_string(idx) + ")";
		throw new bfsUtilError( message );
	}

	// Return the encoding
	return( BASE64_ENCODING[idx] );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfs_fromB64Digit
// Description  : Convert a base 64 digit into its int representation
//
// Inputs       : ch - the characters to encde
// Outputs      : decoded value or throws exception on error

int bfs_fromB64Digit( char ch ) {

	// Local variables
	string message;

	// Check by class of digit
	if ( isupper(ch) ) {
		return( ch-'A' );
	} else if ( islower(ch) ) {
		return( (ch-'a')+26 );
	} else if ( isdigit(ch) ) {
		return( (ch-'0')+52 );
	} else if ( ch == '+' ) {
		return( 62 );
	} else if ( ch == '/' ) {
		return( 63 );
	} 

	// Error writing too much data
	message = "Illegal base 64 encoding character (" + to_string((int)ch) + ")";
	throw new bfsUtilError( message );
}

#ifndef __BFS_ENCLAVE_MODE
////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfs_base64_utest
// Description  : Execute a unit test on the base64 implementation.
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int bfs_base64_utest( void ) {

	// Test vectors from (RFC 4648)
	const char * bfsBase64TestVectors[14] = {
   		"", "",
   		"f", "Zg==",
   		"fo", "Zm8=",
   		"foo", "Zm9v",
   		"foob", "Zm9vYg==",
   		"fooba", "Zm9vYmE=",
   		"foobar", "Zm9vYmFy"
	};

	// Local variables
	bfsFlexibleBuffer buf, enc;
	string encoded;
	uint32_t i, len;

	// Log, then do the encoding test from RFC 4648
	logMessage( UTIL_LOG_LEVEL, "Exceuting bfs base 64 tests." );
	for ( i=0; i<14; i+=2 ) {

		// Convert into a BASE 64 encoding
		buf.setData( bfsBase64TestVectors[i], (bfs_size_t)strlen(bfsBase64TestVectors[i]) );
		logMessage( UTIL_VRBLOG_LEVEL, "Encoding [%s]", buf.toString().c_str() );
		bfs_toBase64( buf, encoded );
		if ( encoded.compare(bfsBase64TestVectors[i+1]) != 0 ) {
			logMessage( LOG_ERROR_LEVEL, "Failed base 64 encoding compare, generated [%s], expected [%s]", 
				encoded.c_str(), bfsBase64TestVectors[i+1] );
			return( -1 );
		}
		logMessage( UTIL_LOG_LEVEL, "Correctly encoded [%s] as [%s]", encoded.c_str(), bfsBase64TestVectors[i+1] );
		// Convert from base 64 to the string
		encoded = bfsBase64TestVectors[i+1];
		bfs_fromBase64( encoded, buf );
		logMessage( UTIL_VRBLOG_LEVEL, "Decoded [%s]", buf.toString().c_str() );
		if ( buf.getLength() == 0 ) {
		  	if ( strlen(bfsBase64TestVectors[i]) != 0 ) {
				logMessage( LOG_ERROR_LEVEL, "Failed base 64 de-encoding 0 length buffer." );
				return( -1 );
			}
		} else if ( strcmp(buf.getBuffer(), bfsBase64TestVectors[i]) != 0 ) {
			logMessage( LOG_ERROR_LEVEL, "Failed base 64 de-encoding compare, generated [%s], expected [%s]", 
				buf.getBuffer(), bfsBase64TestVectors[i+1] );
			return( -1 );
		}
		logMessage( UTIL_LOG_LEVEL, "Correctly decoded [%s] as [%s]", encoded.c_str(), bfsBase64TestVectors[i+1] );

	}

	// Now just do some random encodings
	for ( i=0; i<BFS_BASE64_UTEST_ITERATIONS; i++ ) {

		// Create some random data
		len = get_random_value(1, 64);
		buf.resetWithAlloc( len );
		get_random_data( buf.getBuffer(), len );
		
		// Encode, then decode the data
		bfs_toBase64( buf, encoded );
		bfs_fromBase64( encoded, enc );

		// Now compare the 
		if ( buf != enc ) {
			logMessage( LOG_ERROR_LEVEL, "Failed random data encode/decode compare." );
			return( -1 );
		} else {
			logMessage( UTIL_LOG_LEVEL, "Success en/decoded base64 test string : %s", encoded.c_str() );
		}
	}

	// Log, return successfully
	logMessage( UTIL_LOG_LEVEL, "bfs base 64 test completed successfully." );
	return( 0 );
}
#endif
