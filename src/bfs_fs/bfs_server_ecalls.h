/**
 * @file bfs_server_ecalls.h
 * @brief ECall interface, types, and macros for the bfs server.
 */

#ifndef BFS_SERVER_ECALLS_H
#define BFS_SERVER_ECALLS_H

#include <stdint.h>

#include <bfsFlexibleBuffer.h>
#include <bfsUtilError.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Handles sending messages originating from the client to the enclave */
int64_t ecall_bfs_handle_in_msg(void *, void *);

#ifdef __cplusplus
}
#endif

#endif /* BFS_SERVER_ECALLS_H */
