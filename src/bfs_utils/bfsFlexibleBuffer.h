#ifndef BFSFLEXIBLEBUFFER_INCLUDED
#define BFSFLEXIBLEBUFFER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsFlexibleBuffer.h
//  Description   : This file contains the definitions for the flexible buffer,
//                  a buffer that preallocates space for the header and tailer
//                  data that is used for things like network protocols and
//                  cryptographic functions.
//
//   Author       : Patrick McDaniel
//   Created      : Thu 22 Apr 2021 04:35:49 PM EDT
//

// Include files
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Project incluides
#include <bfsUtilError.h>
#include <bfsUtilLayer.h>
#include <bfs_common.h>

// C++/STL Isms
#include <string>
using namespace std;

// Definitions
#define BFSFLEXBUF_DEFAULT_PAD 32
#define BFSFLEXBUF_DEFAULT_ALLOC 1024
#define BFSFLEXBUF_DEFAULT_BURN true

// Types

// Header data types
typedef enum {
	BFS_FLEXBUF_BYTE = 0, // Byte type
	BFS_FLEXBUF_BOOL = 1, // Boolean type
	BFS_FLEXBUF_UI16 = 2, // uint16_t type
	BFS_FLEXBUF_I16 = 3,  // int16_t type
	BFS_FLEXBUF_UI32 = 4, // uint32_t type
	BFS_FLEXBUF_I32 = 5,  // int32_t type
	BFS_FLEXBUF_UI64 = 6, // uint64_t type
	BFS_FLEXBUF_I64 = 7,  // int64_t type
	BFS_FLEXBUF_DATA = 8, // General bytes data type
} bfs_flexbuf_dtypes_t;

//
// Flexible Buffer Class Definition

class bfsFlexibleBuffer {

public:
	//
	// Constructors and destructors

	bfsFlexibleBuffer(void);
	// Default constructor

	bfsFlexibleBuffer(char *dat, bfs_size_t len,
					  bfs_size_t hpadsz = BFSFLEXBUF_DEFAULT_PAD,
					  bfs_size_t tpadsz = BFSFLEXBUF_DEFAULT_PAD);
	// Attribute constructor

	// Attribute constructor (unsigned char version)
	bfsFlexibleBuffer(unsigned char *dat, bfs_size_t len)
		: bfsFlexibleBuffer((char *)dat, len) {}

	bfsFlexibleBuffer(const bfsFlexibleBuffer &cpy);
	// Copy constructor

	virtual ~bfsFlexibleBuffer();
	// Destructor

	//
	// Access Methods

	// Get the buffer to flexible buffer
	char *getBuffer(void) const { return (&buffer[hlength]); }

	char *getFullBuffer(void) const { return (buffer); }

	// Get the length of the flexible buffer
	bfs_size_t getLength(void) const { return (length); }

	bfs_size_t getHLength(void) const { return (hlength); }

	bfs_size_t getTLength(void) const { return (tlength); }

	bfs_size_t getAllocation(void) const { return (allocation); }

	bool getBurnOnFree(void) const { return burnOnFree; }

	// Set the buffer to burn memory on release
	void setBurnOnFree(bool bof) { burnOnFree = bof; }

	//
	// Class Methods

	// Burn the buffer (clear contents)
	void burn(void) {
		memset(buffer, 0x0, allocation);

		// QB: dont resize on burn
		// tlength = allocation/2;
		// hlength = allocation - tlength;
		// length = 0;
		return;
	}

	virtual bfs_size_t setData(const char *dat, bfs_size_t len);
	// Set the data in the buffer (discards previous data)

	bfs_size_t resetWithAlloc(bfs_size_t sz, char fill = 0x0,
							  bfs_size_t hpadsz = BFSFLEXBUF_DEFAULT_PAD,
							  bfs_size_t tpadsz = BFSFLEXBUF_DEFAULT_PAD,
							  bool burn_on_reset = false);
	// Reset the buffer, preallocating buffer with data of specific length and
	// fill

	string toString(int maxdigits = -1);
	// Create a human readable string describing the buffer

	// Adding Headers

	bfs_size_t addHeader(char *dat, bfs_size_t len);
	// Add a header to the flexible buffer (prepend)

	// byte header addition
	bfs_size_t addHeader(char &val) {
		return (addHeader((char *)&val, sizeof(char)));
	}

	// boolean header addition
	bfs_size_t addHeader(bool &val) {
		return (addHeader((char *)&val, sizeof(bool)));
	}

	// uint16_t header addition
	bfs_size_t addHeader(uint16_t &val) {
		return (addHeader((char *)&val, sizeof(uint16_t)));
	}
	// int16_t header addition
	bfs_size_t addHeader(int16_t &val) {
		return (addHeader((char *)&val, sizeof(int16_t)));
	}

	// uint32_t header addition
	bfs_size_t addHeader(uint32_t &val) {
		return (addHeader((char *)&val, sizeof(uint32_t)));
	}
	// int32_t header addition
	bfs_size_t addHeader(int32_t &val) {
		return (addHeader((char *)&val, sizeof(int32_t)));
	}

