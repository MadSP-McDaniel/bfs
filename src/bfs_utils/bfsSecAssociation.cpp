////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsSecAssociation.cpp
//  Description   : This file contains the implementation of the security
//  association
//                  that is used to secure communication between endpoints in
//                  the bfs filesystem.  Note that this is a simplex connection
//                  with an initiator (sender) and a responder (recipient).
//
//  Author		  : Patrick McDaniel
//  Created	      : Thu 22 Apr 2021 06:31:56 PM EDT
//

// Includes

// Project Includes
#include <bfsCryptoError.h>
#include <bfsSecAssociation.h>
#include <bfs_base64.h>
#include <bfs_log.h>
#include <bfs_util.h>

//
// Class Data

//
// Class Methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsSecAssociation::bfsSecAssociation
// Description  : Attrivute constructor
//
// Inputs       : in - the initiator identity
//                resp - the responder identity
//                key - the key for the association
// Outputs      : none

bfsSecAssociation::bfsSecAssociation(string in, string resp, bfsCryptoKey *key)
	: initiator(in), responder(resp), saKey(NULL) {

	// Set the key as necessary
	if (key != NULL) {
		setKey(key);
	}

	// Return, no return code
	logMessage(CRYPTO_LOG_LEVEL,
			   "Created security association [%s/%s], key id = %d",
			   initiator.c_str(), responder.c_str(),
			   (saKey != NULL) ? (int)saKey->getKeyId() : -1);

	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsSecAssociation::bfsSecAssociation
// Description  : Configuration constructor
//
// Inputs       : config - configuration to prcess
//                secure_buf - flag indicating if the buffer for parsing the key
//                             should be in secure memory or not
// Outputs      : none

bfsSecAssociation::bfsSecAssociation(bfsCfgItem *config, bool secure_buf)
	: bfsSecAssociation() {

	// Local variables
	bfsCfgItem *subitem;
	bfsFlexibleBuffer keybuf;
	bfsSecureFlexibleBuffer secure_keybuf;

	// Get the initiator configuration item
#ifdef __BFS_ENCLAVE_MODE
	const long sub_max_len = 100;
	char _initiator[sub_max_len] = {0}, _responder[sub_max_len] = {0},
		 _key[sub_max_len] = {0};
	int64_t ret = 0, ocall_status = 0;

	subitem = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&subitem, (int64_t)config, "initiator",
			  strlen("initiator") + 1)) != SGX_SUCCESS)) {
		string message =
			"Failure missing SA initiator in config on constructor";
		throw new bfsCryptoError(message);
	}
	if (((ocall_status = ocall_bfsCfgItemValue(&ret, (int64_t)subitem,
											   _initiator, sub_max_len)) !=
		 SGX_SUCCESS) ||
		(ret != BFS_SUCCESS)) {
		string message = "Failed ocall_bfsCfgItemValue";
		throw new bfsCryptoError(message);
	}
	initiator = std::string(_initiator);

	subitem = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&subitem, (int64_t)config, "responder",
			  strlen("responder") + 1)) != SGX_SUCCESS)) {
		string message =
			"Failure missing SA responder in config on constructor";
		throw new bfsCryptoError(message);
	}
	if (((ocall_status = ocall_bfsCfgItemValue(&ret, (int64_t)subitem,
											   _responder, sub_max_len)) !=
		 SGX_SUCCESS) ||
		(ret != BFS_SUCCESS)) {
		string message = "Failed ocall_bfsCfgItemValue";
		throw new bfsCryptoError(message);
	}
	responder = std::string(_responder);

	subitem = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&subitem, (int64_t)config, "key",
			  strlen("key") + 1)) != SGX_SUCCESS)) {
		string message = "Failure missing SA key in config on constructor";
		throw new bfsCryptoError(message);
	}
	if (((ocall_status = ocall_bfsCfgItemValue(&ret, (int64_t)subitem, _key,
											   sub_max_len)) != SGX_SUCCESS) ||
		(ret != BFS_SUCCESS)) {
		string message = "Failed ocall_bfsCfgItemValue";
		throw new bfsCryptoError(message);
	}

	if (secure_buf)
		bfs_fromBase64(std::string(_key), secure_keybuf);
	else
		bfs_fromBase64(std::string(_key), keybuf);
