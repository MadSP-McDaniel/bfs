/**
 * @file bfs_device_ecalls.cpp
 * @brief ECall function definitions for the device subsystem.
 */

#ifdef __BFS_ENCLAVE_MODE
#include "bfs_enclave_t.h" /* For ocalls */
#elif defined(__BFS_DEBUG_NO_ENCLAVE)
#include "bfs_device_ecalls.h"
#include "bfs_device_ocalls.h"
#endif

void empty_device_ecall() {} // Placeholder so enclave compiles OK (need root
							 // ECALL).
