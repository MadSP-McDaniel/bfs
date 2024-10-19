#ifndef BFS_SEC_ASSOC_INCLUDED
#define BFS_SEC_ASSOC_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsSecAssociation.h
//  Description   : This file contains the definition of the security
//  association
//                  that is used to secure communication between endpoints in
//                  the bfs filesystem.  Note that this is a simplex connection
//                  with an initiator (sender) and a responder (recipient).
//
//   Author       : Patrick McDaniel
//   Created      : Thu 06 May 2021 05:05:31 PM EDT
//

// Include files

// Project incluides
#include <bfsCfgStore.h>
#include <bfsCryptoKey.h>
#include <bfsCryptoLayer.h>
#include <bfsFlexibleBuffer.h>

// C++/STL Isms
#include <string>
using namespace std;

// Definitions

// Types

//
// bfsSecAssociation Class Definition

class bfsSecAssociation {

public:
	//
	// Constructors and destructors

	// Default constructor
	bfsSecAssociation(void) : saKey(NULL) {}

	bfsSecAssociation(string in, string resp, bfsCryptoKey *key = NULL);
	// Attribute constructor

	bfsSecAssociation(bfsCfgItem *config, bool secure_buf = false);
	// Configuration constructor

	~bfsSecAssociation();
	// Destructor

	//
	// Access Methods

	// Get the initiator side indentiy
	const string &getIntiator(void) { return (initiator); }

	// Get the initiator side indentiy
	const string &getResponder(void) { return (responder); }

	// Access the key associated with this association
	bfsCryptoKey *getKey(void) { return (saKey); }

	//
	// Class Methods

	int setKey(bfsCryptoKey *key);
	// Set the key for the security association

	int encryptData(bfsFlexibleBuffer &buf, bfsFlexibleBuffer *aad = NULL,
					bool mac = false);
	// In-place encryption of data

	int encryptData2(bfsFlexibleBuffer &buf, bfsFlexibleBuffer *aad,
					 uint8_t **iv = NULL, uint8_t **mac = NULL);

	int encryptData(bfsFlexibleBuffer &buf, bfsFlexibleBuffer &out,
					bfsFlexibleBuffer *aad = NULL, bool mac = false);
	// Output buffer encryption of data (out = E(buf))

	int decryptData(bfsFlexibleBuffer &buf, bfsFlexibleBuffer *aad = NULL,
					bool mac = false, uint8_t *const mac_out = NULL);
	// In-place decryption of data

	int decryptData2(bfsFlexibleBuffer &buf, bfsFlexibleBuffer *aad,
					 uint8_t *iv = NULL, uint8_t *mac = NULL);

	int decryptData(bfsFlexibleBuffer &buf, bfsFlexibleBuffer &out,
					bfsFlexibleBuffer *aad = NULL, bool mac = false,
					uint8_t *const mac_out = NULL);
	// Output buffer decryption of data (out = D(buf))

	int hmacData(uint8_t *out, uint8_t *left, uint8_t *right, int len);

	int macData(bfsFlexibleBuffer &buf);
	// Add a MAC onto an buffer (needs IV)

	// void extractMac(bfsFlexibleBuffer &, unsigned char **, unsigned char **,
	// 				size_t *);

	// bool verifyMac(bfsFlexibleBuffer &buf, bfsFlexibleBuffer *aad = NULL);
	// MAC check (removes MAC trailer), returns true if verified

	size_t addPKCS7Padding(bfsFlexibleBuffer &buf);
	// Add the PKCS padding (returns number of pad bytes)

private:
	//
	// Private Methods

	size_t removePKCS7Padding(bfsFlexibleBuffer &buf);
	// Remove the PKCS padding (returns bytes padded)

	friend int bfsCryptoLayer::bfsCryptoLayerUtest(void);
	// A friend function (allows private access in unit test)

	//
	// Class Data

	string initiator;
	// The initiator of the flow of data

	string responder;
	// The responder to the flow of data

	bfsCryptoKey *saKey;
	// The key to be used for all cipher operations

	bfsFlexibleBuffer iv;
	// The IV for each encrypted block

	//
	// Static class data
};

#endif
