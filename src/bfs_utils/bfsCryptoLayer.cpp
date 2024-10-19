////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCryptoLayer.cpp
//  Description   : This is the class implementing the crypto layer interface
//                  for the bfs file system.  Note that this is static class
//                  in which there are no objects.
//
//  Author  : Patrick McDaniel
//  Created : Wed 28 Apr 2021 07:37:03 AM EDT
//

// Include files

// Project include files
#include <bfsCfgItem.h>
#include <bfsCfgParserError.h>
#include <bfsCfgStore.h>
#include <bfsConfigLayer.h>
#include <bfsCryptoError.h>
#include <bfsCryptoKey.h>
#include <bfsCryptoLayer.h>
#include <bfsSecAssociation.h>
#include <bfs_log.h>
#include <bfs_util.h>

#ifdef __BFS_DEBUG_NO_ENCLAVE
/* For non-enclave testing; just directly call the ecall function */
#include "bfs_util_ecalls.h"
#elif defined(__BFS_NONENCLAVE_MODE)
#include "sgx_tcrypto.h" /* For sgx mac tag type */
/* For making legitimate ecalls */
#include "bfs_enclave_u.h"
#include "sgx_urts.h"
static sgx_enclave_id_t eid = 0; // for testing with enclave
#endif

//
// Class Data

// Create the static class data
unsigned long bfsCryptoLayer::bfsCryptoLogLevel = (unsigned long)0;
unsigned long bfsCryptoLayer::bfsVerboseCryptoLogLevel = (unsigned long)0;

// Static initializer, make sure this is idenpendent of other layers
bool bfsCryptoLayer::bfsCryptoLayerInitialized = false;

//
// Class Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCryptoLayer::bfsCryptoLayerInit
// Description  : Initialize the crypto layer
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int bfsCryptoLayer::bfsCryptoLayerInit(void) {

	// Local variables
	bfsCfgItem *config;
	bool crylog, vrblog;

	// bfsUtilLayer::bfsUtilLayerInit();
	// bfsConfigLayer::bfsConfigLayerInit();

#ifdef __BFS_ENCLAVE_MODE
	const long log_flag_max_len = 10;
	char log_enabled_flag[log_flag_max_len] = {0},
		 log_verbose_flag[log_flag_max_len] = {0};
	bfsCfgItem *subcfg;
	int64_t ret = 0, ocall_status = 0;

	if (((ocall_status = ocall_getConfigItem(
			  &ret, BFS_CRYPTLYR_CONFIG, strlen(BFS_CRYPTLYR_CONFIG) + 1)) !=
		 SGX_SUCCESS) ||
		(ret == (int64_t)NULL)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getConfigItem\n");
		return (-1);
	}
	config = (bfsCfgItem *)ret;

	if (((ocall_status = ocall_bfsCfgItemType(&ret, (int64_t)config)) !=
		 SGX_SUCCESS) ||
		(ret != bfsCfgItem_STRUCT)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Unable to find crypto configuration in system config "
				   "(ocall_bfsCfgItemType) : %s",
				   BFS_CRYPTLYR_CONFIG);
		return (-1);
	}

	subcfg = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&subcfg, (int64_t)config, "log_enabled",
			  strlen("log_enabled") + 1)) != SGX_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
		return (-1);
	}
	if (((ocall_status =
			  ocall_bfsCfgItemValue(&ret, (int64_t)subcfg, log_enabled_flag,
									log_flag_max_len)) != SGX_SUCCESS) ||
		(ret != BFS_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
		return (-1);
	}
	crylog = (std::string(log_enabled_flag) == "true");
	bfsCryptoLogLevel = registerLogLevel("CRYPTO_LOG_LEVEL", crylog);

	subcfg = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&subcfg, (int64_t)config, "log_verbose",
			  strlen("log_verbose") + 1)) != SGX_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
		return (-1);
	}
	if (((ocall_status =
			  ocall_bfsCfgItemValue(&ret, (int64_t)subcfg, log_verbose_flag,
									log_flag_max_len)) != SGX_SUCCESS) ||
		(ret != BFS_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_bfsCfgItemValue");
		return (-1);
	}
	vrblog = (std::string(log_verbose_flag) == "true");
	bfsVerboseCryptoLogLevel = registerLogLevel("CRYPTO_LOG_LEVEL", vrblog);

	// TODO initialize the sgx crypto library
#else
	// Get the crypto layer configuration
	config = bfsConfigLayer::getConfigItem(BFS_CRYPTLYR_CONFIG);
	if (config->bfsCfgItemType() != bfsCfgItem_STRUCT) {
		logMessage(LOG_ERROR_LEVEL,
				   "Unable to find crypto configuration in system config : %s",
				   BFS_CRYPTLYR_CONFIG);
		return (-1);
	}

	// Setup the logging
	crylog =
		(config->getSubItemByName("log_enabled")->bfsCfgItemValue() == "true");
	bfsCryptoLogLevel = registerLogLevel("CRYPTO_LOG_LEVEL", crylog);
	vrblog =
		(config->getSubItemByName("log_verbose")->bfsCfgItemValue() == "true");
	bfsVerboseCryptoLogLevel = registerLogLevel("CRYPTO_LOG_LEVEL", vrblog);

	// Initialize the gcrypt library (no secure memory needed)
	gcry_check_version(GCRYPT_VERSION);
	gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
	gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
#endif

	// Log the crypto layer being initialized, return successfully
	bfsCryptoLayerInitialized = true;
	logMessage(CRYPTO_LOG_LEVEL, "bfsCryptoLayer initialized. ");
	return (0);
}