	// uint64_t header addition
	bfs_size_t addHeader(uint64_t &val) {
		return (addHeader((char *)&val, sizeof(uint64_t)));
	}
	// int64_t header addition
	bfs_size_t addHeader(int64_t &val) {
		return (addHeader((char *)&val, sizeof(int64_t)));
	}

	// Removing Headers

	bfs_size_t removeHeader(char *dat, bfs_size_t len);
	// Remove the header from the buffer

	// byte header addition
	bfs_size_t removeHeader(char &val) {
		return (removeHeader((char *)&val, sizeof(char)));
	}

	// boolean header addition
	bfs_size_t removeHeader(bool &val) {
		return (removeHeader((char *)&val, sizeof(bool)));
	}

	// uint16_t header addition
	bfs_size_t removeHeader(uint16_t &val) {
		return (removeHeader((char *)&val, sizeof(uint16_t)));
	}
	// int16_t header addition
	bfs_size_t removeHeader(int16_t &val) {
		return (removeHeader((char *)&val, sizeof(int16_t)));
	}

	// uint32_t header addition
	bfs_size_t removeHeader(uint32_t &val) {
		return (removeHeader((char *)&val, sizeof(uint32_t)));
	}
	// int32_t header addition
	bfs_size_t removeHeader(int32_t &val) {
		return (removeHeader((char *)&val, sizeof(int32_t)));
	}

	// uint64_t header addition
	bfs_size_t removeHeader(uint64_t &val) {
		return (removeHeader((char *)&val, sizeof(uint64_t)));
	}
	// int64_t header addition
	bfs_size_t removeHeader(int64_t &val) {
		return (removeHeader((char *)&val, sizeof(int64_t)));
	}

	// Adding Trailers

	bfs_size_t addTrailer(char *dat, bfs_size_t len);
	// Add a trailer to the flexible buffer (append)

	// byte trailer addition
	bfs_size_t addTrailer(char &val) {
		return (addTrailer((char *)&val, sizeof(char)));
	}

	// boolean trailer addition
	bfs_size_t addTrailer(bool &val) {
		return (addTrailer((char *)&val, sizeof(bool)));
	}

	// uint16_t trailer addition
	bfs_size_t addTrailer(uint16_t &val) {
		return (addTrailer((char *)&val, sizeof(uint16_t)));
	}
	// int16_t trailer addition
	bfs_size_t addTrailer(int16_t &val) {
		return (addTrailer((char *)&val, sizeof(int16_t)));
	}

	// uint32_t trailer addition
	bfs_size_t addTrailer(uint32_t &val) {
		return (addTrailer((char *)&val, sizeof(uint32_t)));
	}
	// int32_t trailer addition
	bfs_size_t addTrailer(int32_t &val) {
		return (addTrailer((char *)&val, sizeof(int32_t)));
	}

	// uint64_t trailer addition
	bfs_size_t addTrailer(uint64_t &val) {
		return (addTrailer((char *)&val, sizeof(uint64_t)));
	}
	// int64_t trailer addition
	bfs_size_t addTrailer(int64_t &val) {
		return (addTrailer((char *)&val, sizeof(int64_t)));
	}

	// Removing Trailers

	bfs_size_t removeTrailer(char *const dat, bfs_size_t len);
	// Add a trailer to the flexible buffer (append)

	// byte trailer addition
	bfs_size_t removeTrailer(char &val) {
		return (removeTrailer((char *)&val, sizeof(char)));
	}

	// boolean trailer addition
	bfs_size_t removeTrailer(bool &val) {
		return (removeTrailer((char *)&val, sizeof(bool)));
	}

	// uint16_t trailer addition
	bfs_size_t removeTrailer(uint16_t &val) {
		return (removeTrailer((char *)&val, sizeof(uint16_t)));
	}
	// int16_t trailer addition
	bfs_size_t removeTrailer(int16_t &val) {
		return (removeTrailer((char *)&val, sizeof(int16_t)));
	}

	// uint32_t trailer addition
	bfs_size_t removeTrailer(uint32_t &val) {
		return (removeTrailer((char *)&val, sizeof(uint32_t)));
	}
	// int32_t trailer addition
	bfs_size_t removeTrailer(int32_t &val) {
		return (removeTrailer((char *)&val, sizeof(int32_t)));
	}

	// uint64_t trailer addition
	bfs_size_t removeTrailer(uint64_t &val) {
		return (removeTrailer((char *)&val, sizeof(uint64_t)));
	}
	// int64_t trailer addition
	bfs_size_t removeTrailer(int64_t &val) {
		return (removeTrailer((char *)&val, sizeof(int64_t)));
	}

	//
	// Operator Methods

	const bfsFlexibleBuffer &operator=(const bfsFlexibleBuffer &fb);
	// The copy operators

	// The equals operator
	bool operator==(const bfsFlexibleBuffer &b) const {
		return ((length == b.length) &&
				(memcmp(&buffer[hlength], b.getBuffer(), length) == 0));
	}

