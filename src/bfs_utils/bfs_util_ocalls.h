/**
 * @file bfs_util_ocalls.h
 * @brief Ocall interface for utility functions.
 */

#ifndef BFS_UTILS_OCALLS_H
#define BFS_UTILS_OCALLS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Handles printing log messages originating from the enclave */
int64_t ocall_printf(int32_t, uint32_t, char *);
int64_t ocall_getTime(uint64_t, char *);
double ocall_get_time2();
int ocall_write_to_file(uint32_t, const char *, uint32_t, const char *);
int32_t ocall_openLog(char *, uint32_t);
int64_t ocall_closeLog(int32_t);
int64_t ocall_do_alloc(uint32_t);
void ocall_delete_allocation(int64_t);

#ifdef __cplusplus
}
#endif

#endif /* BFS_UTILS_OCALLS_H */
