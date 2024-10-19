/**
 * @file bfs_comms_ocalls.cpp
 * @brief Provides support for the enclave code to use comms layer. Mainly used
 * for defining the enclave->device interface for ocalls.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <string.h>

#include "bfsConnectionMux.h"
#include "bfsNetworkConnection.h"
#include "bfs_common.h"
#include "bfs_comms_ocalls.h"
#include "bfs_log.h"

int32_t ocall_rawnet_connect_server(unsigned short port) {
  return rawnet_connect_server(port);
}

int32_t ocall_rawnet_client_connect(const char *chAddress, uint32_t addr_len,
                                    unsigned short port) {
  (void)addr_len;
  return rawnet_client_connect(chAddress, port);
}

int32_t ocall_rawnet_close(int socket) { return rawnet_close(socket); }

int32_t ocall_rawnet_send_bytes(int socket, uint32_t len, char *buf) {
  // fine for short/long packetized
  return rawnet_send_bytes(socket, len, buf);
}

int32_t ocall_rawnet_read_bytes(int socket, uint32_t len, char *buf) {
  // fine for short/long packetized
  return rawnet_read_bytes(socket, len, buf);
}

int32_t ocall_sendPacketizedDataHdrL(int socket, uint32_t len) {
  uint32_t slen = htonl(len);
  return rawnet_send_bytes(socket, sizeof(uint32_t), (char *)&slen);
}

uint32_t ocall_recvPacketizedDataHdrL(int socket) {
  uint32_t slen = 0;
  uint32_t ret = rawnet_read_bytes(socket, sizeof(uint32_t), (char *)&slen);
  if (ret <= 0)
    return ret;

  slen = ntohl(slen);
  return slen;
}

int32_t ocall_rawnet_accept_connection(int socket) {
  return rawnet_accept_connection(socket);
}

int32_t ocall_waitConnections(uint16_t wt, uint64_t socks_count,
                              int32_t *all_socks, uint32_t *ready_cnt) {
  // Local variables
  fd_set rfds;
  int nfds, ret;
  // bfsConnectionList::iterator it;
  struct timeval wait;

  // Setup and perform the select
  FD_ZERO(&rfds);
  nfds = 0;
  for (uint32_t i = 0; i < socks_count; i++) {
    if (all_socks[i] >= nfds) { // compares the socket
      nfds = all_socks[i] + 1;
    }
    FD_SET(all_socks[i], &rfds);
  }

  // Do the select, wait or not
  if (wt > 0) {
    wait.tv_sec = (int)wt / 1000;
    wait.tv_usec = (wt % 1000) * 1000;
    ret = select(nfds, &rfds, NULL, NULL, &wait);
  } else {
    ret = select(nfds, &rfds, NULL, NULL, NULL);
  }

  // Check the return value
  if (ret == -1) {
    logMessage(LOG_ERROR_LEVEL, "ocall_waitConnections failed: [%s]",
               strerror(errno));
    return ret;
  }

  // replace the ones in the beginning as we loop
  uint32_t num_ready = 0;
  for (uint32_t i = 0; i < socks_count; i++) {
    if (FD_ISSET(all_socks[i], &rfds)) {
      all_socks[num_ready] = all_socks[i];
      num_ready++;
    }
  }
  *ready_cnt = num_ready;

  // clear out the rest to be safe
  for (uint32_t i = num_ready; i < socks_count; i++) {
    all_socks[i] = 0;
  }

  return ret;
}
