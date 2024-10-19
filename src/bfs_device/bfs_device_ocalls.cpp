/**
 * @file bfs_device_ocalls.cpp
 * @brief Provides support for the bfs device enclave code.
 */

#include "bfs_device_ocalls.h"
#include "bfsDeviceStorage.h"

/**
 * @brief Initializes the mmap for disk storage on a file indicated by the given
 * name, returning the untrusted base address of the mmap. Expects a pointer to
 * the device storage object in enclave memory.
 *
 * @param did: the device id to initialize for
 * @param plen: length of the path string for storage
 * @param storagePath_str: buffer containing the storage path string
 * @param numBlocks: the size of the memory map
 * @return uint64_t: pointer to the base address of the block storage memory map
 */
uint64_t ocall_createDiskStorage(uint32_t did, uint32_t plen,
								 const char *storagePath_str,
								 uint64_t numBlocks) {
	(void)plen;
	return (uint64_t)__createDiskStorage(did, storagePath_str, numBlocks);
}

/**
 * @brief Deinitializes the mmap for disk storage. Expects a pointer to
 * the device storage object in enclave memory and the size of the region to
 * unmap.
 *
 * @param blockStorage: pointer to the memory map region
 * @param numBlocks: number of device blocks to unmap
 */
void ocall_deleteDiskStorage(uint64_t blockStorage, uint64_t numBlocks) {
	__deleteDiskStorage((char *)blockStorage, numBlocks);
}