#ifdef __BFS_DEBUG_NO_ENCLAVE
////////////////////////////////////////////////////////////////////////////////
//
// Function     : bfsCryptoLayer::bfsCryptoLayerUtest
// Description  : Perform a unit test on the crypto implementation
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int bfsCryptoLayer::bfsCryptoLayerUtest(void) {

	// Local variables
	bfsCryptoKey *key;
	bfsSecAssociation *sa, *sa_list[CRYPTO_UTEST_NUMBER_SAS];
	bfsFlexibleBuffer buf, ibuf, obuf, pbuf, iplace, icipher;
	bfsCfgStore cfg;
	bfsCfgItem *item;
	string filename, cfgname, init, resp;
	size_t lin, lout, expected;
	char val = 'X', *home;
	uint32_t i, saindex;
	bool verify;
	char _aad[8] = {0x0};
	bfsSecureFlexibleBuffer *aad =
		new bfsSecureFlexibleBuffer((char *)_aad, sizeof(_aad));
	// Walk through the unit tests
	bfsCryptoLayerInit();
	logMessage(CRYPTO_LOG_LEVEL, "Starting BFS crypto unit test.");
	try {

		//
		// Load the configuration

		// Log, setup the configuration file
		logMessage(CRYPTO_LOG_LEVEL, "Starting crypto config test.");
		if ((home = getenv("BFS_HOME")) == NULL) {
			throw new bfsCfgError("BFS_HOME undefined, failure");
		}
		filename = (string)home + "/config/bfs_crypto_utest.cfg";
		cfg.loadConfigurationFile(filename);

		// Read the five test security associations (from utest config)
		saindex = 0;
		for (i = 0; i < 5; i++) {
			cfgname = "secassoc[" + to_string(saindex) + "]";
			if ((item = cfg.queryConfig(cfgname)) == NULL) {
				throw new bfsCfgError("Unable to find SA configuration : " +
									  cfgname);
			}
			sa_list[saindex] = new bfsSecAssociation(item);
			saindex++;
		}
		logMessage(CRYPTO_LOG_LEVEL, "Crypto config completed succesfully.");

		// Generate 5 more random security associations
		for (i = 0; i < 5; i++) {
			init = "name" + to_string(i);
			resp = "name" + to_string(i + 1);
			sa_list[saindex] = new bfsSecAssociation(
				init, resp, bfsCryptoKey::createRandomKey());
			saindex++;
		}

		//
		// PKCS Padding

		// Try the PKCS padding
		logMessage(CRYPTO_LOG_LEVEL, "Starting PKCS#7 padding test.");
		for (i = 0; i < key->getDefaultBlockSize() * 2; i++) {

			// Select a random SA
			sa = sa_list[get_random_value(0, 9)];

			// Do the padding
			lin = sa->addPKCS7Padding(buf);
			logMessage(CRYPTO_VRBLOG_LEVEL, "PKCS#7 padded   : %s",
					   buf.toString(5).c_str());
			lout = sa->removePKCS7Padding(buf);
			logMessage(CRYPTO_VRBLOG_LEVEL, "PKCS#7 unpadded : %s",
					   buf.toString(5).c_str());

			// Sanity check things
			expected = (i % key->getDefaultBlockSize() == 0)
						   ? key->getDefaultBlockSize()
						   : key->getDefaultBlockSize() -
								 (i % key->getDefaultBlockSize());
			if ((lin != lout) || (lin != expected)) {
				logMessage(LOG_ERROR_LEVEL,
						   "PKCS#7 padding mismatch %u != %u, exected %u", lin,
						   lout, expected);
				return (-1);
			}

			// Add another byte to the buffer
			buf << val;
		}
		logMessage(CRYPTO_LOG_LEVEL,
				   "PKCS#7 padding test completed succesfully.");

		//
		// Encyrption/MAC test

		// Iterate a bunch of random values
		logMessage(CRYPTO_LOG_LEVEL,
				   "Encryption/MAC test completed succesfully.");
		for (i = 0; i < CRYPTO_ENCDEC_UTEST_ITERATIONS; i++) {

			// Select a random SA
			sa = sa_list[get_random_value(0, 9)];

			// Setup some random data, log attempt
			lin = get_random_value(1, 32);
			ibuf.resetWithAlloc((bfs_size_t)lin);
			get_random_data(ibuf.getBuffer(), ibuf.getLength());
			iplace = ibuf;
			logMessage(CRYPTO_VRBLOG_LEVEL,
					   "Testing random data encryption, len=%d", lin);
			logMessage(CRYPTO_VRBLOG_LEVEL, "Plaintext: %s",
					   ibuf.toString(5).c_str());

			// Determine if we need to generate and verify a MAC
			verify = (bool)get_random_value(0, 1);

			// Try buf->buf encryption style
			// sa->encryptData(ibuf, obuf, aad, NULL, NULL);
			// sa->decryptData(obuf, pbuf, aad, NULL, NULL);
			if (ibuf != pbuf) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failed encryption/decryption buf->buf comparison.");
				return (-1);
			}

			// Now do the inplace buffer
			// sa->encryptData(iplace, aad, NULL, NULL);
			// icipher = iplace;
			// sa->decryptData(iplace, aad, NULL, NULL);
			if (iplace != pbuf) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failed encryption/decryption in-place comparison.");
				logMessage(LOG_ERROR_LEVEL, "Inplace  : %s",
						   iplace.toString(5).c_str());
				logMessage(LOG_ERROR_LEVEL, "Expected : %s",
						   pbuf.toString(5).c_str());
				return (-1);
			}

			// Log successs
			logMessage(
				CRYPTO_LOG_LEVEL,
				"Successfully encrypted/decrypted %u bytes with key %u, %s MAC",
				lin, sa->getKey()->getKeyId(), (verify) ? "With" : "without");
			logMessage(CRYPTO_VRBLOG_LEVEL, "Plain    : %s",
					   ibuf.toString(5).c_str());
			logMessage(CRYPTO_VRBLOG_LEVEL, "Cipher B : %s",
					   obuf.toString(5).c_str());
			logMessage(CRYPTO_VRBLOG_LEVEL, "Cipher I : %s",
					   iplace.toString(5).c_str());
		}

		delete aad;

	} catch (bfsCfgParserError *e) {
		logMessage(LOG_ERROR_LEVEL,
				   "BFS crypto unit test config parse failed [%s], aborting",
				   e->getMessage().c_str());
		return (-1);
	} catch (bfsCfgError *e) {
		logMessage(LOG_ERROR_LEVEL,
				   "BFS crypto unit test config failed [%s], aborting",
				   e->getMessage().c_str());
		return (-1);
	} catch (bfsCryptoError *e) {
		logMessage(LOG_ERROR_LEVEL,
				   "BFS crypto unit test crypto failed [%s], aborting",
				   e->getMessage().c_str());
		return (-1);
	}

	// Log, return successfully
	logMessage(CRYPTO_LOG_LEVEL,
			   "Bfs crypto unit test completed successfully.");
	return (0);
}

