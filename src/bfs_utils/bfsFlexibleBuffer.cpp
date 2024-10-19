////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsFlexibleBuffer.cpp
//  Description   : This file contains the implementation for the flexible
//  buffer,
//                  a buffer that preallocates space for the header and tailer
//                  data that is used for things like network protocols and
//                  cryptographic functions.
//
//  Author		  : Patrick McDaniel
//  Created	      : Thu 22 Apr 2021 06:31:56 PM EDT
//

// Includes
#include <string.h>

// Project Includes
#ifdef __BFS_ENCLAVE_MODE
#include "bfs_enclave_t.h" /* For ocalls */
#include "sgx_trts.h"
#else
#include "bfs_util_ocalls.h"
#endif
#include <bfsFlexibleBuffer.h>
#include <bfsUtilError.h>
#include <bfs_log.h>
#include <bfs_util.h>

// Local defines
#define BFS_FLEX_UTEST_ITERATIONS 10
#define BFS_FLEX_UTEST_STACKSZ 10
#define BFS_FLEX_HDTR_UTEST_ITERATIONS 100
#define BFS_FLEX_UTEST_BASESZ 256
#define BFS_FLEX_UTEST_APPNDSZ 64 // Must be < base size in previous line

//
// Class Data

// Descriptive strings for the flexible buffer data types
const char *bfsFlexibleBuffer::bfs_flexbuf_dtypes_strings[] = {
	"BFS_FLEXBUF_BYTE", "BFS_FLEXBUF_BOOL", "BFS_FLEXBUF_UI16",
	"BFS_FLEXBUF_I16",	"BFS_FLEXBUF_UI32", "BFS_FLEXBUF_I32",
	"BFS_FLEXBUF_UI64", "BFS_FLEXBUF_I64",	"BFS_FLEXBUF_DATA"};

//
// Class Methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::bfsFlexibleBuffer
// Description  : Default constructor
//
// Inputs       : none
// Outputs      : none

