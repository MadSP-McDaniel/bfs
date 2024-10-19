/**
 * @file enclave.cpp
 * @brief ECall entry function definitions for bfs filesystem.
 */

#ifdef __BFS_ENCLAVE_MODE
#include "bfs_enclave_t.h" /* For ocalls */
#include "sgx_tcrypto.h"
#include "sgx_trts.h" /* For generating random numbers */
#include <cstdint>

#include "bfs_log.h"

int64_t ecall_bfs_encrypt(char **key, char *iv, char *out, bfs_size_t olen,
						  char *in, bfs_size_t ilen, uint8_t **mtag) {
	sgx_status_t err;
	(void)olen;

	char aad[8] = {0x0};

	if ((err = sgx_rijndael128GCM_encrypt(
			 (sgx_aes_gcm_128bit_key_t *)*key, (const uint8_t *)in, ilen,
			 (uint8_t *)out, (const uint8_t *)iv, 12, (const uint8_t *)aad,
			 sizeof(aad), (sgx_aes_gcm_128bit_tag_t *)mtag)) != SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "tcrypto failure during encryption: %d",
				   err);
		return BFS_FAILURE;
	}

	return BFS_SUCCESS;
}

int64_t ecall_bfs_decrypt(char **key, char *iv, char *out, bfs_size_t olen,
						  char *in, bfs_size_t ilen, uint8_t **mtag) {
	sgx_status_t err;
	(void)olen;

	char aad[8] = {0x0};

	if ((err = sgx_rijndael128GCM_decrypt(
			 (sgx_aes_gcm_128bit_key_t *)*key, (const uint8_t *)in, ilen,
			 (uint8_t *)out, (const uint8_t *)iv, 12, (const uint8_t *)aad,
			 sizeof(aad), (const sgx_aes_gcm_128bit_tag_t *)mtag)) !=
		SGX_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "tcrypto failure during decryption: %d",
				   err);
		return BFS_FAILURE;
	}

	return BFS_SUCCESS;
}

/**
 * @brief Empty ecall for enclave ecall latency testing.
 *
 */
void empty_util_ecall() { return; }
#endif
