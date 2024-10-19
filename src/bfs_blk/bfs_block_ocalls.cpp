/**
 * @file bfs_block_ocalls.cpp
 * @brief Provides support for the bfs block enclave code.
 */

#include "bfs_block_ocalls.h"
#include "bfsBlockLayer.h"
#include <bfs_common.h>

// int ocall_readBlock(uint64_t vbid, void *blk_buf) {
// 	// execute the request by calling into the cluster object from non-TEE
// 	// context
// 	return bfsBlockLayer::get_vbc()->readBlock_helper(vbid, (char *)blk_buf);
// }

// int ocall_writeBlock(uint64_t vbid, void *blk_buf, int flags) {
// 	// execute the request by calling into the cluster object from non-TEE
// 	// context
// 	return bfsBlockLayer::get_vbc()->writeBlock_helper(vbid, (char *)blk_buf,
// 													   (op_flags_t)flags);
// }
