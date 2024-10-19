/**
 * @file bfs_device_ocalls.h
 * @brief Ocall interface.
 */

#ifndef BFS_DEVICE_OCALLS_H
#define BFS_DEVICE_OCALLS_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/* Initializes the mmap for disk storage on a file indicated by the given name,
 * returning the untrusted base address of the mmap */
uint64_t ocall_createDiskStorage(uint32_t, uint32_t, const char *, uint64_t);

/* Deinitializes the mmap for disk storage. */
void ocall_deleteDiskStorage(uint64_t, uint64_t);

#ifdef __cplusplus
}
#endif

#endif /* BFS_DEVICE_OCALLS_H */
