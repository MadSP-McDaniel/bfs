#ifndef RAWNET_NETWORK_INCLUDED
#define RAWNET_NETWORK_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfs_rawnet.h
//  Description   : This is the network definitions RAW network I/O in BFS.
//
//  Author        : Patrick McDaniel
//  Last Modified : Wed 10 Mar 2021 01:41:54 PM EST
//

// Include Files
#include <list>
#include <pthread.h>
#include <stdint.h>
#include <tuple>

// Defines

//
// Type Definitions

//
// Network utility functions

int rawnet_connect_server(unsigned short port);
// This function makes a server connection on a bound port.

int rawnet_accept_connection(int server);
// This function accepts an incoming connection from a client (using the server
// socket)

int rawnet_client_connect(const char *ip, unsigned short port);
// Connect a client socket for network communication.

int rawnet_send_bytes(int sock, int len, char *buf);
// Send some bytes over the network

int rawnet_read_bytes(int sock, int len, char *buf);
// Read some bytes from the network

int rawnet_wait_read(int sock);
// Wait until the socket has bytes to read

int rawnet_close(int sock);
// Close a socket associated with network communication.

//
// Unit Tests

int rawnet_server_unittest(unsigned short port);
// This is the unit test for the networking code (server).

int rawnet_client_unittest(const char *addr, unsigned short port);
// This is the unit test for the networking code (client).

#endif