bfsFlexibleBuffer::bfsFlexibleBuffer(void)
	: buffer(NULL), allocation(0), hlength(0), length(0), tlength(0),
	  burnOnFree(BFSFLEXBUF_DEFAULT_BURN) {

	// Return, no return code
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::bfsFlexibleBuffer
// Description  : Attribute constructor
//
// Inputs       : dat - data to put into the buffer
//                len - the length of the data
//                hpadsz - the initial header size to allocate
//                tpadsz - the initial trailer size to allocate
// Outputs      : none

bfsFlexibleBuffer::bfsFlexibleBuffer(char *dat, bfs_size_t len,
									 bfs_size_t hpadsz, bfs_size_t tpadsz)
	: bfsFlexibleBuffer() {

	// Set the data length
	if (dat)
		setData(dat, len);
	logMessage(UTIL_VRBLOG_LEVEL,
			   "Creating flex buffer sz=%d, head=%d, tail=%d", len, hpadsz,
			   tpadsz);

	// Return, no return code
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::bfsFlexibleBuffer
// Description  : Copy constructor
//
// Inputs       : cpy - the buffer object to copy
// Outputs      : none

bfsFlexibleBuffer::bfsFlexibleBuffer(const bfsFlexibleBuffer &cpy)
	: buffer(NULL), allocation(cpy.allocation), hlength(cpy.hlength),
	  length(cpy.length), tlength(cpy.tlength), burnOnFree(cpy.burnOnFree) {

	// Duplicate the buffer data
#ifdef __BFS_ENCLAVE_MODE
	if ((ocall_do_alloc((long *)&buffer, allocation) != SGX_SUCCESS) ||
		(buffer == (long)NULL))
		throw new bfsUtilError("Failed bfsFlexibleBuffer copy constructor");

	if (sgx_is_outside_enclave(buffer, allocation) != 1)
		throw new bfsUtilError(
			"Failed bfsFlexibleBuffer ocall_do_alloc: addr range is "
			"not entirely outside enclave "
			"(may be corrupt source ptr)");
#else
	buffer = new char[allocation];
#endif

	memcpy(buffer, cpy.buffer, allocation);

	// Return, no return code
	return;
}

bfsSecureFlexibleBuffer::bfsSecureFlexibleBuffer(const bfsFlexibleBuffer &cpy) {
	buffer = NULL;
	allocation = cpy.getAllocation();
	hlength = cpy.getHLength();
	length = cpy.getLength();
	tlength = cpy.getTLength();
	burnOnFree = cpy.getBurnOnFree();

	// Duplicate the buffer data. Secure buffers should always be in secure
	// memory (for sgx-based builds), so if we are doing sgx-based builds (with
	// __BFS_ENCLAVE_MODE flag) then always check that the allocation is in
	// enclave memory.
	buffer = new char[allocation];

#ifdef __BFS_ENCLAVE_MODE
	if (sgx_is_within_enclave(buffer, allocation) != 1)
		throw new bfsUtilError(
			"Failed bfsSecureFlexibleBuffer alloc: addr range is "
			"not entirely inside enclave");
#endif

	memcpy(buffer, cpy.getFullBuffer(), allocation);

	// Return, no return code
	return;
}

bfsSecureFlexibleBuffer::bfsSecureFlexibleBuffer(char *dat, uint32_t len,
												 bfs_size_t hpadsz,
												 bfs_size_t tpadsz)
	: bfsSecureFlexibleBuffer() {

	// Set the data length
	if (dat)
		setData(dat, len);
	logMessage(UTIL_VRBLOG_LEVEL,
			   "Creating flex buffer sz=%d, head=%d, tail=%d", len, hpadsz,
			   tpadsz);

	// Return, no return code
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::~bfsFlexibleBuffer
// Description  : Destructor
//
// Inputs       : none
// Outputs      : none

bfsFlexibleBuffer::~bfsFlexibleBuffer(void) {

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double dl_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&dl_start_time) != SGX_SUCCESS) ||
	// 			(dl_start_time == -1))
	// 			return;
	// 	}
	// #endif

	// See if we need to burn the buffer
	logMessage(UTIL_VRBLOG_LEVEL, "Deleting flex buffer sz=%d", length);
	if (burnOnFree == true) {
		memset(buffer, 0x0, allocation);
	}

	// Cleanup the data
	if (buffer != NULL) {
#ifdef __BFS_ENCLAVE_MODE
		if (sgx_is_outside_enclave(buffer, allocation) != 1)
			throw new bfsUtilError(
				"Failed destructor: addr range is not entirely outside "
				"enclave (may be corrupt source ptr)");

		if ((ocall_delete_allocation((long)buffer) != SGX_SUCCESS))
			throw new bfsUtilError("Failed ocall_delete_allocation");
#else
		delete[] buffer;
#endif
		buffer = NULL;
	}

	allocation = hlength = length = tlength = 0;

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double dl_end_time = 0.0;
	// 	if (bfsUtilLayer::perf_test()) {
	// 		if ((ocall_get_time2(&dl_end_time) != SGX_SUCCESS) ||
	// 			(dl_end_time == -1))
	// 			return;

	// 		logMessage(UTIL_VRBLOG_LEVEL,
	// 				   "===== Time in flexbuffer destructor: %.3f us",
	// 				   dl_end_time - dl_start_time);
	// 	}
	// #endif
}

bfsSecureFlexibleBuffer::~bfsSecureFlexibleBuffer(void) {

	// See if we need to burn the buffer
	logMessage(UTIL_VRBLOG_LEVEL, "Deleting flex buffer sz=%d", length);
	if (burnOnFree == true) {
		memset(buffer, 0x0, allocation);
	}

	// Cleanup the data
	if (buffer != NULL) {
#ifdef __BFS_ENCLAVE_MODE
		if (sgx_is_within_enclave(buffer, allocation) != 1)
			throw new bfsUtilError(
				"Failed destructor: addr range is not entirely inside enclave");
#endif
		delete[] buffer;
		buffer = NULL;
	}

	allocation = hlength = length = tlength = 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::setData
// Description  : Set the data in the buffer (discards previous data)
//
// Inputs       : dat - the data to set
//                len - the length of the data
// Outputs      : the new length of the buffer

bfs_size_t bfsFlexibleBuffer::setData(const char *dat, bfs_size_t len) {

	// Ok, see if we have enough space for the new data (give us some padding)
	// burn(); // QB: dont force burn on setData (buffer should be reset before
	// use; otherwise this resizes the header/buf/trailer and forces unnecessary
	// reallocations); edit: no longer resizing on burn anyway, but still dont
	// need

	// Force lengths to be equal for now (so the header and trailer split the
	// length in half), which should give us enough header space until memmove
	// method fixed; otherwise when later trying to add headers, (len > hlength)
	// will evaluate true and trigger a resize, which will force a re-center and
	// thus we will lose the data (ie it will be stuck in the header portion)
	if (len != length) {
		// if ( len > length ) {
		resizeAllocation(hlength, len, tlength);
	}

	// Copy over the data
	memcpy(&buffer[hlength], dat, len);
	length = len;
	logMessage(UTIL_VRBLOG_LEVEL,
			   "Setting flex buffer base data, size %d (%d/%d/%d, alloc %d)",
			   len, hlength, length, tlength, allocation);
	return (length);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::resetWithAlloc
// Description  : Reset the buffer, preallocating buffer with data of specific
//                length and fill
//
// Inputs       : sz - size of the preallocation
//                fill - byte to fill allocation with
//                hpadsz - minimum header pad
//                tpadsz - minimum trailer pad
//                burn_on_reset - flag indicating whether to burn and set
//                default sizes in the buf
//                                (generally should be unused)
// Outputs      : the length of the data after allocation

bfs_size_t bfsFlexibleBuffer::resetWithAlloc(bfs_size_t sz, char fill,
											 bfs_size_t hpadsz,
											 bfs_size_t tpadsz,
											 bool burn_on_reset) {

	// Resize to the minimums
	if (burn_on_reset)
		burn();

	// resize if the header/trailer are shorter than requested, or the length is
	// not exactly as requested
	if ((length != sz) || (hlength < hpadsz) || (tlength < tpadsz)) {
		resizeAllocation(hpadsz, sz, tpadsz);
	}

	// Portion out the allocation
	/**
	 * QB: seems like missing an assignment to hlength below, so the old hlength
	 * value persists but these get updated, which sometimes end up causing the
	 * total size to be > the allocation value. Need to update hlength.
	 */
	// Edit: these are assigned in resize call
	// bfs_size_t remaining = allocation - sz;
	// tlength = tpadsz; // tlength = remaining / 2;
	// remaining = remaining - tlength;
	// hlength = remaining; // hlength = remaining;
	// length = sz;

	// Edit: checked above
	// if ((hlength < hpadsz) || (tlength < tpadsz) || (length != sz)) {
	//     logMessage(LOG_ERROR_LEVEL, "Incorrect sizes in resetWithAlloc");
	//     abort();
	// }

	// Set the data, return
	memset(&buffer[hlength], fill, sz);
	return (sz);
};

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::addHeader
// Description  : Add a header to the flexible buffer (prepend)
//
// Inputs       : dat - the data to add as a header
//                len - the lnegth of the header
// Outputs      : the new length of the buffer

bfs_size_t bfsFlexibleBuffer::addHeader(char *dat, bfs_size_t len) {

	// As needed, reset the header length
	if (len > hlength) {
		resizeAllocation(BFSFLEXBUF_DEFAULT_PAD + len, length, tlength);
	}

	// Now add the header
	logMessage(UTIL_VRBLOG_LEVEL, "Adding flex buffer header, size %d", len);
	memcpy(&buffer[hlength - len], dat, len);
	hlength -= len;
	length += len;

	// Return the new buffer length
	return (length);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::removeHeader
// Description  : Remove a header from the flexible buffer
//
// Inputs       : dat - place to put data
//                len - the length of the header to remove
// Outputs      : the new length of the buffer

bfs_size_t bfsFlexibleBuffer::removeHeader(char *dat, bfs_size_t len) {

	// Check to make sure we are not going past the end of the data
	if (buffer == NULL) {
		throw new bfsUtilError("Flexible buffer remove header on NULL data");
	}

	// Check to make sure we are not going past the end of the data
	if (len > length) {
		throw new bfsUtilError("Flexible buffer remove header underflow");
	}

	// If NULL, no copy (otherwise copy)
	logMessage(UTIL_VRBLOG_LEVEL, "Removing flex buffer header, size %d", len);
	if (dat != NULL) {
		memcpy(dat, &buffer[hlength], len);
	}

	// Just skip the header part, return
	hlength += len;
	length -= len;
	return (length);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::addTrailer
// Description  : Add a trailer to the flexible buffer (append)
//
// Inputs       : dat - the data to add as a trailer
//                len - the length of the trailer
// Outputs      : the new length of the buffer

bfs_size_t bfsFlexibleBuffer::addTrailer(char *dat, bfs_size_t len) {

	// As needed, reset the header length
	if (len > tlength) {
		resizeAllocation(hlength, length, len + BFSFLEXBUF_DEFAULT_PAD);
	}

	// Now add the header
	logMessage(UTIL_VRBLOG_LEVEL, "Adding flex buffer trailer, size %d", len);
	memcpy(&buffer[hlength + length], dat, len);
	tlength -= len;
	length += len;

	// Return the new buffer length
	return (length);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::removeTrailer
// Description  : Remove the trailer from the buffer
//
// Inputs       : dat - place to put data
//                len - the length of the trailer to remove
// Outputs      : the new length of the buffer

bfs_size_t bfsFlexibleBuffer::removeTrailer(char *const dat, bfs_size_t len) {

	// Check to make sure we are not going past the end of the data
	if (buffer == NULL) {
		throw new bfsUtilError("Flexible buffer remove header on NULL data");
	}

	// Check to make sure we are not going past the end of the data
	if (length < len) {
		throw new bfsUtilError("Flexible buffer remove trailer underflow");
	}

	// If NULL, no copy (otherwise copy)
	logMessage(UTIL_VRBLOG_LEVEL, "Removing flex buffer trailer, size %d", len);
	if (dat != NULL) {
		memcpy(dat, &buffer[(hlength + length) - len], len);
	}

	// Just skip the header part, return
	tlength += len;
	length -= len;
	return (length);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::toString
// Description  : Create a human readable string describing the buffer
//
// Inputs       : maxdigits - the maximum number of digits to display
// Outputs      : 0 if successful test, -1 if failure

string bfsFlexibleBuffer::toString(int maxdigits) {

	// Local variables
	char strval[129];
	string str;
	uint32_t len = 0;

	// Figure out the bytes to
	if (maxdigits == -1) {
		len = length;
	} else {
		len = (maxdigits < (int)length) ? maxdigits : length;
	}

	// Now encode/create the string
	str = "bfsFlexBuf (len=" + to_string(length) + ") : ";
	bufToString(getBuffer(), len, strval, 128);
	str += strval;

	// Return the string
	return (str);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::operator=
// Description  : The copy operator
//
// Inputs       : fb - the copied buffer
// Outputs      : 0 if successful test, -1 if failure

const bfsFlexibleBuffer &
bfsFlexibleBuffer::operator=(const bfsFlexibleBuffer &fb) {
	setData(fb.getBuffer(), fb.getLength());
	return (*this);
}

//
// Static class methods

#ifndef __BFS_ENCLAVE_MODE
////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::flexBufferUTest
// Description  : Test the flexible buffer implementation
//
// Inputs       : none
// Outputs      : 0 if successful test, -1 if failure

int bfsFlexibleBuffer::flexBufferUTest(void) {

	// The unit test bookeeping structure
	struct {
		bfs_flexbuf_dtypes_t type;
		union {
			char byteT;
			bool boolT;
			uint16_t uint16T;
			int16_t int16T;
			uint32_t uint32T;
			int32_t int32T;
			uint64_t uint64T;
			int64_t int64T;
			struct {
				char block[BFS_FLEX_UTEST_BASESZ];
				uint32_t size;
			} blk;
		} un;
	} headerStack[BFS_FLEX_UTEST_STACKSZ], trailerStack[BFS_FLEX_UTEST_STACKSZ];

	// Local variables
	int i, j, iter, hstkPtr = 0, tstkPtr = 0;
	char newblk[BFS_FLEX_UTEST_BASESZ], byteT, *blkptr;
	bool doheader, adddat, match, boolT, completed;
	bfsFlexibleBuffer *buffer;
	bfs_flexbuf_dtypes_t ntype;
	uint32_t sz, *szptr, uint32T;
	uint16_t uint16T;
	int16_t int16T;
	int32_t int32T;
	uint64_t uint64T;
	int64_t int64T;
	string message;

	// Iterated on different flexible buffer combos
	for (i = 0; i < BFS_FLEX_UTEST_ITERATIONS; i++) {

		// Create a (possibly seeded) buffer
		if (get_random_value(0, 100) <= 50) {
			// Just create an empty buffer
			buffer = new bfsFlexibleBuffer();
		} else {
			// Create a randomized buffer
			sz = get_random_value(1, BFS_FLEX_UTEST_BASESZ);
			get_random_data(newblk, sz);
			buffer = new bfsFlexibleBuffer(newblk, sz);
		}
		hstkPtr = tstkPtr = 0;

		// Iterated on the headers and trailers
		iter = 0;
		completed = false;
		while (!completed) {

			// Figure out add/remove and header or trailer
			if (iter >= BFS_FLEX_HDTR_UTEST_ITERATIONS) {
				// Clearing mode (after iterations done)
				doheader = (hstkPtr > 0);
				adddat = false;
			} else {

				// Figure out what we are doing (header or trailer)
				if ((bool)(get_random_value(0, 100) <= 50)) {
					doheader = true;
					adddat =
						(hstkPtr == 0) || ((hstkPtr < BFS_FLEX_UTEST_STACKSZ) &&
										   (get_random_value(0, 100) <= 50));
				} else {
					doheader = false;
					adddat =
						(tstkPtr == 0) || ((tstkPtr < BFS_FLEX_UTEST_STACKSZ) &&
										   (get_random_value(0, 100) <= 50));
				}
			}
			logMessage(LOG_INFO_LEVEL, "Utest %s operation on %s, index %d",
					   (adddat) ? "add" : "remove",
					   (doheader) ? "header" : "trailer",
					   (doheader) ? hstkPtr - (!adddat) : tstkPtr - (!adddat));

			// Check if we are adding or removing headers/trailers
			if (adddat) {

				// Pick a data type for the creation of a new header/trailer
				ntype = (bfs_flexbuf_dtypes_t)get_random_value(
					BFS_FLEXBUF_BYTE, BFS_FLEXBUF_DATA);
				switch (ntype) {

				case BFS_FLEXBUF_BYTE: // Byte type
					byteT = (char)get_random_value(0, 255);
					if (doheader) {
						headerStack[hstkPtr].un.byteT = byteT;
						headerStack[hstkPtr].type = BFS_FLEXBUF_BYTE;
						*buffer << headerStack[hstkPtr].un.byteT;
						logMessage(LOG_INFO_LEVEL, "Adding byte header, val=%u",
								   byteT);
						hstkPtr++;
					} else {
						trailerStack[tstkPtr].un.byteT = byteT;
						trailerStack[tstkPtr].type = BFS_FLEXBUF_BYTE;
						buffer->addTrailer(trailerStack[tstkPtr].un.byteT);
						logMessage(LOG_INFO_LEVEL,
								   "Adding byte trailer, val=%u", byteT);
						tstkPtr++;
					}
					break;

				case BFS_FLEXBUF_BOOL: // Boolean type
					boolT = (bool)get_random_value(0, 1);
					if (doheader) {
						headerStack[hstkPtr].un.boolT = boolT;
						headerStack[hstkPtr].type = BFS_FLEXBUF_BOOL;
						*buffer << headerStack[hstkPtr].un.boolT;
						logMessage(LOG_INFO_LEVEL, "Adding bool header, val=%s",
								   (boolT) ? "true" : "false");
						hstkPtr++;
					} else {
						trailerStack[tstkPtr].un.boolT = boolT;
						trailerStack[tstkPtr].type = BFS_FLEXBUF_BOOL;
						buffer->addTrailer(trailerStack[tstkPtr].un.boolT);
						logMessage(LOG_INFO_LEVEL,
								   "Adding bool trailer, val=%s",
								   (boolT) ? "true" : "false");
						tstkPtr++;
					}
					break;

				case BFS_FLEXBUF_UI16: // uint16_t type
					uint16T = (uint16_t)get_random_value(0, UINT16_MAX);
					if (doheader) {
						headerStack[hstkPtr].un.uint16T = uint16T;
						headerStack[hstkPtr].type = BFS_FLEXBUF_UI16;
						*buffer << headerStack[hstkPtr].un.uint16T;
						logMessage(LOG_INFO_LEVEL,
								   "Adding uint16 header, val=%u", uint16T);
						hstkPtr++;
					} else {
						trailerStack[tstkPtr].un.uint16T = uint16T;
						trailerStack[tstkPtr].type = BFS_FLEXBUF_UI16;
						buffer->addTrailer(trailerStack[tstkPtr].un.uint16T);
						logMessage(LOG_INFO_LEVEL,
								   "Adding uint16 trailer, val=%u", uint16T);
						tstkPtr++;
					}
					break;

				case BFS_FLEXBUF_I16: // int16_t type
					int16T = (int16_t)get_random_value(0, INT16_MAX);
					if (doheader) {
						headerStack[hstkPtr].un.int16T = int16T;
						headerStack[hstkPtr].type = BFS_FLEXBUF_I16;
						*buffer << headerStack[hstkPtr].un.int16T;
						logMessage(LOG_INFO_LEVEL,
								   "Adding int16 header, val=%d", int16T);
						hstkPtr++;
					} else {
						trailerStack[tstkPtr].un.int16T = int16T;
						trailerStack[tstkPtr].type = BFS_FLEXBUF_I16;
						buffer->addTrailer(trailerStack[tstkPtr].un.int16T);
						logMessage(LOG_INFO_LEVEL,
								   "Adding int16 trailer, val=%d", int16T);
						tstkPtr++;
					}
					break;

				case BFS_FLEXBUF_UI32: // uint32_t type
					uint32T = (uint32_t)get_random_value(0, UINT32_MAX);
					if (doheader) {
						headerStack[hstkPtr].un.uint32T = uint32T;
						headerStack[hstkPtr].type = BFS_FLEXBUF_UI32;
						*buffer << headerStack[hstkPtr].un.uint32T;
						logMessage(LOG_INFO_LEVEL,
								   "Adding uint32 header, val=%u", uint32T);
						hstkPtr++;
					} else {
						trailerStack[tstkPtr].un.uint32T = uint32T;
						trailerStack[tstkPtr].type = BFS_FLEXBUF_UI32;
						buffer->addTrailer(trailerStack[tstkPtr].un.uint32T);
						logMessage(LOG_INFO_LEVEL,
								   "Adding uint32 trailer, val=%u", uint32T);
						tstkPtr++;
					}
					break;

				case BFS_FLEXBUF_I32: // int32_t type
					int32T = (int32_t)get_random_value(0, INT32_MAX);
					if (doheader) {
						headerStack[hstkPtr].un.int32T = int32T;
						headerStack[hstkPtr].type = BFS_FLEXBUF_I32;
						*buffer << headerStack[hstkPtr].un.int32T;
						logMessage(LOG_INFO_LEVEL,
								   "Adding int32 header, val=%d", int32T);
						hstkPtr++;
					} else {
						trailerStack[tstkPtr].un.int32T = int32T;
						trailerStack[tstkPtr].type = BFS_FLEXBUF_I32;
						buffer->addTrailer(trailerStack[tstkPtr].un.int32T);
						logMessage(LOG_INFO_LEVEL,
								   "Adding int32 trailer, val=%d", int32T);
						tstkPtr++;
					}
					break;

				case BFS_FLEXBUF_UI64: // uint64_t type
					get_random_data((char *)&uint64T, sizeof(uint64_t));
					if (doheader) {
						headerStack[hstkPtr].un.uint64T = uint64T;
						headerStack[hstkPtr].type = BFS_FLEXBUF_UI64;
						*buffer << headerStack[hstkPtr].un.uint64T;
						logMessage(LOG_INFO_LEVEL,
								   "Adding uint64 header, val=%lu", uint64T);
						hstkPtr++;
					} else {
						trailerStack[tstkPtr].un.uint64T = uint64T;
						trailerStack[tstkPtr].type = BFS_FLEXBUF_UI64;
						buffer->addTrailer(trailerStack[tstkPtr].un.uint64T);
						logMessage(LOG_INFO_LEVEL,
								   "Adding uint64 trailer, val=%lu", uint64T);
						tstkPtr++;
					}
					break;

				case BFS_FLEXBUF_I64: // int64_t type
					get_random_data((char *)&int64T, sizeof(int64_t));
					if (doheader) {
						headerStack[hstkPtr].un.int64T = int64T;
						headerStack[hstkPtr].type = BFS_FLEXBUF_I64;
						*buffer << headerStack[hstkPtr].un.int64T;
						logMessage(LOG_INFO_LEVEL,
								   "Adding int64 header, val=%ld", int64T);
						hstkPtr++;
					} else {
						trailerStack[tstkPtr].un.int64T = int64T;
						trailerStack[tstkPtr].type = BFS_FLEXBUF_I64;
						buffer->addTrailer(trailerStack[tstkPtr].un.int64T);
						logMessage(LOG_INFO_LEVEL,
								   "Adding int64 trailer, val=%ld", int64T);
						tstkPtr++;
					}
					break;

				case BFS_FLEXBUF_DATA: // General bytes data type
					// Setup some general purpose value data
					blkptr = (doheader) ? headerStack[hstkPtr].un.blk.block
										: trailerStack[tstkPtr].un.blk.block;
					szptr = ((doheader) ? &headerStack[hstkPtr].un.blk.size
										: &trailerStack[tstkPtr].un.blk.size);
					*szptr = get_random_value(1, BFS_FLEX_UTEST_BASESZ);
					get_random_data(blkptr, *szptr);

					// Now add headers or trailers
					if (doheader) {
						logMessage(LOG_INFO_LEVEL, "Adding data header, len=%d",
								   *szptr);
						headerStack[hstkPtr].type = BFS_FLEXBUF_DATA;
						buffer->addHeader(blkptr, *szptr);
						hstkPtr++;
					} else {
						logMessage(LOG_INFO_LEVEL,
								   "Adding data trailer, len=%d", *szptr);
						trailerStack[tstkPtr].type = BFS_FLEXBUF_DATA;
						buffer->addTrailer(blkptr, *szptr);
						tstkPtr++;
					}
					break;

				default: // The deafult case (should be unreachable)
					logMessage(LOG_ERROR_LEVEL,
							   "Uknown header/trailer type, failed [%d]",
							   ntype);
					return (-1);
				}

			} else {

				// Check the type of the element we are removing
				ntype = (doheader) ? headerStack[hstkPtr - 1].type
								   : trailerStack[tstkPtr - 1].type;
				switch (ntype) {

				case BFS_FLEXBUF_BYTE: // Byte type
					// Remove the header/trailer
					if (doheader) {
						*buffer >> byteT;
						match = (byteT == headerStack[hstkPtr - 1].un.byteT);
						hstkPtr--;
					} else {
						buffer->removeTrailer(byteT);
						match = (byteT == trailerStack[tstkPtr - 1].un.byteT);
						tstkPtr--;
					}

					// Now check to see if the header/trailer matched
					if (match) {
						logMessage(LOG_INFO_LEVEL,
								   "Correctly removed byte %s, val %u",
								   (doheader) ? "header" : "trailer", byteT);
					} else {
						logMessage(LOG_ERROR_LEVEL,
								   "Failed removing byte %s, val %u != %u",
								   (doheader) ? "header" : "trailer", byteT,
								   (doheader) ? headerStack[hstkPtr].un.byteT
											  : trailerStack[tstkPtr].un.byteT);
						return (-1);
					}
					break;

				case BFS_FLEXBUF_BOOL: // Boolean type
					// Remove the header/trailer
					if (doheader) {
						*buffer >> boolT;
						match = (boolT == headerStack[hstkPtr - 1].un.boolT);
						hstkPtr--;
					} else {
						buffer->removeTrailer(boolT);
						match = (boolT == trailerStack[tstkPtr - 1].un.boolT);
						tstkPtr--;
					}

					// Now check to see if the header/trailer matched
					if (match) {
						logMessage(LOG_INFO_LEVEL,
								   "Correctly removed bool %s, val %d",
								   (doheader) ? "header" : "trailer",
								   (boolT) ? "true" : "false");
					} else {
						logMessage(LOG_ERROR_LEVEL,
								   "Failed removing bool %s, val %u != %u",
								   (doheader) ? "header" : "trailer", boolT,
								   (doheader) ? headerStack[hstkPtr].un.boolT
											  : trailerStack[tstkPtr].un.boolT);
						return (-1);
					}
					break;

				case BFS_FLEXBUF_UI16: // uint16_t type
					// Remove the header/trailer
					if (doheader) {
						*buffer >> uint16T;
						match =
							(uint16T == headerStack[hstkPtr - 1].un.uint16T);
						hstkPtr--;
					} else {
						buffer->removeTrailer(uint16T);
						match =
							(uint16T == trailerStack[tstkPtr - 1].un.uint16T);
						tstkPtr--;
					}

					// Now check to see if the header/trailer matched
					if (match) {
						logMessage(LOG_INFO_LEVEL,
								   "Correctly removed uint16 %s, val %u",
								   (doheader) ? "header" : "trailer", uint16T);
					} else {
						logMessage(LOG_ERROR_LEVEL,
								   "Failed removing uint16 %s, val %u != %u",
								   (doheader) ? "header" : "trailer", uint16T,
								   (doheader)
									   ? headerStack[hstkPtr].un.uint16T
									   : trailerStack[tstkPtr].un.uint16T);
						return (-1);
					}
					break;

				case BFS_FLEXBUF_I16: // int16_t type
					// Remove the header/trailer
					if (doheader) {
						*buffer >> int16T;
						match = (int16T == headerStack[hstkPtr - 1].un.int16T);
						hstkPtr--;
					} else {
						buffer->removeTrailer(int16T);
						match = (int16T == trailerStack[tstkPtr - 1].un.int16T);
						tstkPtr--;
					}

					// Now check to see if the header/trailer matched
					if (match) {
						logMessage(LOG_INFO_LEVEL,
								   "Correctly removed int16 %s, val %d",
								   (doheader) ? "header" : "trailer", int16T);
					} else {
						logMessage(LOG_ERROR_LEVEL,
								   "Failed removing int16 %s, val %d != %d",
								   (doheader) ? "header" : "trailer", int16T,
								   (doheader)
									   ? headerStack[hstkPtr].un.int16T
									   : trailerStack[tstkPtr].un.int16T);
						return (-1);
					}
					break;

				case BFS_FLEXBUF_UI32: // uint32_t type
					// Remove the header/trailer
					if (doheader) {
						*buffer >> uint32T;
						match =
							(uint32T == headerStack[hstkPtr - 1].un.uint32T);
						hstkPtr--;
					} else {
						buffer->removeTrailer(uint32T);
						match =
							(uint32T == trailerStack[tstkPtr - 1].un.uint32T);
						tstkPtr--;
					}

					// Now check to see if the header/trailer matched
					if (match) {
						logMessage(LOG_INFO_LEVEL,
								   "Correctly removed uint32 %s, val %u",
								   (doheader) ? "header" : "trailer", uint32T);
					} else {
						logMessage(LOG_ERROR_LEVEL,
								   "Failed removing uint32 %s, val %u != %u",
								   (doheader) ? "header" : "trailer", uint32T,
								   (doheader)
									   ? headerStack[hstkPtr].un.uint32T
									   : trailerStack[tstkPtr].un.uint32T);
						return (-1);
					}
					break;

				case BFS_FLEXBUF_I32: // int32_t type
					// Remove the header/trailer
					if (doheader) {
						*buffer >> int32T;
						match = (int32T == headerStack[hstkPtr - 1].un.int32T);
						hstkPtr--;
					} else {
						buffer->removeTrailer(int32T);
						match = (int32T == trailerStack[tstkPtr - 1].un.int32T);
						tstkPtr--;
					}

					// Now check to see if the header/trailer matched
					if (match) {
						logMessage(LOG_INFO_LEVEL,
								   "Correctly removed int32 %s, val %d",
								   (doheader) ? "header" : "trailer", int32T);
					} else {
						logMessage(LOG_ERROR_LEVEL,
								   "Failed removing int32 %s, val %d != %d",
								   (doheader) ? "header" : "trailer", int32T,
								   (doheader)
									   ? headerStack[hstkPtr].un.int32T
									   : trailerStack[tstkPtr].un.int32T);
						return (-1);
					}
					break;

				case BFS_FLEXBUF_UI64: // uint64_t type
					// Remove the header/trailer
					if (doheader) {
						*buffer >> uint64T;
						match =
							(uint64T == headerStack[hstkPtr - 1].un.uint64T);
						hstkPtr--;
					} else {
						buffer->removeTrailer(uint64T);
						match =
							(uint64T == trailerStack[tstkPtr - 1].un.uint64T);
						tstkPtr--;
					}

					// Now check to see if the header/trailer matched
					if (match) {
						logMessage(LOG_INFO_LEVEL,
								   "Correctly removed uint64 %s, val %lu",
								   (doheader) ? "header" : "trailer", uint64T);
					} else {
						logMessage(LOG_ERROR_LEVEL,
								   "Failed removing uint64 %s, val %lu != %lu",
								   (doheader) ? "header" : "trailer", uint64T,
								   (doheader)
									   ? headerStack[hstkPtr].un.uint64T
									   : trailerStack[tstkPtr].un.uint64T);
						return (-1);
					}
					break;

				case BFS_FLEXBUF_I64: // int64_t type
					// Remove the header/trailer
					if (doheader) {
						*buffer >> int64T;
						match = (int64T == headerStack[hstkPtr - 1].un.int64T);
						hstkPtr--;
					} else {
						buffer->removeTrailer(int64T);
						match = (int64T == trailerStack[tstkPtr - 1].un.int64T);
						tstkPtr--;
					}

					// Now check to see if the header/trailer matched
					if (match) {
						logMessage(LOG_INFO_LEVEL,
								   "Correctly removed int64 %s, val %ld",
								   (doheader) ? "header" : "trailer", int64T);
					} else {
						logMessage(LOG_ERROR_LEVEL,
								   "Failed removing int64 %s, val %ld != %ld",
								   (doheader) ? "header" : "trailer", int64T,
								   (doheader)
									   ? headerStack[hstkPtr].un.int64T
									   : trailerStack[tstkPtr].un.int64T);
						return (-1);
					}
					break;

				case BFS_FLEXBUF_DATA: // General bytes data type
					// Log and remove the header
					sz = ((doheader) ? headerStack[hstkPtr - 1].un.blk.size
									 : trailerStack[tstkPtr - 1].un.blk.size);
					logMessage(LOG_INFO_LEVEL,
							   "Removing data header of length [%u]", sz);
					if (doheader) {
						buffer->removeHeader(newblk, sz);
						hstkPtr--;
					} else {
						buffer->removeTrailer(newblk, sz);
						tstkPtr--;
					}

					// Check the header, make sure it is exactly as added
					if (memcmp(newblk,
							   (doheader) ? headerStack[hstkPtr].un.blk.block
										  : trailerStack[tstkPtr].un.blk.block,
							   sz) == 0) {
						logMessage(LOG_INFO_LEVEL,
								   "Correctly removed %s data buffer, size %u",
								   (doheader) ? "header" : "trailer", sz);
					} else {
						logMessage(LOG_ERROR_LEVEL,
								   "Bad %s match in flexible buffer unit test, "
								   "aborting",
								   (doheader) ? "header" : "trailer");
						if (buffer != NULL)
							delete[] buffer;
						return (-1);
					}
					break;

				default: // The deafult case (should be unreachable)
					logMessage(LOG_ERROR_LEVEL,
							   "Uknown header/trailer type, failed [%u]",
							   ntype);
					return (-1);
				}
			}

			// Print the stack states
			message = "Header state (ptr" + to_string(hstkPtr) + ") ";
			for (j = 0; j < hstkPtr; j++) {
				message += "[" + to_string(j) + ":" +
						   getDataTypeString(headerStack[j].type) + "]";
			}
			logMessage(LOG_INFO_LEVEL, message.c_str());
			message = "Trailer state (ptr" + to_string(tstkPtr) + ") ";
			for (j = 0; j < tstkPtr; j++) {
				message += "[" + to_string(j) + ":" +
						   getDataTypeString(trailerStack[j].type) + "]";
			}
			logMessage(LOG_INFO_LEVEL, message.c_str());

			// Check if we are done
			iter++;
			completed = (iter >= BFS_FLEX_HDTR_UTEST_ITERATIONS) &&
						(hstkPtr == 0) && (tstkPtr == 0);
		}
	}

	// Log, return successfully
	logMessage(LOG_INFO_LEVEL, "Successfully completed header/trailer test.");
	return (0);
}
#endif

//
// Private Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsFlexibleBuffer::resizeAllocation
// Description  : Resize the pre-allocation of the buffer (only increases in
// size)
//
// Inputs       : minhd - the minimum header lebgth
//                newlen - the new data length
//                mintl - the minimum trailer length
// Outputs      : the new length of the buffer

bfs_size_t bfsFlexibleBuffer::resizeAllocation(bfs_size_t minhd,
											   bfs_size_t newlen,
											   bfs_size_t mintl) {

	// Local variables
	char *newbuf;
	bfs_size_t newsz = minhd + newlen + mintl;
	bfs_size_t left, new_hlength, new_tlength;

	// Check if we need to bother
	if ((minhd <= hlength) && (newlen == length) && (mintl <= tlength)) {
		return (allocation);
	}

	// Special case of re-allocation - need to "re-center" the data to allow for
	// new sizes
	if (newsz <= allocation) {
		// Figure out new spaces
		left = allocation - newsz;
		new_hlength = minhd + left / 2 + left % 2;
		new_tlength = mintl + left / 2;

		// Re-center, log and return
		// Update the length first so the memmove reflects the allowable size to
		// copy over (otherwise we overflow the stack). Might truncate the
		// original buffer, however, if all data was to be kept, then the
		// arguments to the original resize call were incorrect.
		length = newlen;
		// memmove( &buffer[new_hlength], &buffer[hlength], length );
		hlength = new_hlength;
		tlength = new_tlength;
		logMessage(UTIL_VRBLOG_LEVEL,
				   "Re-locating flex buffer, size (%u,%u,%u)", hlength, length,
				   tlength);
		return (allocation);
	}
	// else if ( newsz == allocation ) {
	// 	length = newlen;
	// 	hlength = minhd;
	// 	tlength = mintl;
	// 	logMessage( UTIL_VRBLOG_LEVEL, "Re-locating flex buffer, size
	// (%u,%u,%u)", hlength, length, tlength ); 	return( allocation );
	// }

	// Sanity check the re-allocation
	// if ( (length > 0) && (newlen != length) ) {
	// if ( (newlen != length) ) { // length was expected to be 0 from burn, but
	// we removed the call 	throw new bfsUtilError( "Flexible buffer
	// re-allocation sized data" );
	// }

	// Do the realloc
	logMessage(UTIL_VRBLOG_LEVEL,
			   "Reallocating flex buffer, size from %d to %d (%u,%u,%u)",
			   allocation, newsz, minhd, newlen, mintl);

	newbuf = do_alloc(newsz);

	memset(newbuf, 0x0, newsz);

	// Copy the data over (while sanity checking)
	if (length > 0) {
		if (buffer == NULL) {
			throw new bfsUtilError("Flex buffer length>0, NULL buffer");
		}
		memcpy(&newbuf[minhd], &buffer[hlength], length);
	}

	// Now cleanup the existing buffer
	if (buffer != NULL) {
		if (burnOnFree == true)
			memset(buffer, 0x0, allocation);
		do_del_alloc();
	}

	// Switch over to the new buffer
	allocation = newsz;
	hlength = minhd;
	length = newlen;
	tlength = mintl;
	buffer = newbuf;

	// Return succesfully (new length)
	return (length);
}

/**
 * @brief Allocate a new buffer. If this is invoked from enclave code (sometimes
 * the enclave will request buffers in untrusted memory), the target buffer
 * should be allocated in untrusted memory using an ocall, otherwise if it is
 * called from nonenclave code it will be allocated from untrusted memory.
 *
 * @param sz: size of allocation
 * @return char*: pointer to the new buffer
 */
char *bfsFlexibleBuffer::do_alloc(bfs_size_t sz) {
	char *_newbuf = NULL;

#ifdef __BFS_ENCLAVE_MODE
	if ((ocall_do_alloc((long *)&_newbuf, sz) != SGX_SUCCESS) ||
		(_newbuf == (long)NULL))
		throw new bfsUtilError("Failed ocall_do_alloc");

	if (sgx_is_outside_enclave(_newbuf, sz) != 1)
		throw new bfsUtilError(
			"Failed bfsFlexibleBuffer normal alloc: addr range is "
			"not entirely outside enclave "
			"(may be corrupt source ptr)");
#else
	_newbuf = new char[sz];
#endif

	return _newbuf;
}

/**
 * @brief Deletes a buffer in untrusted memory. If called from enclave code,
 * requires an ocall.
 */
void bfsFlexibleBuffer::do_del_alloc() {
#ifdef __BFS_ENCLAVE_MODE
	if (sgx_is_outside_enclave(buffer, allocation) != 1)
		throw new bfsUtilError(
			"Failed bfsFlexibleBuffer del alloc: addr range is "
			"not entirely outside enclave "
			"(may be corrupt source ptr)");

	if ((ocall_delete_allocation((long)buffer) != SGX_SUCCESS))
		throw new bfsUtilError("Failed ocall_delete_allocation");
#else
	delete[] buffer;
#endif
}

/**
 * @brief Allocate a new buffer in secure memory only.
 *
 * @param sz: size of allocation
 * @return char*: pointer to the new buffer
 */
char *bfsSecureFlexibleBuffer::do_alloc(bfs_size_t sz) {
	char *b = new char[sz];

#ifdef __BFS_ENCLAVE_MODE
	if (sgx_is_within_enclave(b, sz) != 1)
		throw new bfsUtilError("Failed do_alloc: addr is not inside enclave");
#endif

	return b;
}

void bfsSecureFlexibleBuffer::do_del_alloc() {
#ifdef __BFS_ENCLAVE_MODE
	if (sgx_is_within_enclave(buffer, allocation) != 1)
		throw new bfsUtilError(
			"Failed do_del_alloc: addr is not inside enclave");

	delete[] buffer;
#endif
}
