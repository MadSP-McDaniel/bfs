////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCryptoKey.cpp
//  Description   : This is the class implementing the crypto layer interface
//                  for the bfs file system.  Note that this is static class
//                  in which there are no objects.
//
//  Author  : Patrick McDaniel
//  Created : Thu 06 May 2021 05:05:31 PM EDT
//

// Include files

// Project include files
#include <bfsCryptoError.h>
#include <bfsCryptoKey.h>
#include <bfsCryptoLayer.h>
#include <bfs_base64.h>
#include <bfs_log.h>
#include <bfs_util.h>

//
// Class Data

bfs_keyid_t bfsCryptoKey::nextAvailableKeyID = CRYPTO_FIRST_KEYID;

//
// Class Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCryptoKey::bfsCryptoKey
// Description  : Default constructor
//
// Inputs       : none
// Outputs      : none

bfsCryptoKey::bfsCryptoKey(void)
	: keyid(nextAvailableKeyID++), cipherInitialized(false), blocksize(0),
	  maclen(0), hmac_len(0), ivlen(0) {
#ifndef __BFS_ENCLAVE_MODE
	keydat = NULL;
	keylen = 0;
	memset(&cipher, 0x0, sizeof(gcry_cipher_hd_t));
	memset(&hmac_h, 0x0, sizeof(gcry_cipher_hd_t));
	// memset( &mac, 0x0, sizeof(gcry_mac_hd_t) );
#else
	// hmac_hh = NULL; // void* (not struct) so just init to NULL

	cipher_keydat = NULL;
	cipher_keylen = 0;
#endif

	// Return, no return code
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCryptoKey::bfsCryptoKey
// Description  : Attribute constructor
//
// Inputs       : key - the data to turn into a key
//                len - the length of the data
// Outputs      : none

bfsCryptoKey::bfsCryptoKey(const char *key, bfs_size_t len) : bfsCryptoKey() {

	// Set key and return, no return code
	setKeyData(key, len);
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCryptoKey::~bfsCryptoKey
// Description  : Destructor
//
// Inputs       : none
// Outputs      : none

bfsCryptoKey::~bfsCryptoKey(void) {

	// Return, no return code
	destroyCipher();
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCryptoKey::setKeyData
// Description  : Set the key and initialize the cipher
//
// Inputs       : key - the data associated with the key
//                len - the length of the key data
// Outputs      : true if succesful or throws exception

bool bfsCryptoKey::setKeyData(const char *key, bfs_size_t len) {

	// Local variables
	string message;

	// Cleanup cipher, clone the key data
	destroyCipher();

#ifndef __BFS_ENCLAVE_MODE
	// Setup the cipher and MAC
	gcry_error_t err;

	keydat = new char[len];
	memcpy(keydat, key, len);

	if ((err = gcry_cipher_open(&cipher, BFS_CRYPTO_DEFAULT_CIPHER,
								BFS_CRYPTO_DEFAULT_CIPHER_MODE, 0)) !=
		GPG_ERR_NO_ERROR) {
		message =
			(string) "gcrypt failure setting up cipher: " + gcry_strerror(err);
		throw new bfsCryptoError(message);
	}

	if ((err = gcry_md_open(&hmac_h, GCRY_MD_SHA256, GCRY_MD_FLAG_HMAC)) !=
		GPG_ERR_NO_ERROR) {
		message =
			(string) "gcrypt failure setting up hmac_h: " + gcry_strerror(err);
		throw new bfsCryptoError(message);
	}

	// if ( (err = gcry_mac_open(&mac, BFS_CRYPTO_DEFAULT_MAC, 0, NULL)) !=
	// GPG_ERR_NO_ERROR ) { 	message = (string)"gcrypt failure setting up
	// mac:
	// "
	// + gcry_strerror(err); 	throw new bfsCryptoError( message );
	// }

	// Get the cipher key and block size
	if (((keylen = (bfs_size_t)gcry_cipher_get_algo_keylen(
			  BFS_CRYPTO_DEFAULT_CIPHER)) == 0) ||
		((blocksize = (bfs_size_t)gcry_cipher_get_algo_blklen(
			  BFS_CRYPTO_DEFAULT_CIPHER)) == 0) ||
		((maclen = gcry_mac_get_algo_maclen(GCRY_MAC_GMAC_AES)) == 0)) {
		message = (string) "gcrypt unable to get key/block/mac size";
		throw new bfsCryptoError(message);
	}
	// ivlen = getBlocksize();
	ivlen = BFS_CRYPTO_DEFAULT_IV_LEN;

	// Now set the key for the cipher
	if ((err = gcry_cipher_setkey(cipher, keydat, len)) != GPG_ERR_NO_ERROR) {
		message =
			(string) "gcrypt failure setting cipher key: " + gcry_strerror(err);
		throw new bfsCryptoError(message);
	}
	if ((err = gcry_md_setkey(hmac_h, keydat, keylen)) != GPG_ERR_NO_ERROR) {
		message =
			(string) "gcrypt failure setting hmac_h key: " + gcry_strerror(err);
		throw new bfsCryptoError(message);
	}

	// if ( (err = gcry_mac_setkey(mac, keydat, len)) != GPG_ERR_NO_ERROR ) {
	// 	message = (string)"gcrypt failure setting MAC key: " +
	// gcry_strerror(err); 	throw new bfsCryptoError( message );
	// }
#else
	// TODO
	// Abort for now if key too long, later should just use 16 of the bytes
	// instead since the gcrypt code default key size is 32 but sgx aes gcm only
	// supports 128-bit (16 byte) keys it looks like (see developer reference pg
	// 266/343) enc_init should be used when using same key over multiple
	// messages (otherwise if all data is available, it is easier to just call
	// sgx_rijndael128GCM_encrypt directly)
	// if (len > SGX_AESGCM_KEY_SIZE)
	// 	abort();

	blocksize = BFS_CRYPTO_DEFAULT_BLK_SZ;
	ivlen = SGX_AESGCM_IV_SIZE; // ==BFS_CRYPTO_DEFAULT_IV_LEN

	// Copy both the cipher and mac keys from `key` for now. SGX crypto only
	// supports 128-bit aes gcm keys, but 256-bit sha keys (while gcrypt
	// supports 256-bit keys for both).
	// cipher_keylen = SGX_AESGCM_KEY_SIZE;
	cipher_keylen = len;
	cipher_keydat = new char[cipher_keylen];
	memcpy(cipher_keydat, key, cipher_keylen);

	// mac_keylen = SGX_HMAC256_KEY_SIZE;
	maclen = SGX_AESGCM_MAC_SIZE; // use default mac from encrypt operation
								  // mac_keydat = new char[mac_keylen];
								  // memcpy(mac_keydat, key, mac_keylen);

	// if (sgx_hmac256_init((const unsigned char *)cipher_keydat, cipher_keylen,
	// &hmac_hh) != 	SGX_SUCCESS) { 	logMessage(LOG_ERROR_LEVEL, "Failed
	// sgx_hmac256_init\n"); 	return NULL;
	// }

	// Setup initial context for the cipher and mac handles. In contrast to
	// gcrypt, updates to the handle (eg updating IV) will also go through
	// sgx_aes_gcm128_enc_init, so this might be redundant (leave for now).
	// Edit: skip init, just go straight to encrypt/decrypt methods
	// if (sgx_aes_gcm128_enc_init((const uint8_t *)cipher_keydat, NULL,
	// 							SGX_AESGCM_IV_SIZE, NULL, 0,
	// 							&cipher) != SGX_SUCCESS) {
	// 	message = std::string("tcrypto failure setting up cipher");
	// 	throw new bfsCryptoError(message);
	// }

	// if (sgx_hmac256_init((const unsigned char *)mac_keydat, mac_keylen, &mac)
	// != 	SGX_SUCCESS) { 	message = std::string("tcrypto failure setting up
	// mac"); 	throw new bfsCryptoError(message);
	// }
#endif
	hmac_len = 32; // sha256 for both gcrypt/sgx crypto

	cipherInitialized = true;

	// Return succesfully
	return (true);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCryptoKey::destroyCipher
// Description  : Destroy all of the cipher context
//
// Inputs       : none
// Outputs      : true if succesful or throws exception

bool bfsCryptoKey::destroyCipher(void) {
#ifndef __BFS_ENCLAVE_MODE
	// Close the cipher
	if (cipherInitialized == true) {
		gcry_cipher_close(cipher);
		// gcry_mac_close( mac );
		gcry_md_close(hmac_h);
	}

	// Cleanup the data associated
	if (keydat != NULL) {
		memset(keydat, 0x0, keylen);
		delete[] keydat;
		keydat = NULL;
	}

	keylen = 0;
#else
	std::string err_msg;

	// Close the cipher and mac handles
	if (cipherInitialized == true) {
		// if (sgx_aes_gcm_close(cipher) != SGX_SUCCESS) {
		//     err_msg = std::string("tcrypto failure closing cipher");
		//     throw new bfsCryptoError(err_msg);
		// }

		// if (sgx_hmac256_close(hmac_hh) != SGX_SUCCESS) {
		// 	logMessage(LOG_ERROR_LEVEL, "tcrypto failure closing hmac_hh");
		// 	return BFS_FAILURE;
		// }
	}

	if (cipher_keydat != NULL) {
		memset(cipher_keydat, 0x0, cipher_keylen);
		delete[] cipher_keydat;
		cipher_keydat = NULL;
	}

	cipher_keylen = 0;

	// // Cipher and mac keys are the same but in diff buffers, so need to free
	// both. if ( mac_keydat != NULL ) { 	memset( mac_keydat, 0x0, mac_keylen
	// ); 	delete[] mac_keydat; 	mac_keydat = NULL;
	// }

	// mac_keylen = 0;
#endif
	cipherInitialized = false;

	// Return succesfully
	return (true);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCryptoKey::encryptData
// Description  : Encrypt the data using the low level functions
//
// Inputs       : iv - the initialization vector for the encryption
//                out - the buffer to place descrypted data into
//                olen - the length of the output data
//                in - the buffer data to encrypt
//                ilen - the length of the data to encrypt
//                mtag - mac tag buffer to fill (for sgx crypto)
// Outputs      : true if succesful or throws exception

bool bfsCryptoKey::encryptData(char *iv, char *out, bfs_size_t olen, char *in,
							   bfs_size_t ilen, char *aad, int aad_len,
							   void *mtag) {

	// Local variables
	string message;

	// Check the cipher initialized
	if (!cipherInitialized == true) {
		message = (string) "Attempting to encrypt using uninitialized crypto "
						   "key, abort";
		throw new bfsCryptoError(message);
	}
	logMessage(CRYPTO_VRBLOG_LEVEL, "Encrypting keyid %u, %d bytes", keyid,
			   ilen);

#ifndef __BFS_ENCLAVE_MODE
	(void)mtag;
	gcry_error_t err;

	// Set the passed IV for the encryption algorithm
	if ((err = gcry_cipher_setkey(cipher, keydat, 16)) != GPG_ERR_NO_ERROR) {
		message =
			(string) "gcrypt failure setting cipher key: " + gcry_strerror(err);
		throw new bfsCryptoError(message);
	}

	if ((err = gcry_cipher_setiv(cipher, iv, getIVlen())) != GPG_ERR_NO_ERROR) {
		message =
			(string) "gcrypt failure setting cipher IV: " + gcry_strerror(err);
		throw new bfsCryptoError(message);
	}

	if (gcry_cipher_authenticate(cipher, aad, aad_len) != GPG_ERR_NO_ERROR) {
		message = (string) "gcrypt add MAC AAD failure (encrypt): " +
				  gcry_strerror(err);
		throw new bfsCryptoError(message);
	}

	// Encyrpt the data passed in
	if ((err = gcry_cipher_encrypt(cipher, out, olen, in, ilen)) !=
		GPG_ERR_NO_ERROR) {
		message =
			(string) "gcrypt failure on encrypt data: " + gcry_strerror(err);
		throw new bfsCryptoError(message);
	}
#else
	(void)olen;
	if (sgx_rijndael128GCM_encrypt(
			(sgx_aes_gcm_128bit_key_t *)cipher_keydat, (const uint8_t *)in,
			ilen, (uint8_t *)out, (const uint8_t *)iv, getIVlen(),
			(const uint8_t *)aad, aad_len,
			(sgx_aes_gcm_128bit_tag_t *)mtag) != SGX_SUCCESS) {
		message = std::string("tcrypto failure during encryption");
		throw new bfsCryptoError(message);
	}
#endif

	// Return successfully
	return (true);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCryptoKey::decryptData
// Description  : Decrypt the data using the low level functions
//
// Inputs       : iv - the initialization vector for the decryption
//                out - the buffer to place decrypted data into
//                olen - the length of the output data
//                in - the buffer data to decrypt
//                ilen - the length of the data to decrypt
// Outputs      : true if succesful or throws exception

bool bfsCryptoKey::decryptData(char *iv, char *out, bfs_size_t olen, char *in,
							   bfs_size_t ilen, char *aad, int aad_len,
							   void *mtag) {

	// Local variables
	string message;

	// Check the cipher initialized
	if (!cipherInitialized == true) {
		message = (string) "Attempting to decrypt using uninitialized crypto "
						   "key, abort";
		throw new bfsCryptoError(message);
	}
	logMessage(CRYPTO_VRBLOG_LEVEL, "Decrypting keyid %u, %d bytes", keyid,
			   ilen);

#ifndef __BFS_ENCLAVE_MODE
	(void)mtag;
	gcry_error_t err;

	if ((err = gcry_cipher_setkey(cipher, keydat, 16)) != GPG_ERR_NO_ERROR) {
		message =
			(string) "gcrypt failure setting cipher key: " + gcry_strerror(err);
		throw new bfsCryptoError(message);
	}

	// Set the passed IV for the decryption algorithm
	if ((err = gcry_cipher_setiv(cipher, iv, getIVlen())) != GPG_ERR_NO_ERROR) {
		message =
			(string) "gcrypt failure setting cipher IV: " + gcry_strerror(err);
		throw new bfsCryptoError(message);
	}

	if (gcry_cipher_authenticate(cipher, aad, aad_len) != GPG_ERR_NO_ERROR) {
		message = (string) "gcrypt add MAC AAD failure (encrypt): " +
				  gcry_strerror(err);
		throw new bfsCryptoError(message);
	}

	// Encyrpt the data passed in
	if ((err = gcry_cipher_decrypt(cipher, out, olen, in, ilen)) !=
		GPG_ERR_NO_ERROR) {
		message =
			(string) "gcrypt failure on decrypt data: " + gcry_strerror(err);
		throw new bfsCryptoError(message);
	}
#else
	(void)olen;
	sgx_status_t err;
	if ((err = sgx_rijndael128GCM_decrypt(
			 (sgx_aes_gcm_128bit_key_t *)cipher_keydat, (const uint8_t *)in,
			 ilen, (uint8_t *)out, (const uint8_t *)iv, getIVlen(),
			 (const uint8_t *)aad, aad_len,
			 (const sgx_aes_gcm_128bit_tag_t *)mtag)) != SGX_SUCCESS) {
		message = std::string("tcrypto failure during decryption: " +
							  std::to_string(err));
		throw new bfsCryptoError(message);
	}
#endif

	// Return successfully
	return (true);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCryptoKey::macData
// Description  : MAC a data item
//
// Inputs       : iv - the initialization vector for the MAC
//                macval - the place to put the MAC value
//                mlen - the length of the MAC value buffer
//                in - the buffer to MAC
//                ilen - the length of the buffer
// Outputs      : true if successful, exception on failure

bool bfsCryptoKey::macData(char *macval, bfs_size_t mlen, char *in,
						   bfs_size_t ilen) {
	return (doMAC(macval, mlen, in, ilen, false));
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCryptoKey::verifyMac
// Description  : Validate the MAC value
//
// Inputs       : iv - the initialization vector for the MAC
//                macval - the MAC value to validate
//                mlen - the length of the MAC value buffer
//                in - the buffer to validate the MAC for
//                ilen - the length of the buffer
// Outputs      : true if successful, exception on failure

bool bfsCryptoKey::verifyMac(char *macval, bfs_size_t mlen, char *in,
							 bfs_size_t ilen) {
	return (doMAC(macval, mlen, in, ilen, true));
}

//
// Static class methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCryptoKey::createRandomKey
// Description  : Create a random key with defaults
//
// Inputs       : none
// Outputs      : pointer to the new key

bfsCryptoKey *bfsCryptoKey::createRandomKey(void) {

	// Generate the new key data, construct and return
	char newkey[getDefaultKeySize()];

#ifndef __BFS_ENCLAVE_MODE
	get_random_data(newkey, (uint32_t)getDefaultKeySize());
#else
	if (sgx_read_rand((unsigned char *)newkey, (uint32_t)getDefaultKeySize()) !=
		SGX_SUCCESS) {
		std::string message =
			std::string("Failed generating random iv in encryptData");
		throw new bfsCryptoError(message);
	}
#endif

	return (new bfsCryptoKey(newkey, getDefaultKeySize()));
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCryptoKey::toBase64
// Description  : Convert key into base64 representation
//
// Inputs       : none
// Outputs      : string containing the base 64 encoding

string bfsCryptoKey::toBase64(void) {

	// Setup the keybuffer, convert
#ifndef __BFS_ENCLAVE_MODE
	bfsFlexibleBuffer keybuf(keydat, (bfs_size_t)keylen);
#else
	bfsFlexibleBuffer keybuf(cipher_keydat, (bfs_size_t)cipher_keylen);
#endif

	string encoded;
	bfs_toBase64(keybuf, encoded);

	// Return the encoded key data
	return (encoded);
}

//
// Private Class Methods

int bfsCryptoKey::hmacData(uint8_t *out, bfs_size_t mac_size, uint8_t *left,
						   uint8_t *right, int len) {
#ifndef __BFS_ENCLAVE_MODE
	// gcry_md_hd_t h;
	// gcry_md_open(&h, GCRY_MD_SHA256, GCRY_MD_FLAG_HMAC);
	// gcry_md_setkey(h, keydat, keylen);

	gcry_md_write(hmac_h, left, len);
	gcry_md_write(hmac_h, right, len);
	unsigned char *_mac = gcry_md_read(hmac_h, GCRY_MD_SHA256);
	memcpy(out, _mac, mac_size);
	// gcry_md_close(h);
	gcry_md_reset(hmac_h);
#else
	// if (sgx_hmac256_init((const unsigned char *)cipher_keydat, cipher_keylen,
	// 					 &hmac_hh) != SGX_SUCCESS) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed sgx_hmac256_init\n");
	// 	return NULL;
	// }

	// if (sgx_hmac256_update(left, len, hmac_hh) != SGX_SUCCESS) {
	// 	logMessage(LOG_ERROR_LEVEL,
	// 			   "tcrypto failure sgx_hmac256_update on left child");
	// 	return BFS_FAILURE;
	// }

	// if (sgx_hmac256_update(right, len, hmac_hh) != SGX_SUCCESS) {
	// 	logMessage(LOG_ERROR_LEVEL,
	// 			   "tcrypto failure sgx_hmac256_update on right child");
	// 	return BFS_FAILURE;
	// }

	// if (sgx_hmac256_final(out, mac_size, hmac_hh) != SGX_SUCCESS) {
	// 	logMessage(LOG_ERROR_LEVEL,
	// 			   "tcrypto failure sgx_hmac256_update on right child");
	// 	return BFS_FAILURE;
	// }

	// if (sgx_hmac256_close(hmac_hh) != SGX_SUCCESS) {
	// 	logMessage(LOG_ERROR_LEVEL, "tcrypto failure sgx_hmac256_close");
	// 	return BFS_FAILURE;
	// }

	char *cat = (char *)malloc(len * 2);
	memcpy(cat, left, len);
	memcpy(cat + len, right, len);
	if (sgx_hmac_sha256_msg((const unsigned char *)cat, len * 2,
							(unsigned char *)cipher_keydat, cipher_keylen,
							(unsigned char *)out, mac_size) != SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed hmacData\n");
		return NULL;
	}
	free(cat);
#endif

	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCryptoKey::doMAC
// Description  : Perform a MAC (for creation or validation, see "verify"
// param).
//                Only called for non-enclave code, since SGX always computes a
//                MAC tag during encrypt.
//
// Inputs       : macval - the place to put the MAC value
//                mlen - the length of the MAC value buffer
//                in - the buffer to MAC
//                ilen - the length of the buffer
// Outputs      : true if successful, false if verifying and MAC failed,
//                exception on functional failure

bool bfsCryptoKey::doMAC(char *macval, bfs_size_t mlen, char *in,
						 bfs_size_t ilen, bool verify) {

	// Local variables
	string message;
	// size_t cmlen;
	(void)in;

	// Check the cipher initialized
	if (!cipherInitialized == true) {
		message = (string) "Attempting to doMAC using uninitialized crypto "
						   "key, abort";
		throw new bfsCryptoError(message);
	}

	// Log the operation
	logMessage(CRYPTO_VRBLOG_LEVEL, "%s MAC with keyid %u, data len %d bytes",
			   (verify) ? "Verifying" : "Creating", keyid, ilen);

#ifndef __BFS_ENCLAVE_MODE
	gcry_error_t err;

	if (!verify) {
		// This function should be called right after encryption since it will
		// give the MAC based on the current cipher state. Completes the MAC
		// routine by reading the mac into the buffer from the internal cipher
		// state. If we need to verify, then the tag is checked too. For sgx
		// code, the mac is computed with the encryption call and state is not
		// preserved, so we pass the tag buffer to encrypt for those. Note that
		// this is only used for AEAD ciphers (see gcrypt header)
		if ((err = gcry_cipher_gettag(cipher, macval, mlen)) !=
			GPG_ERR_NO_ERROR) {
			message = (string) "gcrypt failure reading MAC tag: " +
					  gcry_strerror(err);
			throw new bfsCryptoError(message);
		}

		// // Reset the MAC
		// if ( (err = gcry_mac_reset(mac)) != GPG_ERR_NO_ERROR ) {
		// 	message = (string)"gcrypt failure resetting MAC: " +
		// gcry_strerror(err); 	throw new bfsCryptoError( message );
		// }

		// // Write the data to the MAC computation
		// if ( (err = gcry_mac_write(mac, in, ilen)) != GPG_ERR_NO_ERROR ) {
		// 	message = (string)"gcrypt failure writing MAC input: " +
		// gcry_strerror(err); 	throw new bfsCryptoError( message );
		// }

		// Check if we are verifying the MAC
		// if (verify) {
	} else {
		// Verify the MAC (see for more info:
		// https://www.gnupg.org/documentation/manuals/gcrypt/Working-with-cipher-handles.html).
		// Note that SGX tag verified automatically during decrypt.
		if ((err = gcry_cipher_checktag(cipher, macval, mlen)) !=
			GPG_ERR_NO_ERROR) {
			message = (string) "gcrypt MAC verification failure: " +
					  gcry_strerror(err);
			throw new bfsCryptoError(message);
		}

		// if ( (err = gcry_mac_verify(mac, macval, mlen)) != GPG_ERR_NO_ERROR )
		// {

		//     // If the error is that the MAC failed, just return false
		//     if ( err == GPG_ERR_CHECKSUM ) {
		//         logMessage( CRYPTO_LOG_LEVEL, "MAC verification failed on
		//         data [len=%s]", ilen ); return( false );
		//     }

		//     // Some other error occured, bail out
		//     message = (string)"gcrypt failure resetting MAC: " +
		//     gcry_strerror(err); throw new bfsCryptoError( message );
		// }
	}

	// Not verifying, just create the MAC value
	// cmlen = mlen;
	// if ( (err = gcry_mac_read(mac, macval, &mlen)) != GPG_ERR_NO_ERROR ) {
	// 	message = (string)"gcrypt failure reading MAC input: " +
	// gcry_strerror(err); 	throw new bfsCryptoError( message );
	// }

	// // Sanity check the MAC lengths
	// if ( cmlen  != mlen ) {
	// 	message = (string)"bfsCryptoKey failure, MAC length mismatch.";
	// 	throw new bfsCryptoError( message );
	// }
#else
	(void)macval;
	(void)mlen;
#endif

	// Return successfully
	return (true);
}