#else
	if ((subitem = config->getSubItemByName("initiator")) == NULL) {
		string message =
			"Failure missing SA initiator in config on constructor";
		throw new bfsCryptoError(message);
	}
	initiator = subitem->bfsCfgItemValue();

	// Get the responder configuration item
	if ((subitem = config->getSubItemByName("responder")) == NULL) {
		string message =
			"Failure missing SA responder in config on constructor";
		throw new bfsCryptoError(message);
	}
	responder = subitem->bfsCfgItemValue();

	// Get the SA key
	if ((subitem = config->getSubItemByName("key")) == NULL) {
		string message = "Failure missing SA key in config on constructor";
		throw new bfsCryptoError(message);
	}
	if (secure_buf)
		bfs_fromBase64(subitem->bfsCfgItemValue(), secure_keybuf);
	else
		bfs_fromBase64(subitem->bfsCfgItemValue(), keybuf);
#endif
	if (secure_buf)
		saKey = new bfsCryptoKey(secure_keybuf.getBuffer(),
								 secure_keybuf.getLength());
	else
		saKey = new bfsCryptoKey(keybuf.getBuffer(), keybuf.getLength());

	// Return, no return code
	logMessage(CRYPTO_LOG_LEVEL,
			   "Created security association [%s/%s], key id = %d",
			   initiator.c_str(), responder.c_str(),
			   (saKey != NULL) ? (int)saKey->getKeyId() : -1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsSecAssociation::~bfsSecAssociation
// Description  : Destructor
//
// Inputs       : none
// Outputs      : none

bfsSecAssociation::~bfsSecAssociation(void) {
	// Return, no return code
	logMessage(CRYPTO_LOG_LEVEL, "Destroyed security association [%s/%s]",
			   initiator.c_str(), responder.c_str());
	return;
}

//
// Class methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsSecAssociation::setKey
// Description  : Set the key for the security association
//
// Inputs       : key - the key to set for the association
// Outputs      : 0 or throws exception on error

int bfsSecAssociation::setKey(bfsCryptoKey *key) {

	// Log and set the key
	logMessage(CRYPTO_LOG_LEVEL, "Setting key for security association [%s/%s]",
			   initiator.c_str(), responder.c_str());
	saKey = key;

	// Return successfully
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsSecAssociation::encryptData
// Description  : In-place encryption of data
//
// Inputs       : buf - buffer to encrypt (in place)
// Outputs      : 0 or throws exception on error

int bfsSecAssociation::encryptData(bfsFlexibleBuffer &buf,
								   bfsFlexibleBuffer *aad, bool mac) {

	// Check the state of the association
	if (saKey == NULL) {
		string message =
			(string) "Attempting to encrypt using security with NULL key";
		throw new bfsCryptoError(message);
	}

	// memset(buf.getBuffer(), 0x0, buf.getLength());

	// Add the padding, setup output to padded size
	addPKCS7Padding(buf);

	// Setup a random IV, do the encryption
	iv.resetWithAlloc((bfs_size_t)saKey->getIVlen());

#ifndef __BFS_ENCLAVE_MODE
	// TODO: use counter (seq perhaps) as IV for GCM
	get_random_data(iv.getBuffer(), (bfs_size_t)saKey->getIVlen());
	// memset(iv.getBuffer(), 0x0, 12);
	// saKey->encryptData(iv.getBuffer(), buf.getBuffer(), buf.getLength());
	saKey->encryptData(iv.getBuffer(), buf.getBuffer(), buf.getLength(),
					   aad ? aad->getBuffer() : NULL,
					   aad ? aad->getLength() : 0, NULL);

	buf.addHeader(iv.getBuffer(), iv.getLength());

	// MAC as necessary, adding as trailer
	if (mac)
		macData(buf);

#else
	// if (!dynamic_cast<bfsSecureFlexibleBuffer *>(&buf)) {
	// 	std::string message = std::string("encrypt buffer is not a secure
	// buffer\n"); 	throw new bfsCryptoError(message);
	// }

	if (sgx_read_rand((unsigned char *)iv.getBuffer(),
					  (bfs_size_t)saKey->getIVlen()) != SGX_SUCCESS) {
		std::string message =
			std::string("Failed generating random iv in encryptData");
		throw new bfsCryptoError(message);
	}

	// Mimic in-place encryption for now so other enclave code doesn't break; eg
	// device code (in-place not natively supported by sgx crypto)
	sgx_aes_gcm_128bit_tag_t mtag = {0};
	// bfsSecureFlexibleBuffer buf_cpy(buf);
	// saKey->encryptData(iv.getBuffer(), buf.getBuffer(), buf.getLength(),
	// 				   buf_cpy.getBuffer(), buf_cpy.getLength(),
	// 				   (unsigned char **)&mtag);
	char *buf_cpy = (char *)calloc(buf.getLength(),
								   1); // avoid destructor calls with buf_cpy
	memcpy(buf_cpy, buf.getBuffer(), buf.getLength());
	saKey->encryptData(iv.getBuffer(), buf.getBuffer(), buf.getLength(),
					   buf_cpy, buf.getLength(), aad ? aad->getBuffer() : NULL,
					   aad ? aad->getLength() : 0, (unsigned char **)&mtag);
	buf.addHeader(iv.getBuffer(), iv.getLength());
	free(buf_cpy);

	// Just append the computed mac as a trailer (adapted from macData)
	size_t len = 0;
	char *dat, *mac_ptr, dummy[saKey->getMACsize()];
	std::string message;
	if (mac) {
		if (buf.getLength() <= saKey->getBlocksize()) {
			message = (string) "sec association failure short buffer on MAC "
							   "(encrypt)";
			throw new bfsCryptoError(message);
		}

		// Setup the mac and location (ignore the IV)
		len = buf.getLength() - saKey->getIVlen();
		buf.addTrailer(dummy, (bfs_size_t)saKey->getMACsize());
		dat = buf.getBuffer() + saKey->getIVlen();
		mac_ptr = dat + len;

		// copy the computed MAC tag into the buffer for sending
		memcpy(mac_ptr, (unsigned char *)mtag, (bfs_size_t)saKey->getMACsize());
	}
#endif

	// Return successfully
	return (0);
}

int bfsSecAssociation::encryptData2(bfsFlexibleBuffer &buf,
									bfsFlexibleBuffer *aad, uint8_t **iv,
									uint8_t **mac) {

	// Check the state of the association
	if (saKey == NULL) {
		string message =
			(string) "Attempting to encrypt using security with NULL key";
		throw new bfsCryptoError(message);
	}

	// memset(buf.getBuffer(), 0x0, buf.getLength());

	// Add the padding, setup output to padded size
	// addPKCS7Padding(buf);

	// Setup a random IV, do the encryption
	// iv.resetWithAlloc((bfs_size_t)saKey->getIVlen());
	if (!(*iv || *mac)) {
		std::string message = std::string("NULL iv or mac in encryptData2");
		throw new bfsCryptoError(message);
	}

#ifndef __BFS_ENCLAVE_MODE
	// TODO: use counter (seq perhaps) as IV for GCM
	get_random_data((char *)*iv, (bfs_size_t)saKey->getIVlen());
	// memset(iv.getBuffer(), 0x0, 12);
	// saKey->encryptData(iv.getBuffer(), buf.getBuffer(), buf.getLength());
	saKey->encryptData((char *)*iv, buf.getBuffer(), buf.getLength(),
					   aad ? aad->getBuffer() : NULL,
					   aad ? aad->getLength() : 0, NULL);

	// buf.addHeader(*iv, saKey->getIVlen());

	// MAC as necessary, adding as trailer
	// if (mac)
	// 	macData(buf);
	saKey->macData((char *)*mac, saKey->getMACsize(), NULL, 0);

#else
	// if (!dynamic_cast<bfsSecureFlexibleBuffer *>(&buf)) {
	// 	std::string message = std::string("encrypt buffer is not a secure
	// buffer\n"); 	throw new bfsCryptoError(message);
	// }

	if (sgx_read_rand((unsigned char *)*iv, (bfs_size_t)saKey->getIVlen()) !=
		SGX_SUCCESS) {
		std::string message =
			std::string("Failed generating random iv in encryptData");
		throw new bfsCryptoError(message);
	}

	// Mimic in-place encryption for now so other enclave code doesn't break; eg
	// device code (in-place not natively supported by sgx crypto)
	sgx_aes_gcm_128bit_tag_t mtag = {0};
	// bfsSecureFlexibleBuffer buf_cpy(buf);
	// saKey->encryptData(iv.getBuffer(), buf.getBuffer(), buf.getLength(),
	// 				   buf_cpy.getBuffer(), buf_cpy.getLength(),
	// 				   (unsigned char **)&mtag);
	char *buf_cpy = (char *)calloc(buf.getLength(),
								   1); // avoid destructor calls with buf_cpy
	memcpy(buf_cpy, buf.getBuffer(), buf.getLength());
	saKey->encryptData((char *)*iv, buf.getBuffer(), buf.getLength(), buf_cpy,
					   buf.getLength(), aad ? aad->getBuffer() : NULL,
					   aad ? aad->getLength() : 0, (unsigned char **)&mtag);
	// buf.addHeader(*iv, saKey->getIVlen());
	free(buf_cpy);

	// Just append the computed mac as a trailer (adapted from macData)
	// size_t len = 0;
	// char *dat, *mac_ptr, dummy[saKey->getMACsize()];
	// std::string message;
	// if (mac) {
	// 	if (buf.getLength() <= saKey->getBlocksize()) {
	// 		message = (string) "sec association failure short buffer on MAC "
	// 						   "(encrypt)";
	// 		throw new bfsCryptoError(message);
	// 	}

	// 	// Setup the mac and location (ignore the IV)
	// 	len = buf.getLength() - saKey->getIVlen();
	// 	buf.addTrailer(dummy, (bfs_size_t)saKey->getMACsize());
	// 	dat = buf.getBuffer() + saKey->getIVlen();
	// 	mac_ptr = dat + len;

	// 	// copy the computed MAC tag into the buffer for sending
	// 	memcpy(mac_ptr, (unsigned char *)mtag, (bfs_size_t)saKey->getMACsize());
	// }
	memcpy(*mac, (unsigned char *)mtag, (bfs_size_t)saKey->getMACsize());
#endif

	// Return successfully
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsSecAssociation::encryptData
// Description  : Output buffer encryption of data (out = E(buf))
//
// Inputs       : buf - the buffer to encrypt
//                out - the output buffer to place ciphertext into
// Outputs      : 0 or throws exception on error

int bfsSecAssociation::encryptData(bfsFlexibleBuffer &buf,
								   bfsFlexibleBuffer &out,
								   bfsFlexibleBuffer *aad, bool mac) {

	// Check the state of the association
	if (saKey == NULL) {
		string message =
			(string) "Attempting to encrypt using security with NULL key";
		throw new bfsCryptoError(message);
	}

	// Add the padding, setup output to padded size
	addPKCS7Padding(buf);

	out.resetWithAlloc(buf.getLength());

	// Setup a random IV, do the encryption
	iv.resetWithAlloc((bfs_size_t)saKey->getIVlen());

#ifndef __BFS_ENCLAVE_MODE
	// if (saKey->add_add(aad) != BFS_SUCCESS) {
	// 	std::string message = std::string("Failed adding aad in encryptData");
	// 	throw new bfsCryptoError(message);
	// }

	get_random_data(iv.getBuffer(), (bfs_size_t)saKey->getIVlen());
	// memset(iv.getBuffer(), 0x0, 12);
	saKey->encryptData(iv.getBuffer(), out.getBuffer(), out.getLength(),
					   buf.getBuffer(), buf.getLength(),
					   aad ? aad->getBuffer() : NULL,
					   aad ? aad->getLength() : 0, NULL);
	out.addHeader(iv.getBuffer(), iv.getLength());
	removePKCS7Padding(buf); // Restore original state

	// MAC as necessary, adding as trailer
	if (mac)
		macData(out);

#else
	// if (!dynamic_cast<bfsSecureFlexibleBuffer *>(&out)) {
	// 	std::string message = std::string("encrypt buffer is not a secure
	// buffer\n"); 	throw new bfsCryptoError(message);
	// }

	if (sgx_read_rand((unsigned char *)iv.getBuffer(),
					  (bfs_size_t)saKey->getIVlen()) != SGX_SUCCESS) {
		std::string message =
			std::string("Failed generating random iv in encryptData");
		throw new bfsCryptoError(message);
	}

	sgx_aes_gcm_128bit_tag_t mtag = {0};
	saKey->encryptData(iv.getBuffer(), out.getBuffer(), out.getLength(),
					   buf.getBuffer(), buf.getLength(),
					   aad ? aad->getBuffer() : NULL,
					   aad ? aad->getLength() : 0, (unsigned char **)&mtag);
	out.addHeader(iv.getBuffer(), iv.getLength());
	removePKCS7Padding(buf); // Restore original state

	// Just append the computed mac as a trailer (adapted from macData)
	size_t len = 0;
	char *dat, *mac_ptr, dummy[saKey->getMACsize()];
	std::string message;
	if (mac) {
		if (out.getLength() <= saKey->getBlocksize()) {
			message = (string) "sec association failure short buffer on MAC "
							   "(encrypt2)";
			throw new bfsCryptoError(message);
		}

		// Setup the mac and location (ignore the IV)
		len = out.getLength() - saKey->getIVlen();
		out.addTrailer(dummy, (bfs_size_t)saKey->getMACsize());
		dat = out.getBuffer() + saKey->getIVlen();
		mac_ptr = dat + len;

		// copy the computed MAC tag into the buffer for sending
		memcpy(mac_ptr, (unsigned char *)mtag, (bfs_size_t)saKey->getMACsize());
	}
#endif

	// Return successfully
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsSecAssociation::decryptData
// Description  : In-place decryption of data
//
// Inputs       : buf - buffer to decrypt
// Outputs      : 0 or throws exception on error

int bfsSecAssociation::decryptData(bfsFlexibleBuffer &buf,
								   bfsFlexibleBuffer *aad, bool mac,
								   uint8_t *const mac_out) {

	// Check the state of the association
	if (saKey == NULL) {
		string message =
			(string) "Attempting to decrypt using security with NULL key";
		throw new bfsCryptoError(message);
	}

// Verify MAC as necessary, adding as trailer
#ifndef __BFS_ENCLAVE_MODE
	char mac_copy[saKey->getMACsize()];
#else
	sgx_aes_gcm_128bit_tag_t mac_copy;
#endif

	// copy mac tag out (remove trailer from buf) so we can resize the out
	// buffer appropriately for decryption
	if (mac)
		buf.removeTrailer(mac_out ? (char *)mac_out : (char *)mac_copy,
						  (bfs_size_t)saKey->getMACsize());

	// Remove the IV from the buffer
	iv.resetWithAlloc((bfs_size_t)saKey->getIVlen());
	buf.removeHeader(iv.getBuffer(), (bfs_size_t)saKey->getIVlen());

#ifndef __BFS_ENCLAVE_MODE
	// saKey->decryptData(iv.getBuffer(), buf.getBuffer(), buf.getLength());
	// Note: need to pass aad to decrypt call so it adds the aad before calling
	// decrypt routine (aad buffer not needed anymore in verify)
	saKey->decryptData(iv.getBuffer(), buf.getBuffer(), buf.getLength(),
					   aad ? aad->getBuffer() : NULL,
					   aad ? aad->getLength() : 0, NULL);

	// if (mac && !verifyMac(buf, aad)) {
	// 	string message = "Sec assoc: MAC failed on decryption";
	// 	throw new bfsCryptoError(message);
	// }
	if (mac)
		saKey->verifyMac(mac_out ? (char *)mac_out : (char *)mac_copy,
						 saKey->getMACsize(), NULL, 0);
#else
	// Mimic in-place decryption for now
	// if (!dynamic_cast<bfsSecureFlexibleBuffer *>(&buf)) {
	// 	std::string message = std::string("decrypt buffer is not a secure
	// buffer\n"); 	throw new bfsCryptoError(message);
	// }

	// bfsSecureFlexibleBuffer buf_cpy(buf);
	// saKey->decryptData(iv.getBuffer(), buf.getBuffer(), buf.getLength(),
	// 				   buf_cpy.getBuffer(), buf_cpy.getLength(),
	// 				   (uint8_t **)&mac_copy);
	char *buf_cpy = (char *)calloc(buf.getLength(),
								   1); // avoid destructor calls with buf_cpy
	memcpy(buf_cpy, buf.getBuffer(), buf.getLength());
	saKey->decryptData(iv.getBuffer(), buf.getBuffer(), buf.getLength(),
					   buf_cpy, buf.getLength(), aad ? aad->getBuffer() : NULL,
					   aad ? aad->getLength() : 0,
					   mac ? (mac_out ? (char *)mac_out : (char *)mac_copy)
						   : NULL);
	free(buf_cpy);
#endif

	removePKCS7Padding(buf);

	// Return successfully
	return (0);
}

int bfsSecAssociation::decryptData2(bfsFlexibleBuffer &buf,
									bfsFlexibleBuffer *aad, uint8_t *iv,
									uint8_t *mac) {

	// Check the state of the association
	if (saKey == NULL) {
		string message =
			(string) "Attempting to decrypt using security with NULL key";
		throw new bfsCryptoError(message);
	}

	if (!(iv || mac)) {
		std::string message = std::string("NULL iv or mac in decryptData2");
		throw new bfsCryptoError(message);
	}

	// Verify MAC as necessary, adding as trailer
	// #ifndef __BFS_ENCLAVE_MODE
	// 	char mac_copy[saKey->getMACsize()];
	// #else
	// 	sgx_aes_gcm_128bit_tag_t mac_copy;
	// #endif

	// copy mac tag out (remove trailer from buf) so we can resize the out
	// buffer appropriately for decryption
	// if (mac)
	// 	buf.removeTrailer(mac_out ? (char *)mac_out : (char *)mac_copy,
	// 					  (bfs_size_t)saKey->getMACsize());

	// Remove the IV from the buffer
	// iv.resetWithAlloc((bfs_size_t)saKey->getIVlen());
	// buf.removeHeader(iv.getBuffer(), (bfs_size_t)saKey->getIVlen());

#ifndef __BFS_ENCLAVE_MODE
	// saKey->decryptData(iv.getBuffer(), buf.getBuffer(), buf.getLength());
	// Note: need to pass aad to decrypt call so it adds the aad before calling
	// decrypt routine (aad buffer not needed anymore in verify)
	saKey->decryptData((char *)iv, buf.getBuffer(), buf.getLength(),
					   aad ? aad->getBuffer() : NULL,
					   aad ? aad->getLength() : 0, NULL);

	// if (mac && !verifyMac(buf, aad)) {
	// 	string message = "Sec assoc: MAC failed on decryption";
	// 	throw new bfsCryptoError(message);
	// }
	// if (mac)
	// 	saKey->verifyMac(mac_out ? (char *)mac_out : (char *)mac,
	// 					 saKey->getMACsize(), NULL, 0);
	saKey->verifyMac((char *)mac, saKey->getMACsize(), NULL, 0);
#else
	// Mimic in-place decryption for now
	// if (!dynamic_cast<bfsSecureFlexibleBuffer *>(&buf)) {
	// 	std::string message = std::string("decrypt buffer is not a secure
	// buffer\n"); 	throw new bfsCryptoError(message);
	// }

	// bfsSecureFlexibleBuffer buf_cpy(buf);
	// saKey->decryptData(iv.getBuffer(), buf.getBuffer(), buf.getLength(),
	// 				   buf_cpy.getBuffer(), buf_cpy.getLength(),
	// 				   (uint8_t **)&mac_copy);
	char *buf_cpy = (char *)calloc(buf.getLength(),
								   1); // avoid destructor calls with buf_cpy
	memcpy(buf_cpy, buf.getBuffer(), buf.getLength());
	saKey->decryptData((char *)iv, buf.getBuffer(), buf.getLength(), buf_cpy,
					   buf.getLength(), aad ? aad->getBuffer() : NULL,
					   aad ? aad->getLength() : 0, mac);
	free(buf_cpy);
#endif

	// removePKCS7Padding(buf);

	// Return successfully
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsSecAssociation::decryptData
// Description  : Output buffer decryption of data (out = D(buf))
//
// Inputs       : buf - buffer to decrypt
//                out - buffer to place recovered plaintext into
// Outputs      : 0 or throws exception on error

int bfsSecAssociation::decryptData(bfsFlexibleBuffer &buf,
								   bfsFlexibleBuffer &out,
								   bfsFlexibleBuffer *aad, bool mac,
								   uint8_t *const mac_out) {

	// Check the state of the association
	if (saKey == NULL) {
		string message =
			(string) "Attempting to decrypt using security with NULL key";
		throw new bfsCryptoError(message);
	}

#ifndef __BFS_ENCLAVE_MODE
	char mac_copy[saKey->getMACsize()];
#else
	sgx_aes_gcm_128bit_tag_t mac_copy;
#endif

	// copy mac tag out (remove trailer from buf) so we can resize the out
	// buffer appropriately for decryption
	if (mac)
		buf.removeTrailer(mac_out ? (char *)mac_out : (char *)mac_copy,
						  (bfs_size_t)saKey->getMACsize());

	// Remove the IV from the buffer
	iv.resetWithAlloc((bfs_size_t)saKey->getIVlen());
	buf.removeHeader(iv.getBuffer(), (bfs_size_t)saKey->getIVlen());
	out.resetWithAlloc(buf.getLength());

#ifndef __BFS_ENCLAVE_MODE
	saKey->decryptData(iv.getBuffer(), out.getBuffer(), out.getLength(),
					   buf.getBuffer(), buf.getLength(),
					   aad ? aad->getBuffer() : NULL,
					   aad ? aad->getLength() : 0, NULL);

	// Verify MAC as necessary for gcrypt code; sgx code always checks MAC
	// if (mac && !verifyMac(buf, aad)) {
	// 	string message = "Sec assoc: MAC failed on decryption";
	// 	throw new bfsCryptoError(message);
	// }
	if (mac)
		saKey->verifyMac(mac_out ? (char *)mac_out : (char *)mac_copy,
						 saKey->getMACsize(), NULL, 0);
#else
	// if (!dynamic_cast<bfsSecureFlexibleBuffer *>(&out)) {
	// 	std::string message = std::string("decrypt buffer is not a secure
	// buffer\n"); 	throw new bfsCryptoError(message);
	// }

	// decrypt just the data, not the MAC
	saKey->decryptData(
		iv.getBuffer(), out.getBuffer(), out.getLength(), buf.getBuffer(),
		buf.getLength(), aad ? aad->getBuffer() : NULL,
		aad ? aad->getLength() : 0,
		mac ? (mac_out ? (char *)mac_out : (char *)mac_copy) : NULL);
#endif

	removePKCS7Padding(out);

	// Return successfully
	return (0);
}

int bfsSecAssociation::hmacData(uint8_t *out, uint8_t *left, uint8_t *right,
								int len) {
	// Local variables
	// uint8_t *mac = (uint8_t *)malloc(saKey->getHMACsize());
	string message;

	// Check the state of the association
	if (saKey == NULL) {
		message = (string) "Attempting to hmac using security with NULL key";
		throw new bfsCryptoError(message);
	}

	// Do the MAC and return successfully
	if (saKey->hmacData(out, saKey->getHMACsize(), left, right, len) !=
		BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed hmacData\n");
		return BFS_FAILURE;
	}

	return BFS_SUCCESS;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsSecAssociation::macData
// Description  : Add a MAC onto an buffer (needs IV)
//
// Inputs       : buf - buffer to compute MAC, append MAC to (the first block
//                      of buffer MUST be the IV)
// Outputs      : 0 or throws exception on error

int bfsSecAssociation::macData(bfsFlexibleBuffer &buf) {
#ifndef __BFS_ENCLAVE_MODE
	// Local variables
	char *dat, *mac, dummy[saKey->getMACsize()];
	string message;
	bfs_size_t len;

	// Check the state of the association
	if (saKey == NULL) {
		message = (string) "Attempting to MAC using security with NULL key";
		throw new bfsCryptoError(message);
	}

	// Sanity check buffer
	if (buf.getLength() <= saKey->getBlocksize()) {
		message =
			(string) "sec association failure short buffer on MAC (macData)";
		throw new bfsCryptoError(message);
	}

	// Setup the mac and location (ignore the IV)
	len = buf.getLength() - saKey->getIVlen();
	buf.addTrailer(dummy, (bfs_size_t)saKey->getMACsize());
	dat = buf.getBuffer() + saKey->getIVlen();
	mac = dat + len;

	// Do the MAC and return successfully
	saKey->macData(mac, saKey->getMACsize(), dat, len);
#else
	(void)buf;
#endif
	return (0);
}

//
// Private class methods

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsSecAssociation::addPKCS7Padding
// Description  : Add the PKCS padding (returns number of pad bytes)
//
// Inputs       : buf - buffer to pad
// Outputs      : pad bytes

size_t bfsSecAssociation::addPKCS7Padding(bfsFlexibleBuffer &buf) {

	// Local variables
	size_t padsz =
		(buf.getLength() % saKey->getBlocksize() == 0)
			? saKey->getBlocksize()
			: saKey->getBlocksize() - buf.getLength() % saKey->getBlocksize();
	char padding[saKey->getBlocksize()];

	// Now add the PKCS#7 padding bytes to the data, return pad bytes
	memset(padding, (char)padsz, padsz);
	buf.addTrailer(padding, (bfs_size_t)padsz);
	return (padsz);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsSecAssociation::removePKCS7Padding
// Description  : Remove the PKCS padding (returns bytes padded)
//
// Inputs       : buf - buffer to pad
// Outputs      : pad bytes or throws exception

size_t bfsSecAssociation::removePKCS7Padding(bfsFlexibleBuffer &buf) {

	// Local variables
	size_t padsz = (size_t)buf.getBuffer()[buf.getLength() - 1];
	char padding[saKey->getBlocksize()];
	string message;

	// Sanity check padding
	if (padsz > saKey->getBlocksize()) {
		message = (string) "Bad PKCS#7 padding, sz=" + to_string(padsz);
		throw new bfsCryptoError(message);
	}

	// Remove the padding and return
	buf.removeTrailer(padding, (bfs_size_t)padsz);
	return (padsz);
}
