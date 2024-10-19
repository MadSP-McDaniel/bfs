/**
 * @file bfs_core_test_ecalls.h
 * @brief ECall interface, types, and macros for the bfs core unit tests.
 */

#ifndef BFS_CORE_TEST_ECALLS_H
#define BFS_CORE_TEST_ECALLS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ecall_bfs_enclave_init(int type);
int ecall_bfs_start_core_blk_test(uint64_t);
int ecall_bfs_start_core_file_test_rand(uint64_t, uint64_t, uint64_t, uint64_t);
int ecall_bfs_start_core_file_test_simple(int, uint64_t, uint64_t, uint64_t);

#ifdef __cplusplus
}
#endif

#endif /* BFS_CORE_TEST_ECALLS_H */
