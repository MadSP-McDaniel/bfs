/**
 * @file bfs_common.h
 * @brief Common interfaces, types, and macros for the bfs layers, for both bfs
 * nonenclave code and enclave code.
 */

#ifndef BFS_COMMON_H
#define BFS_COMMON_H

// STL-isms

// Includes
#include <stdint.h>

// Definitions
#define BFS_CORE_ENCLAVE_FILE "libbfs_core_enclave.signed.so"
#define BFS_CORE_TEST_ENCLAVE_FILE "libbfs_core_test_enclave.signed.so"

#define BFS_COMMON_CONFIG "bfsCommon"

#define BLK_SZ 4096
#define BLK_SZ_BITS (BLK_SZ * 8)

#define BFS_IV_LEN BfsFsLayer::get_SA()->getKey()->getIVlen()
#define BFS_MAC_LEN (BfsFsLayer::get_SA()->getKey()->getMACsize())
#define BLK_META_BLK_LOC(b) (b / (BLK_SZ / (BFS_IV_LEN + BFS_MAC_LEN)))
#define BLK_META_BLK_IDX_LOC(b) (b % (BLK_SZ / (BFS_IV_LEN + BFS_MAC_LEN)))

/* for lwext4 */
// #define BFS_LWEXT4_NUM_BLKS ((uint64_t)262144)
#define BFS_LWEXT4_NUM_BLKS bfsBlockLayer::get_num_blocks()
#define BFS_LWEXT_MT_ROOT_BLK_NUM BFS_LWEXT4_NUM_BLKS
#define BFS_LWEXT_META_START_BLK_NUM (BFS_LWEXT_MT_ROOT_BLK_NUM + 1)
// similar to NUM_META_BLOCKS
#define BFS_LWEXT4_META_SPC                                                    \
	((bfs_vbid_t)(                                                             \
		 (BFS_LWEXT4_NUM_BLKS / (BLK_SZ / (BFS_IV_LEN + BFS_MAC_LEN)))) +      \
	 1 + 1) // add space for last blk and for mt root block

#define PKCS_PAD_SZ 1
#define UNUSED_PAD_SZ 4
#define PAD_SZ                                                                 \
	(PKCS_PAD_SZ + UNUSED_PAD_SZ) // 2 bytes for pkcs padding + 4 unused bytes
#define EFF_BLK_SZ                                                             \
	(4096 - 12 - 16 - PAD_SZ) // The encrypted buffers should contain the IV (12
// bytes) + data (4062) + 2 byte pkcs padding + MAC (16
// bytes). 6=2 pkcs pad bytes+4 unused bytes
#define EFF_BLK_SZ_BITS (EFF_BLK_SZ * 8)

/* Based on device geometry (hardcoded for now) */
#define BFS_SB_MAGIC 0xABCDABCDABCDABCD /* for detecting a formatted fs */
#define NUM_BLOCKS bfsBlockLayer::get_vbc()->getMaxVertBlocNum()
#define NUM_INODES ((uint32_t)100e3) /* TODO: figure out how ext4 computes */

// global macros for other reserved blocks
#define SB_REL_START_BLK_NUM ((bfs_vbid_t)0) /* rel start blk (eg in group) */
#define MT_REL_START_BLK_NUM ((bfs_vbid_t)SB_REL_START_BLK_NUM + 1)

#define BFS_SERVER_MAX_MSG_BUF ((uint32_t)10e6)

#define MAX_PATH_LEN 1024
#define MAX_FILE_NAME_LEN 255 /* from EXT4_NAME_LEN; used for dentry sizing */
#define MAX_OPEN_FILES ((uint64_t)1e6) // mainly used for normal bfs fs code
#define START_FD 3

/* For logging. Taken from glibc unistd.h */
#ifdef __BFS_ENCLAVE_MODE
#define STDIN_FILENO 0	/* Standard input.  */
#define STDOUT_FILENO 1 /* Standard output.  */
#define STDERR_FILENO 2 /* Standard error output.  */
#endif