#elif defined(__BFS_NONENCLAVE_MODE)

/**
 * @brief Tests for sgx crypto lib. Either tests against a receiver who is
 * running gcrypt (ie when sending messages to client or disk) or who is also
 * running the sgx crypto lib (ie between the fs and itself).
 *
 * @return int: 0 if success, -1 if failure
 */
int bfsCryptoLayer::bfsCryptoLayerUtest__enclave(void) {
	// Init non enclave mode crypto
	bfsCryptoLayerInit();

	// Create enclave
	sgx_launch_token_t tok = {0};
	int tok_updated = 0;
	if (sgx_create_enclave(
			(std::string(getenv("BFS_HOME")) + std::string("/build/bin/") +
			 std::string(BFS_UTIL_TEST_ENCLAVE_FILE))
				.c_str(),
			SGX_DEBUG_FLAG, &tok, &tok_updated, &eid, NULL) != SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to initialize enclave.");
		return BFS_SUCCESS;
	}

	logMessage(LOG_INFO_LEVEL, "Enclave successfully initialized.\n"
							   "Starting bfs enclave crypto test...\n");

	// Run tests
	// std::string key_str("0123456789abcdef");
	const unsigned char keydat[17] = {0x74, 0x16, 0x27, 0xc2, 0xbb, 0x8b,
									  0xe2, 0xfc, 0xa6, 0x13, 0x9d, 0x08,
									  0x9c, 0x09, 0x15, 0x9a, 0x0};
	std::string key_str((const char *)keydat);
	bfsCryptoKey key(key_str.c_str(), (bfs_size_t)key_str.size());
	bfsSecAssociation sa("testinitiator", "testresponder", &key);
	bfsFlexibleBuffer ibuf, cbuf, pbuf, iv;
	size_t lin;
	bool encrypt_with_sgx = true;
	bool decrypt_with_sgx = false;

	// test with either random data or zero-buffer (to debug sgx/grcrypt
	// semantics)
	try {
		for (int i = 0; i < CRYPTO_ENCDEC_UTEST_ITERATIONS; i++) {
			// initialize input/output buffers with appropriate size
			// lin = get_random_value(1, 32);
			// lin = 14; // 1 block long
			lin = 8;
			ibuf.resetWithAlloc((bfs_size_t)lin);
			// get_random_data(ibuf.getBuffer(), ibuf.getLength());
			// memset(ibuf.getBuffer(), 0x0, 8);

			pbuf.resetWithAlloc((bfs_size_t)16);
			cbuf.resetWithAlloc((bfs_size_t)16);

			logMessage(CRYPTO_VRBLOG_LEVEL,
					   "Testing random data encryption:\nlen=%d\nPlaintext: %s",
					   lin, ibuf.toString(5).c_str());

			int64_t ecall_status = 0, ret = 0;
			char *enc, *dec = pbuf.getBuffer();
			char *key_raw = sa.getKey()->getKeydat();
			sgx_aes_gcm_128bit_tag_t mtag;

			// for gcrypt enc/dec
			gcry_cipher_hd_t cipher;
			std::string message;
			gcry_error_t err;
			char aad[8] = {0x0};

			if (encrypt_with_sgx) {
				sa.addPKCS7Padding(ibuf);
				iv.resetWithAlloc((bfs_size_t)sa.getKey()->getIVlen());
				// get_random_data(iv.getBuffer(),
				// 				(bfs_size_t)sa.getKey()->getIVlen());
				// memset(iv.getBuffer(), 0x0, 12);

				enc = cbuf.getBuffer();

				if (((ecall_status = ecall_bfs_encrypt(
						  eid, &ret, &key_raw, iv.getBuffer(), enc,
						  ibuf.getLength(), ibuf.getBuffer(), ibuf.getLength(),
						  (uint8_t **)&mtag)) != SGX_SUCCESS) ||
					(ret != BFS_SUCCESS)) {
					logMessage(
						LOG_ERROR_LEVEL,
						"Failed during ecall_bfs_encrypt. Error code: %d\n",
						ret);
					return BFS_FAILURE;
				}
			} else {
				sa.addPKCS7Padding(ibuf);
				iv.resetWithAlloc((bfs_size_t)sa.getKey()->getIVlen());
				// get_random_data(iv.getBuffer(),
				// 				(bfs_size_t)sa.getKey()->getIVlen());
				// memset(iv.getBuffer(), 0x0, 12);
				enc = cbuf.getBuffer();

				if ((err = gcry_cipher_open(&cipher, BFS_CRYPTO_DEFAULT_CIPHER,
											BFS_CRYPTO_DEFAULT_CIPHER_MODE,
											0)) != GPG_ERR_NO_ERROR) {
					message = (string) "gcrypt failure setting up cipher: " +
							  gcry_strerror(err);
					throw new bfsCryptoError(message);
				}

				if ((err = gcry_cipher_setkey(cipher, key_raw, 16)) !=
					GPG_ERR_NO_ERROR) {
					message = (string) "gcrypt failure setting cipher key: " +
							  gcry_strerror(err);
					throw new bfsCryptoError(message);
				}

				if ((err = gcry_cipher_setiv(cipher, iv.getBuffer(), 12)) !=
					GPG_ERR_NO_ERROR) {
					message = (string) "gcrypt failure setting cipher IV: " +
							  gcry_strerror(err);
					throw new bfsCryptoError(message);
				}

				if (gcry_cipher_authenticate(cipher, aad, sizeof(aad)) !=
					GPG_ERR_NO_ERROR) {
					message =
						(string) "gcrypt add MAC AAD failure (encrypt): " +
						gcry_strerror(err);
					throw new bfsCryptoError(message);
				}

				// Encyrpt the data passed in
				if ((err = gcry_cipher_encrypt(
						 cipher, enc, ibuf.getLength(), ibuf.getBuffer(),
						 ibuf.getLength())) != GPG_ERR_NO_ERROR) {
					message = (string) "gcrypt failure on encrypt data: " +
							  gcry_strerror(err);
					throw new bfsCryptoError(message);
				}

				if ((err = gcry_cipher_gettag(cipher, mtag, 16)) !=
					GPG_ERR_NO_ERROR) {
					message = (string) "gcrypt failure reading MAC tag: " +
							  gcry_strerror(err);
					throw new bfsCryptoError(message);
				}

				// sa.encryptData(ibuf, cbuf, true); // should add the IV+MAC
				// too

				// iv.setData(cbuf.getBuffer(), 12);
				// enc = cbuf.getBuffer() + 12;

				// memcpy(mtag, enc + 16, 16);
			}

			if (decrypt_with_sgx) {
				if (((ecall_status = ecall_bfs_decrypt(
						  eid, &ret, &key_raw, iv.getBuffer(), dec,
						  ibuf.getLength(), enc, ibuf.getLength(),
						  (uint8_t **)&mtag)) != SGX_SUCCESS) ||
					(ret != BFS_SUCCESS)) {
					logMessage(
						LOG_ERROR_LEVEL,
						"Failed during ecall_bfs_decrypt. Error code: %d\n",
						ret);
					return BFS_FAILURE;
				}
			} else {
				// if it was encrypted with sgx, need to init cipher handle
				if (encrypt_with_sgx) {
					if ((err = gcry_cipher_open(&cipher,
												BFS_CRYPTO_DEFAULT_CIPHER,
												BFS_CRYPTO_DEFAULT_CIPHER_MODE,
												0)) != GPG_ERR_NO_ERROR) {
						message =
							(string) "gcrypt failure setting up cipher: " +
							gcry_strerror(err);
						throw new bfsCryptoError(message);
					}
				}

				if ((err = gcry_cipher_setkey(cipher, keydat, 16)) !=
					GPG_ERR_NO_ERROR) {
					message = (string) "gcrypt failure setting cipher key: " +
							  gcry_strerror(err);
					throw new bfsCryptoError(message);
				}

				// Set the passed IV for the decryption algorithm
				if ((err = gcry_cipher_setiv(cipher, iv.getBuffer(),
											 sa.getKey()->getIVlen())) !=
					GPG_ERR_NO_ERROR) {
					message = (string) "gcrypt failure setting cipher IV: " +
							  gcry_strerror(err);
					throw new bfsCryptoError(message);
				}

				if (gcry_cipher_authenticate(cipher, aad, sizeof(aad)) !=
					GPG_ERR_NO_ERROR) {
					message =
						(string) "gcrypt add MAC AAD failure (encrypt): " +
						gcry_strerror(err);
					throw new bfsCryptoError(message);
				}

				// Encyrpt the data passed in
				if ((err = gcry_cipher_decrypt(cipher, dec, ibuf.getLength(),
											   enc, ibuf.getLength())) !=
					GPG_ERR_NO_ERROR) {
					message = (string) "gcrypt failure on decrypt data: " +
							  gcry_strerror(err);
					throw new bfsCryptoError(message);
				}

				if ((err = gcry_cipher_checktag(cipher, mtag, 16)) !=
					GPG_ERR_NO_ERROR) {
					message = (string) "gcrypt MAC verification failure: " +
							  gcry_strerror(err);
					throw new bfsCryptoError(message);
				}

				// if ((err = gcry_cipher_gettag(cipher, mtag, 16)) !=
				// 	GPG_ERR_NO_ERROR) {
				// 	message = (string) "gcrypt failure reading MAC tag: " +
				// 			  gcry_strerror(err);
				// 	throw new bfsCryptoError(message);
				// }
			}

			// compare results
			if (ibuf != pbuf) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failed encryption/decryption compare.");
				return BFS_FAILURE;
			}

			logMessage(CRYPTO_LOG_LEVEL,
					   "Successfully encrypted (with %s) / decrypted (with "
					   "%s) %u bytes with key %u, "
					   "with MAC",
					   encrypt_with_sgx ? "sgx" : "gcrypt",
					   decrypt_with_sgx ? "sgx" : "gcrypt", lin,
					   sa.getKey()->getKeyId());
			logMessage(CRYPTO_VRBLOG_LEVEL, "Plain    : %s",
					   ibuf.toString(5).c_str());
			logMessage(CRYPTO_VRBLOG_LEVEL, "Cipher B : %s",
					   cbuf.toString(5).c_str());
		}
	} catch (bfsCryptoError *e) {
		logMessage(LOG_ERROR_LEVEL,
				   "BFS crypto unit test crypto failed [%s], aborting",
				   e->getMessage().c_str());
		return BFS_FAILURE;
	}

	sgx_status_t enclave_status = SGX_SUCCESS;
	if ((enclave_status = sgx_destroy_enclave(eid)) != SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to destroy enclave: %d\n",
				   enclave_status);
		return BFS_FAILURE;
	}

	// Log, return successfully
	logMessage(CRYPTO_LOG_LEVEL,
			   "\033[93mBfs crypto unit test completed successfully.\033[0m\n");
	return BFS_SUCCESS;
}
#endif
