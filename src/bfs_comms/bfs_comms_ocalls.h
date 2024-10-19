/**
 * @file bfs_comms_ocalls.h
 * @brief Ocall interface.
 */

#ifndef BFS_COMMS_OCALLS_H
#define BFS_COMMS_OCALLS_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

int32_t ocall_rawnet_connect_server(unsigned short);
int32_t ocall_rawnet_client_connect(const char *, uint32_t, unsigned short);
int32_t ocall_rawnet_close(int);
int32_t ocall_rawnet_send_bytes(int, uint32_t, char *);
int32_t ocall_rawnet_read_bytes(int, uint32_t, char *);

int32_t ocall_sendPacketizedDataHdrL(int, uint32_t);
uint32_t ocall_recvPacketizedDataHdrL(int);

int32_t ocall_rawnet_accept_connection(int);
int32_t ocall_waitConnections(uint16_t, uint64_t, int32_t *, uint32_t *);

#ifdef __cplusplus
}
#endif

#endif /* BFS_COMMS_OCALLS_H */
