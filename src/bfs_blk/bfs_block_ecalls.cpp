/**
 * @file bfs_block_ecalls.cpp
 * @brief ECall entry function definitions for bfs filesystem.
 */

#ifdef __BFS_ENCLAVE_MODE
#include "bfs_enclave_t.h" /* For ocalls */
#elif defined(__BFS_DEBUG_NO_ENCLAVE)
#include "bfs_block_ecalls.h"
#include "bfs_block_ocalls.h"
#endif

void empty_block_ecall() {} // Placeholder so enclave compiles OK (need root
							// ECALL).