	// The equals operator
	bool operator!=(const bfsFlexibleBuffer &b) const {
		return (!(operator==(b)));
	}

	// byte header add (streaming)
	bfsFlexibleBuffer &operator<<(char &val) {
		addHeader(val);
		return (*this);
	}

	// byte header remove (streaming)
	bfsFlexibleBuffer &operator>>(char &val) {
		removeHeader(&val, sizeof(char));
		return (*this);
	}

	// bool header add (streaming)
	bfsFlexibleBuffer &operator<<(bool &val) {
		addHeader(val);
		return (*this);
	}

	// byte header remove (streaming)
	bfsFlexibleBuffer &operator>>(bool &val) {
		removeHeader((char *)&val, sizeof(bool));
		return (*this);
	}

	// uint16_t header add (streaming)
	bfsFlexibleBuffer &operator<<(uint16_t &val) {
		addHeader(val);
		return (*this);
	}

	// uint16_t header remove (streaming)
	bfsFlexibleBuffer &operator>>(uint16_t &val) {
		removeHeader((char *)&val, sizeof(uint16_t));
		return (*this);
	}

	// int16_t header add (streaming)
	bfsFlexibleBuffer &operator<<(int16_t &val) {
		addHeader(val);
		return (*this);
	}

	// int16_t header remove (streaming)
	bfsFlexibleBuffer &operator>>(int16_t &val) {
		removeHeader((char *)&val, sizeof(int16_t));
		return (*this);
	}

	// uint32_t header add (streaming)
	bfsFlexibleBuffer &operator<<(uint32_t &val) {
		addHeader(val);
		return (*this);
	}

	// uint32_t header remove (streaming)
	bfsFlexibleBuffer &operator>>(uint32_t &val) {
		removeHeader((char *)&val, sizeof(uint32_t));
		return (*this);
	}

	// int32_t header add (streaming)
	bfsFlexibleBuffer &operator<<(int32_t &val) {
		addHeader(val);
		return (*this);
	}

	// int32_t header remove (streaming)
	bfsFlexibleBuffer &operator>>(int32_t &val) {
		removeHeader((char *)&val, sizeof(int32_t));
		return (*this);
	}

	// uint64_t header add (streaming)
	bfsFlexibleBuffer &operator<<(uint64_t &val) {
		addHeader(val);
		return (*this);
	}

	// uint64_t header remove (streaming)
	bfsFlexibleBuffer &operator>>(uint64_t &val) {
		removeHeader((char *)&val, sizeof(uint64_t));
		return (*this);
	}

	// int64_t header add (streaming)
	bfsFlexibleBuffer &operator<<(int64_t &val) {
		addHeader(val);
		return (*this);
	}

	// int64_t header remove (streaming)
	bfsFlexibleBuffer &operator>>(int64_t &val) {
		removeHeader((char *)&val, sizeof(int64_t));
		return (*this);
	}

	//
	// Static methods

	static int flexBufferUTest(void);
	// Test the flexible buffer implementation

	static const char *getDataTypeString(bfs_flexbuf_dtypes_t ty) {
		if ((ty < 0) || (ty > BFS_FLEXBUF_DATA)) {
			return ("BAD DATA TYPE");
		}
		return (bfs_flexbuf_dtypes_strings[ty]);
	}

	bfs_size_t resizeAllocation(bfs_size_t minhd, bfs_size_t newlen,
								bfs_size_t mintl);
	// Resize the pre-allocation of the buffer (only increases in size). Make
	// available for efficiently reusing buffers.

protected:
	//
	// Private Methods

	virtual char *do_alloc(bfs_size_t);
	virtual void do_del_alloc();
	// Helpers for allocation and deletion

	//
	// Class Data

	char *buffer;
	// The buffer containing the allocated memory

	bfs_size_t allocation;
	// The allocation (in bytes) of the flexible buffer

	bfs_size_t hlength, length, tlength;
	// Length of the held data (tail - head, for conveinence)

	bool burnOnFree;
	// Flag indicate whether we burn the buffer on free

	//
	// Static class data

	static const char *bfs_flexbuf_dtypes_strings[];
	// Descriptive strings for the flexible buffer data types
};

class bfsSecureFlexibleBuffer : public bfsFlexibleBuffer {

public:
	// Default constructor
	bfsSecureFlexibleBuffer() : bfsFlexibleBuffer() {}

	// Copy constructor
	bfsSecureFlexibleBuffer(const bfsFlexibleBuffer &cpy);

	bfsSecureFlexibleBuffer(char *dat, uint32_t len,
							bfs_size_t hpadsz = BFSFLEXBUF_DEFAULT_PAD,
							bfs_size_t tpadsz = BFSFLEXBUF_DEFAULT_PAD);

	~bfsSecureFlexibleBuffer() override;

protected:
	// Helpers for allocation and deletion
	char *do_alloc(bfs_size_t) override;
	void do_del_alloc() override;
};

#endif
