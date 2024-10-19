#ifndef BFS_CRYPTO_LAYER_INCLUDED
#define BFS_CRYPTO_LAYER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfsCryptoLayer.h
//  Description   : This is the class describing crypto layer interface
//                  for the bfs file system.  Note that this is static class
//                  in which there are no objects.
//
//  Author  : Patrick McDaniel
//  Created : Wed 28 Apr 2021 07:37:03 AM EDT
//


// Includes
#ifdef __BFS_ENCLAVE_MODE
#include "sgx_tcrypto.h"
#include "sgx_trts.h"
#else
#include <gcrypt.h>
#endif

// STL-isms

// Project Includes

//
// Class definitions
#define CRYPTO_LOG_LEVEL bfsCryptoLayer::getCryptoLayerLogLevel()
#define CRYPTO_VRBLOG_LEVEL bfsCryptoLayer::getVerboseCryptoLayerLogLevel()
#define BFS_CRYPTLYR_CONFIG "bfsCryptoLayer"

#define BFS_CRYPTO_DEFAULT_IV_LEN 12 // for aes128-gcm

#ifndef __BFS_ENCLAVE_MODE
#define BFS_CRYPTO_DEFAULT_CIPHER GCRY_CIPHER_AES128 // use 128-bit key since sgx crypto only supports this length
#define BFS_CRYPTO_DEFAULT_CIPHER_MODE GCRY_CIPHER_MODE_GCM
#else
#define BFS_CRYPTO_DEFAULT_BLK_SZ 16 // 128-bit blocks
#endif

#define CRYPTO_UTEST_NUMBER_SAS 10
#define CRYPTO_ENCDEC_UTEST_ITERATIONS 10

//
// Class Definition

class bfsCryptoLayer {

public:

	//
	// Static methods

	static int bfsCryptoLayerInit( void );
	  // Initialize the crypto layer 

	static int bfsCryptoLayerUtest( void );
    static int bfsCryptoLayerUtest__enclave( void );
	  // Perform a unit test on the crypto implementation

	//
	// Static Class Variables

	// Layer log level
	static unsigned long getCryptoLayerLogLevel( void ) {
		return( bfsCryptoLogLevel );
	}

	// Verbose log level
	static unsigned long getVerboseCryptoLayerLogLevel( void ) {
		return( bfsVerboseCryptoLogLevel );
	}

private:

	//
    // Private class methods

	bfsCryptoLayer( void ) {}
	  // Default constructor (prevents creation of any instance)

	//
	// Static Class Variables

	static unsigned long bfsCryptoLogLevel;
	  // The log level for all of the crypto information

	static unsigned long bfsVerboseCryptoLogLevel;
	  // The log level for all of the crypto information

	static bool bfsCryptoLayerInitialized;
      // The flag indicating the crypto layer is initialized

};

#endif
