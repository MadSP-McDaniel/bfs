/**
 * @file bfs_server_ocalls.h
 * @brief Ocall interface for bfs server.
 */

#ifndef BFS_SERVER_OCALLS_H
#define BFS_SERVER_OCALLS_H

#include <stdint.h>

#include <bfsFlexibleBuffer.h>
#include <bfsUtilError.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Handles sending fops messages to the client originating from the enclave */
int32_t ocall_handle_out_msg(void *, uint32_t, char *);

#ifdef __cplusplus
}
#endif

#endif /* BFS_SERVER_OCALLS_H */
