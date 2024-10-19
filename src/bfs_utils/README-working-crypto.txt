

    - flexible buffer
        X + add set data function
        X + make it so it works with empty data
    
----

  1) Create new functions

    macData( buf )
    
    verifyMac( buf )

    2) add mac boolean to the encrypt and decrypt data

        add code to call MAC at and of encrypt
        add code to call verify at beginning of decrypt

    3) Code in unit test

        make MAC option to the encrypt code, etc.

    4) Check to make sure that all sec association calls have non-NULL key

----

    MAC Implementation

    1) define default MAC algorithm [DONE]
    
        BFS_CRYPTO_DEFAULT_MAC CGRY_MAC_HMACSHA3_256

    2) add cipher handle, maclen to the key class [DONE]

#ifndef __BFS_ENCLAVE_MODE
        gcry_mac_hd_t mac;
       
    3) In setKeyData, add get MAC length and initalize mac structure [DONE]

        gcry_mac_open( &mac, BFS_CRYPTO_DEFAULT_MAC, 0, NULL )

        gcry_mac_get_algo_maclen( BFS_CRYPTO_DEFAULT_MAC ) 

        gcry_set_key( mac, BFS_CRYPTO_DEFAULT_MAC, key, len )

    4) In bool bfsCryptoKey::destroyCipher
    
        gcry_close_mac( mac )

    5) New function in KEY class [DONE]
    
        macData( char *iv, char *mac, size_t mlen, char *in, size_t ilen )

        verifyMac( char *iv, char *mac, size_t mlen, char *in, size_t ilen )

        private methods

        bool doMAC( char *iv, char *mac, size_t mlen, char *in, size_t ilen, bool verify )

        gcry_mac_reset( mac )

        gcry_setiv( mac, iv, blocksize )

        gcry_mac_write( mac, in, inlen )


        if ( verifying )

            gcry_mac_verify( mac, mac, mlen )

            GPG_ERR_CHECKSUM returned if not matching

        else {

            gcry_mac_read( mac, mac, mlen )

        }

    6) Check for initalized in all key functions. [DONE]