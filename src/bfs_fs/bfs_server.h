/**
 * @file bfs_server.h
 * @brief Public interface for the server subsystem.
 */

#ifndef BFS_SERVER_H
#define BFS_SERVER_H

#include <bfsConnectionMux.h>
#include <chrono>
#include <list>
#include <pthread.h>
#include <stdint.h>

#define SERVER_LOG_LEVEL bfs_server_log_level
#define SERVER_VRB_LOG_LEVEL bfs_server_vrb_log_level
#define BFS_SERVER_CONFIG "bfsServer"

/* For performance testing */
extern double net_c_send_start_time, net_c_send_end_time;

/* Starts the main server event loop */
int start_server();

/* Initializes the server subsystem */
int server_init();

#endif /* BFS_SERVER_H */
