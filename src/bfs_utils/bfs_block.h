/**
 * @file bfs_block.h
 * @brief Types and interface for a raw bfs data block object.
 */

#ifndef BFS_BLOCK_H
#define BFS_BLOCK_H

#include <map>
#include <vector>

#include <bfsFlexibleBuffer.h>
#include <bfs_cache.h>
#include <bfs_common.h>

/**
 * @brief This class represents a block object in bfs. It is what will be
 * referenced by the file system layer to execute file operations, and by the
 * block layer for block management operations.
 */
class BfsBlock : public CacheableObject {
public:
	BfsBlock();
	virtual ~BfsBlock();
};

/**
 * @brief This class represents a virtual block used by the file system layer.
 * Since the file system layer operates entirely in the enclave (secure memory),
 * the raw data buffer should be a secure buffer also.
 */
class VBfsBlock : public BfsBlock, public bfsSecureFlexibleBuffer {
public:
	VBfsBlock(char *, bfs_size_t, bfs_size_t, bfs_size_t, bfs_vbid_t);
	~VBfsBlock() override {}

	/* get the virtual block id */
	bfs_vbid_t get_vbid() const;

	/* set the virtual block id */
	void set_vbid(bfs_vbid_t);

private:
	bfs_vbid_t vbid; /* the virtual block id */
};

/**
 * @brief This class represents a physical block used by the block layer.
 * Physical blocks reside outside of the enclave (in untrusted memory) and so
 * the raw data buffer should as well. Generally just using wrappers with these
 * for now, until we add additional services to the block layer (e.g.,
 * replication, sharing encrypted virtual blocks).
 */
class PBfsBlock : public BfsBlock, public bfsFlexibleBuffer {
public:
	PBfsBlock(char *dat, bfs_size_t len, bfs_size_t hsz, bfs_size_t tsz, bfs_block_id_t,
			  void *);
	~PBfsBlock() override {}

	/* get the physical block id */
	bfs_block_id_t get_pbid() const;

	/* set the physical block id */
	void set_pbid(bfs_block_id_t);

	void *get_rd() const;

	void set_rd(void *);

private:
	bfs_block_id_t pbid; /* the physical block id */
	void *rd;			 /* back-ptr to remote dev where the blk is located */
};

// Block conmtainer types
typedef vector<bfs_block_id_t> bfs_blockid_list_t;		   /* Block ID List */
typedef map<bfs_block_id_t, PBfsBlock *> bfs_block_list_t; /* phys blk array */
typedef map<bfs_vbid_t, VBfsBlock *> bfs_vblock_list_t;	   /* virt blk array */

#endif /* BFS_BLOCK_H */
