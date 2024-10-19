#ifndef BFS_CRYPTO_KEY_INCLUDED
#define BFS_CRYPTO_KEY_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCryptoKey.h
//  Description   : This is the class describing cryptographic key object for
//                  use in the cryptographic implementation of the BFS
//                  filesystem.  The main purpose of this class is to isolate
//                  the crypto functions between SGX and non-SGX code.
//
// SGX documentation:
// https://github.com/intel/linux-sgx/blob/56faf11a455b06fedd6adc3e60b71f6faf05dc0f/common/inc/sgx_tcrypto.h
//
//  Author  : Patrick McDaniel
//  Created : Thu 06 May 2021 05:05:31 PM EDT
//

// Includes

// STL-isms

// Project Includes
#include <bfsCryptoLayer.h>
#include <bfsFlexibleBuffer.h>

//
// Class definitions
typedef uint32_t bfs_keyid_t;

#define CRYPTO_FIRST_KEYID 1000

//
// Class Definition

class bfsCryptoKey {

public:
	//
	// Static methods

	bfsCryptoKey(void);
	// Default constructor

	bfsCryptoKey(const char *key, bfs_size_t len);
	// Attribute constructor

	virtual ~bfsCryptoKey(void);
	// Default destructor

	//
	// Access Methods

	// Get the key identifier
	bfs_keyid_t getKeyId(void) { return (keyid); }

	// Get the cipher blocksize
	bfs_size_t getBlocksize(void) { return (blocksize); }

	// Get the MAC size
	bfs_size_t getMACsize(void) { return (maclen); }

	// Get the HMAC size
	bfs_size_t getHMACsize(void) { return (hmac_len); }

	bfs_size_t getIVlen(void) { return (ivlen); }

#ifndef __BFS_ENCLAVE_MODE
	char *getKeydat(void) { return (keydat); }
#endif

	//
	// Class Methods

	bool setKeyData(const char *key, bfs_size_t len);
	// Set the key and initialize the cipher

	bool destroyCipher(void);
	// Destroy all of the cipher context

	bool encryptData(char *iv, char *out, bfs_size_t olen, char *in,
					 bfs_size_t ilen, char *aad, int aad_len, void *mtag);
	// Encrypt the data using the low level functions

	// In place encryption
	bool encryptData(char *iv, char *buf, bfs_size_t len, char *aad,
					 int aad_len, void *mtag) {
		return (encryptData(iv, buf, len, NULL, 0, aad, aad_len, mtag));
	}

	bool decryptData(char *iv, char *out, bfs_size_t olen, char *in,
					 bfs_size_t ilen, char *aad, int aad_len, void *mtag);
	// Dencrypt the data using the low level functions

	// In place decryption
	bool decryptData(char *iv, char *buf, bfs_size_t len, char *aad,
					 int aad_len, void *mtag) {
		return (decryptData(iv, buf, len, NULL, 0, aad, aad_len, mtag));
	}

	int hmacData(uint8_t *out, bfs_size_t mac_size, uint8_t *left,
				 uint8_t *right, int len);

	bool macData(char *macval, bfs_size_t mlen, char *in, bfs_size_t ilen);
	// MAC a data item

	bool verifyMac(char *macval, bfs_size_t mlen, char *in, bfs_size_t ilen);
	// Validate the MAC value

	string toBase64(void);
	// Convert key into base64 representation

	//
	// Static Class Methods

	// Get the default key size
	static bfs_size_t getDefaultKeySize(void) {
#ifdef __BFS_ENCLAVE_MODE
		return SGX_AESGCM_KEY_SIZE;
#else
		return (bfs_size_t)(
			gcry_cipher_get_algo_keylen(BFS_CRYPTO_DEFAULT_CIPHER));
#endif
	}

	// Get the default block size
	static bfs_size_t getDefaultBlockSize(void) {
#ifdef __BFS_ENCLAVE_MODE
		return BFS_CRYPTO_DEFAULT_BLK_SZ;
#else
		return (bfs_size_t)(
			gcry_cipher_get_algo_blklen(BFS_CRYPTO_DEFAULT_CIPHER));
#endif
	}

	static bfsCryptoKey *createRandomKey(void);
	// Create a random key with defaults

private:
	//
	// Private class methods

	bool doMAC(char *macval, bfs_size_t mlen, char *in, bfs_size_t ilen,
			   bool verify);
	// Perform a MAC (for creation or validation, see "verify" param)

	//
	// Class Data

	bfs_keyid_t keyid;
	// The unique identifier for this key (class assigned)

	bool cipherInitialized;
	// Flag indicating whether this cipher has been initialized.

	bfs_size_t blocksize;
	// The block size of the cipher

	bfs_size_t maclen;
	// The length of the MAC

	bfs_size_t hmac_len;
	// The length of HMACs

	bfs_size_t ivlen;
	// The length of the IV

#ifndef __BFS_ENCLAVE_MODE
	char *keydat;
	// The actual saved bits of the key

	bfs_size_t keylen;
	// The length of the key (should match selected cipher)

	gcry_cipher_hd_t cipher;
	// This is the handle into the cipher itself
	gcry_md_hd_t hmac_h;
	// this is a handle for the hmac context (merkle tree)

	// gcry_mac_hd_t mac;
	// The handle for the MAC processor
#else
	// sgx_hmac_state_handle_t hmac_hh;
	// this is a handle for the hmac context (merkle tree)

	char *cipher_keydat;
	// The actual saved bits of the cipher key

	bfs_size_t cipher_keylen;
	// The length of the cipher key (should match selected cipher)
#endif

	//
	// Static Member Data

	static bfs_keyid_t nextAvailableKeyID;
	// A unique key identifer for the key
};

#endif
