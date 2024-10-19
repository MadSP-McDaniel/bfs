/**
 * @file bfs_fs_layer.h
 * @brief Interface and types for the shared fs layer.
 */

#ifndef BFS_FS_LAYER_H
#define BFS_FS_LAYER_H

#include <cstdint>

#include <bfsBlockLayer.h>
#include <bfsSecAssociation.h>

#define FS_LOG_LEVEL BfsFsLayer::getFsLayerLogLevel()
#define FS_VRB_LOG_LEVEL BfsFsLayer::getVerboseFsLayerLogLevel()
#define BFS_FS_LAYER_CONFIG "bfsFsLayer"

class BfsFsLayer {
public:
	/* Initialize the fs subsystem */
	static int bfsFsLayerInit(void);

	/* Check if the fs layer is initialized */
	static bool initialized(void);

	static unsigned long getFsLayerLogLevel(void);
	static unsigned long getVerboseFsLayerLogLevel(void);
	static bool use_lwext4(void);

	/* merkle tree helper functions */
	static int
	init_merkle_tree(bool initial = false); // read mt from disk into mem
	static int flush_merkle_tree(
		void); // flush in-mem mt to disk (ie compute+save root hash)
	static int hash_node(bfs_vbid_t, uint8_t *);
	static int save_root_hash(void);
	static int read_blk_meta(bfs_vbid_t, uint8_t **, uint8_t **,
							 bool root = false);
	static int write_blk_meta(bfs_vbid_t, uint8_t **, uint8_t **,
							  bool root = false);
	static int read_block_helper(VBfsBlock &);
	static int write_block_helper(VBfsBlock &);

	/* Get the security context of the fs layer */
	static bfsSecAssociation *get_SA();
	static const merkle_tree_t &get_mt();

private:
	BfsFsLayer(void) {}
	~BfsFsLayer() { delete secContext; }

	static unsigned long bfs_core_log_level;
	static unsigned long bfs_vrb_core_log_level;

	/* Flag indicating if the fs layer is initialized */
	static bool bfsFsLayerInitialized;

	/* The security context between the enclave and itself.
	 * Used for en/de/crypting fs blocks. */
	static bfsSecAssociation *secContext;

	/* merkle tree for tracking integrity of vbc */
	static merkle_tree_t mt;

	/* Flag for switching between bfs and lwext4 fs implementations */
	static bool use_lwext4_impl;
};

#endif /* BFS_FS_LAYER_H */
