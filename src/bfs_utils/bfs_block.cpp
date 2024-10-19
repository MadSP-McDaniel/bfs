/**
 * @file bfs_block.cpp
 * @brief Definitions for the bfs block types.
 */

#include <bfs_block.h>

/**
 * @brief Initialize the abstract block object.
 */
BfsBlock::BfsBlock() : CacheableObject() {}

/**
 * @brief Destroy the abstract block object.
 */
BfsBlock::~BfsBlock() {}

/**
 * @brief Initialize the block object with a pointer to a buffer containing the
 * raw block data. Support two types of initialization: (1) copies the data over
 * from a source buffer; does not assume ownership of the source buffer, or (2)
 * initiailizes an empty buffer if no source buffer is given but length is
 * positive.
 *
 * @param d: buffer for block data
 */
VBfsBlock::VBfsBlock(char *dat, bfs_size_t len, bfs_size_t hsz, bfs_size_t tsz,
					 bfs_vbid_t v)
	: BfsBlock(), bfsSecureFlexibleBuffer() {
	dirty = false;

	vbid = v;

	if (!dat && (len + hsz + tsz > 0)) {
		resetWithAlloc(len, 0x0, hsz, tsz);
	} else if (dat) {
		resizeAllocation(hsz, len, tsz);
		setData(dat, len);
	}
}

/**
 * @brief Gets the virtual block id of the object.
 *
 * @return bfs_vbid_t: the virtual block id
 */
bfs_vbid_t VBfsBlock::get_vbid() const { return vbid; }

/**
 * @brief Set the virtual block id of the object.
 *
 * @param v: new id
 */
void VBfsBlock::set_vbid(bfs_vbid_t v) { vbid = v; }

/**
 * @brief Initialize the block object with a pointer to a buffer containing the
 * raw block data. Support two types of initialization: (1) copies the data over
 * from a source buffer; does not assume ownership of the source buffer, or (2)
 * initiailizes an empty buffer if no source buffer is given but length is
 * positive.
 *
 * @param d: buffer for block data
 * @param len: length of the data
 * @param p: physical block id
 * @param r: back pointer to the remote device object
 */
PBfsBlock::PBfsBlock(char *dat, bfs_size_t len, bfs_size_t hsz, bfs_size_t tsz,
					 bfs_block_id_t p, void *r) {
	dirty = false;

	pbid = p;
	rd = r;

	if (!dat && (len + hsz + tsz > 0)) {
		resetWithAlloc(len, 0x0, hsz, tsz);
	} else if (dat) {
		resizeAllocation(hsz, len, tsz);
		setData(dat, len);
	}
}

/**
 * @brief Gets the physical block id of the object.
 *
 * @return bfs_vbid_t: the physical block id
 */
bfs_block_id_t PBfsBlock::get_pbid() const { return pbid; }

/**
 * @brief Set the physical block id of the object.
 *
 * @param p: new id
 */
void PBfsBlock::set_pbid(bfs_block_id_t p) { pbid = p; }

/**
 * @brief Get the pointer to the remote device storing the physical block
 * object.
 *
 * @return void *: pointer to the remote device object
 */
void *PBfsBlock::get_rd() const { return rd; }

void PBfsBlock::set_rd(void *_rd) { rd = _rd; }
