/**
 * @file bfs_util_ecalls.h
 * @brief ECall interface, types, and macros for the bfs utils. Mainly used for
 * crypto unit tests.
 */

#ifndef BFS_UTIL_ECALLS_H
#define BFS_UTIL_ECALLS_H

#include "bfs_common.h"
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t ecall_bfs_encrypt(char **, char *, char *, bfs_size_t, char *,
						  bfs_size_t, uint8_t **);
int64_t ecall_bfs_decrypt(char **, char *, char *, bfs_size_t, char *,
						  bfs_size_t, uint8_t **);
void empty_util_ecall();

#ifdef __cplusplus
}
#endif

#endif /* BFS_UTIL_ECALLS_H */