/* File system state */
#define UNINITIALIZED 0
#define INITIALIZED 1
#define FORMATTING 2
#define FORMATTED 3
#define MOUNTED 4
#define CORRUPTED 5

typedef uint32_t bfs_size_t;	   /* A universal size type for BFS */
typedef uint32_t bfs_uid_t;		   /* User identifier */
typedef uint32_t bfs_device_id_t;  /* Device identifier */
typedef uint64_t bfs_block_id_t;   /* Physical block ID used by block layer */
typedef bfs_block_id_t bfs_vbid_t; /* Virtual block ID used by filesystem */
typedef uint64_t bfs_ino_id_t;	   /* Inode ID used by filesystem */
typedef uint64_t bfs_fh_t;		   /* Open file handle; 64-bit to match FUSE */

typedef enum _op_flags_t {
	_bfs__NONE = 0,
	_bfs__O_ASYNC, /*  Tell backing store the request is async */
	_bfs__O_SYNC,  /*  Tell backing store to flush the request to bdev */
} op_flags_t;

typedef enum _open_flags_t {
	_bfs__O_APPEND = 0,
} open_flags_t;

typedef enum _msg_type_t {
	INVALID_MSG = -1,
	FROM_SERVER,
	TO_SERVER,
} msg_type_t;

typedef enum _op_type_t {
	INVALID_OP = -1,
	CLIENT_GETATTR_OP,
	CLIENT_MKDIR_OP,
	CLIENT_UNLINK_OP,
	CLIENT_RMDIR_OP,
	CLIENT_RENAME_OP,
	CLIENT_OPEN_OP,
	CLIENT_READ_OP,
	CLIENT_WRITE_OP,
	CLIENT_RELEASE_OP,
	CLIENT_FSYNC_OP,
	CLIENT_OPENDIR_OP,
	CLIENT_READDIR_OP,
	CLIENT_INIT_OP,		 // init (mount)
	CLIENT_INIT_MKFS_OP, // init (mount) + mkfs
	CLIENT_DESTROY_OP,
	CLIENT_CREATE_OP,
	CLIENT_CHMOD_OP,
    CLIENT_TRUNCATE_OP,
} op_type_t;

typedef enum {
	BFS_SHUTDOWN = -2,	   /* indicates to begin graceful shutdown */
	BFS_FAILURE = -1,	   /*  generic BFS failure */
	BFS_SUCCESS,		   /*  generic BFS success */
	BFS_SUCCESS_CACHE_HIT, /* success; requested item found in cache */
	ERR_GETATTR_FAILED,	   /*  failure during read attributes */
	ERR_READDIR_FAILED,	   /*  failure during read directory entries */
	ERR_MKDIR_FAILED,	   /*  failure during make new directory */
	ERR_RMDIR_FAILED,	   /*  failure during remove directory */
	ERR_UNLINK_FAILED,	   /*  failure during delete (unlink) file */
	ERR_RENAME_FAILED,	   /*  failure during rename file */
	ERR_OPEN_FAILED,	   /*  failure during open file */
	ERR_CREATE_FAILED,	   /*  failure during create file */
	ERR_READ_FAILED,	   /*  failure during read file contents */
	ERR_WRITE_FAILED,	   /*  failure during write file contents */
	ERR_RELEASE_FAILED,	   /*  failure during close (release) file */
	ERR_DESTROY_FAILED,	   /*  failure during file system destroy */
	ERR_INIT_FAILED,	   /*  failure during file system intialization */
} status_code_t;

typedef struct merkle_tree_node {
	uint8_t *hash;
} merkle_tree_node_t;

typedef struct merkle_tree {
	bfs_vbid_t n;	   // number of elements in the data structure (eg blocks)
	bfs_vbid_t height; // height of tree
	bfs_vbid_t num_nodes;	   // number of nodes in the tree
	merkle_tree_node_t *nodes; // node list
	int status;
} merkle_tree_t;

#endif /* BFS_COMMON_H */
