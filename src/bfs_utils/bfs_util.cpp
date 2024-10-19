////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfs_util.h
//  Description   : This file contains a collection of utility functions that
//                  all layers of the BFS can access.
//
//  Author   : Patrick McDaniel
//  Created  : Tue 16 Mar 2021 08:22:23 AM EDT
//

// Include files
#ifdef __BFS_ENCLAVE_MODE
// TODO
#else
#include <gcrypt.h>
#endif

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <string>

// Project Include Files
#include "bfs_log.h"
#include "bfs_util.h"

//
// Defines

//
// Global data

#ifndef __BFS_ENCLAVE_MODE
int gcrypt_initialized =
	0; // Flag indicating the library needs to be initialized
gcry_md_hd_t *hfunc = NULL; // A pointer to the gcrypt hash structure

//
// Local functions
int init_gcrypt(void); // initialize the GCRYPT library

// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : generate_md5_signature
// Description  : Generate MD5 signature from buffer
//
// Inputs       : buf - the buffer to generate the signature
//                size - the size of the buffer (in bytes)
//                sig - the signature buffer
//                sigsz - ptr to the size of the signature buffer
//                        (set to sig length when done)
// Outputs      : -1 if failure or 0 if successful

int generate_md5_signature(char *buf, uint32_t size, char *sig,
						   uint32_t *sigsz) {

	// Local variables
	unsigned char *hvalue;

	// Init as needed
	init_gcrypt();

	// Check the signature length
	if (*sigsz < BFSUTIL_HASH_LENGTH) {
		logMessage(LOG_ERROR_LEVEL, "Signature buffer too short  [%d<%d]",
				   *sigsz, BFSUTIL_HASH_LENGTH);
		return (-1);
	}
	*sigsz = BFSUTIL_HASH_LENGTH;

	// Perform the signature operation
	gcry_md_reset(*hfunc);
	gcry_md_write(*hfunc, buf, size);
	gcry_md_final(*hfunc);
	hvalue = gcry_md_read(*hfunc, 0);

	// Check the result
	if (hvalue == NULL) {
		logMessage(LOG_ERROR_LEVEL, "Signature generation failed [NULL]");
		return (-1);
	}

	// Copy the signature bytes, return successfully
	memcpy(sig, hvalue, *sigsz);
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_random_value
// Description  : Using strong randomness, generate random number
//
// Inputs       : min - the minimum number
//                max - the maximum number
// Outputs      : the random value

uint32_t get_random_value(uint64_t min, uint64_t max) {

	// Local variables
	uint64_t val;
	uint64_t range_length = max - min + 1;

	init_gcrypt();
#if 0
	gcry_randomize( &val, sizeof(val), GCRY_STRONG_RANDOM );
#else
	gcry_randomize(&val, sizeof(val), GCRY_WEAK_RANDOM);
#endif
	// Adjust to range
	// range_length==0 when min=0 & max=UINT32_MAX due to integer overflow
	if (range_length != 0) {
		val = (val % range_length) + min;
	}
	return (uint32_t)val;
}

///////////////////////////////////////////////////////////////////////////////
//
// Function     : get_random_signed_value
// Description  : Using strong randomness, generate random number (signed)
//
// Inputs       : min - the minimum number
//                max - the maximum number
// Outputs      : the random value

int32_t get_random_signed_value(int32_t min, int32_t max) {

	// Local variables
	int32_t val;
	int32_t range_length = max - min + 1;
	init_gcrypt();
#if 0
	gcry_randomize( &val, sizeof(val), GCRY_STRONG_RANDOM );
#else
	gcry_randomize(&val, sizeof(val), GCRY_WEAK_RANDOM);
#endif
	// Adjust to range
	// range_length==0 when min=0 & max=UINT32_MAX due to integer overflow
	if (range_length != 0) {
		val = (int32_t)(val % range_length) + min;
	}
	return (val);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_random_data
// Description  : Using gcrypt randomness, generate random data
//
// Inputs       : blk - place to put the random data
//                sz - the size of the data
// Outputs      : the random value

int get_random_data(char *blk, uint32_t sz) {

#if 0
	gcry_randomize( blk, sz, GCRY_STRONG_RANDOM );
#else
	gcry_randomize(blk, sz, GCRY_WEAK_RANDOM);
#endif
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : getRandomAlphanumericData
// Description  : Using gcrypt randomness, generate random alphanumberic
//                text data
//
// Inputs       : blk - place to put the random characters
//                sz - the size of the data
// Outputs      : the random value

int get_random_alphanumeric_data(char *blk, uint32_t sz) {

	/* Local variables */
	const char *allchars = BFSUTIL_ALLCHARS;
	uint32_t i, nochars;

	/* Pick characters and return */
	nochars = (uint32_t)strlen(allchars);
	for (i = 0; i < sz; i++) {
		blk[i] = allchars[get_random_value(0, nochars - 1)];
	}
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : compareTimes
// Description  : Compare two timer values
//
// Inputs       : tm1 - the first time value
//                tm2 - the second time value
// Outputs      : >0, 0, <0 if tmr1 < tmr, tmr1==tmr2, tmr1 > tmr2 ,
// respectively

long compareTimes(struct timeval *tm1, struct timeval *tm2) {

	// compute the difference in usec
	long retval = 0;
	if (tm2->tv_usec < tm1->tv_usec) { // subtract 1 to do carry
		retval = (tm2->tv_sec - tm1->tv_sec - 1) * 1000000L;
		retval += ((tm2->tv_usec + 1000000L) - tm1->tv_usec);
	} else {
		retval = (tm2->tv_sec - tm1->tv_sec) * 1000000L;
		retval += (tm2->tv_usec - tm1->tv_usec);
	}
	return (retval);
}

//
// Local Functions
GCRY_THREAD_OPTION_PTHREAD_IMPL;

////////////////////////////////////////////////////////////////////////////////
//
// Function     : init_gcrypt
// Description  : initialize the GCRYPT library
//
// Inputs       : tm1 - the first time value
//                tm2 - the second time value
// Outputs      : 0 if successful, -1 if failure

int init_gcrypt(void) {

	// If the GCRYPT interface not initialized
	if (!gcrypt_initialized) {

		// gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pth);

		// Initialize the library
		gcry_error_t err;
		gcry_check_version(GCRYPT_VERSION);

		// Create the hash structure
		hfunc = (gcry_md_hd_t *)malloc(sizeof(gcry_md_hd_t));
		memset(hfunc, 0x0, sizeof(gcry_md_hd_t));
		err = gcry_md_open(hfunc, BFSUTIL_HASH_TYPE, 0);
		if (err != GPG_ERR_NO_ERROR) {
			logMessage(LOG_ERROR_LEVEL, "Unable to init hash algorithm  [%s]",
					   gcry_strerror(err));
			return (-1);
		}

		// Set the initialized flag
		gcrypt_initialized = 1;
	}

	// Return successfully
	return (0);
}
#endif

/* File directory/name operations */

/**
 * @brief Get the basename of a given absolute/relatvie path. Adapted from
 * glibc (libgen) basename. Note that this does not modify the string.
 *
 * @param path path name
 * @return char* file name
 */
const char *bfs_basename(const char *path) {
	const char *p = strrchr(path, '/');
	return p ? p + 1 : path;
}

/**
 * @brief Get the dirname of a given absolute/relatvie path. Adapted from
 * barebox dirname. Note that this modifies the string, so need to make sure to
 * be careful with its use afterwards.
 *
 * @param path path name
 * @return char* directory name
 */
char *bfs_dirname(char *path) {
	static char str[2];
	char *fname;

	if (!strchr(path, '/')) {
		str[0] = '.';
		str[1] = 0;
		return str;
	}

	// adapted from bfs_basename
	fname = strrchr(path, '/');
	fname = fname ? fname + 1 : fname;

	--fname;
	if (path == fname)
		*fname++ = '/';
	*fname = '\0';

	return path;
}

/**
 * @brief Set a bit in a bitmap at the given position.
 *
 * @param nr: bit position
 * @param addr: buffer to set a bit in
 * @return bool:
 */
void bfs_set_bit(uint64_t nr, void *addr) {
	uint8_t shift;
	unsigned char mask; // should be unsigned
	unsigned char *_addr = (unsigned char *)addr;

	// throw off last 3 bits to get byte pos
	// then cast to 8bits (and operation wont produce a number that cant fit
	// within 8 bits) and get last 3 bits to get offset in the curr byte
	_addr += nr >> 3;
	shift = (uint8_t)(nr & 0x07);
	mask = (unsigned char)(1 << shift);
	*_addr |= mask;
}

/**
 * @brief Clear a bit in the bitmap at the given position.
 *
 * @param nr
 * @param addr
 * @return true
 * @return false
 */
void bfs_clear_bit(uint64_t nr, void *addr) {
	uint8_t shift;
	unsigned char mask; // should be unsigned
	unsigned char *_addr = (unsigned char *)addr;

	_addr += nr >> 3;
	shift = (uint8_t)(nr & 0x07);
	mask = (unsigned char)(1 << shift);
	*_addr &= (unsigned char)~mask;
}

/**
 * @brief Test a bit in the bitmap at the given position.
 *
 * @param nr
 * @param addr
 * @return true
 * @return bool: true (1) if bit is set, false (0) otherwise
 */
bool bfs_test_bit(uint64_t nr, void *addr) {
	int mask;
	const unsigned char *_addr = (const unsigned char *)addr;

	_addr += nr >> 3;
	mask = 1 << (nr & 0x07);

	return ((mask & *_addr) != 0);
}

/**
 * @brief Duplicate a c-style string. Simple implementation, not available in
 * the enclave.
 *
 * @param s the string to duplicate
 * @return char* the copy to return
 */
char *bfs_strdup(const char *s) {
	uint64_t len = strlen(s) + 1;
	char *copy = (char *)calloc(len, 1);
	memcpy(copy, s, len - 1);
	copy[len - 1] = '\0';
	return copy;
}
