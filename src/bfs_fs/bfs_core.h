/**
 * @file bfs_core.h
 * @brief The interface and types for the core file system layer. This
 * includes declarations for BfsHandle, SuperBlock, IBitMap, Inode, DirEntry,
 * IndirectBlock, OpenFile, and error class types that the server enclave will
 * invoke to execute client requests.
 */

#ifndef BFS_CORE_H
#define BFS_CORE_H

#include <list>
#include <pthread.h>
#include <unordered_map>
#include <vector>

#include "bfs_usr.h"
#include <bfsVertBlockCluster.h>
#include <bfs_cache.h>
#include <bfs_common.h>

/*

						Bfs Layout
|-------------|----------------|----------------|---------------|---------------|
| superblock  |  ibitmap  ...  | itable   ...   | metadata  ... | dblocks  ... |
|-------------|----------------|----------------|---------------|---------------|

*/

/* Reserved inodes */
#define NULL_INO 0			   /* for NULL'ing inode object in cache/itab/de */
#define RESERVED_INO1 1		   /* unused for now */
#define ROOT_INO 2			   /* root inode number for bfs */
#define BLOCK_GRP_DESC_INO 3   /* unused for now */
#define IBITMAP_INO 4		   /* used to get ibitmap */
#define DBITMAP_INO 5		   /* unused for now */
#define ITABLE_INO 6		   /* used to get raw itable data */
#define JOURNAL_INO 7		   /* used to get journal inode */
#define FIRST_UNRESERVED_INO 8 /* first inode number able to be allocated */

/* BFS specific design */
#define NUM_DIRECT_BLOCKS 12 /* num data blk addrs in inode before indirect */
#define NUM_INODE_IBLKS                                                        \
	((bfs_vbid_t)(NUM_DIRECT_BLOCKS + 1)) /* see ext4 in/direct addressing */

#define SB_SZ BLK_SZ
#define NUM_IBITMAP_BLOCKS ((bfs_vbid_t)((NUM_INODES - 1) / BLK_SZ_BITS + 1))
#define INODE_SZ 256
#define NUM_INODES_PER_BLOCK ((uint32_t)(BLK_SZ / INODE_SZ))
#define NUM_UNRES_INODES ((bfs_ino_id_t)(NUM_INODES - FIRST_UNRESERVED_INO))
/* note that we adjust divisor since inode table blocks have unused space */
#define NUM_ITAB_BLOCKS                                                        \
	((bfs_vbid_t)((NUM_INODES - 1) / NUM_INODES_PER_BLOCK + 1))
#define NUM_META_BLOCKS                                                        \
	((bfs_vbid_t)((NUM_BLOCKS / (BLK_SZ / (BFS_IV_LEN + BFS_MAC_LEN)))) + 1)
#define NUM_DATA_BLOCKS                                                        \
	((bfs_vbid_t)(NUM_BLOCKS - NUM_IBITMAP_BLOCKS - NUM_ITAB_BLOCKS -          \
				  NUM_META_BLOCKS - 1 -                                        \
				  1)) // dont count superblock or mt as data block

#define DIRENT_SZ (MAX_FILE_NAME_LEN + sizeof(bfs_ino_id_t))
#define NUM_DIRENTS_PER_BLOCK ((uint32_t)(BLK_SZ / DIRENT_SZ))
#define NUM_BLKS_PER_IB (BLK_SZ / sizeof(bfs_vbid_t)) /* num blk ids in ib */

#define IBM_REL_START_BLK_NUM ((bfs_vbid_t)MT_REL_START_BLK_NUM + 1)
#define ITAB_REL_START_BLK_NUM                                                 \
	((bfs_vbid_t)(IBM_REL_START_BLK_NUM + NUM_IBITMAP_BLOCKS))
#define METADATA_REL_START_BLK_NUM                                             \
	((bfs_vbid_t)(ITAB_REL_START_BLK_NUM + NUM_ITAB_BLOCKS))
#define DATA_REL_START_BLK_NUM                                                 \
	((bfs_vbid_t)(METADATA_REL_START_BLK_NUM + NUM_META_BLOCKS))

/* For indexing into bitmap, itable, and i_block (file relative) arrays  */
#define IBM_ABSOLUTE_BLK_LOC(ino) (IBM_REL_START_BLK_NUM + ino / BLK_SZ_BITS)
/* note that there is some unused space in every block since inodes are fixed
 * 256, so we adjust the divisor for itab blks, but data blocks use all space */
#define ITAB_ABSOLUTE_BLK_LOC(ino)                                             \
	(ITAB_REL_START_BLK_NUM + ino / NUM_INODES_PER_BLOCK)
#define DATA_ABSOLUTE_BLK_IDX(offset) (offset / BLK_SZ)

#define ITAB_ABSOLUTE_BLK_OFF(ino) ((ino % NUM_INODES_PER_BLOCK) * INODE_SZ)
#define DENTRY_ABSOLUTE_BLK_OFF(idx) (idx * DIRENT_SZ)

/* File types. Taken directly from bits/stat.h (sys/stat.h) for the enclave
 * since sys/stat not available in sdk; need to ensure it matches if using
 * equivalent macros in untrusted application code */
#define BFS__S_IFMT 0170000	 /* These bits determine file type.  */
#define BFS__S_IFDIR 0040000 /* Directory.  */
#define BFS__S_IFREG 0100000 /* Regular file.  */
#define BFS__S_ISDIR(mode) ((mode & BFS__S_IFMT) == BFS__S_IFDIR)
#define BFS__S_ISREG(mode) ((mode & BFS__S_IFMT) == BFS__S_IFREG)

/* Block types */
#define SUPER_BLK_TYPE 0
#define IBM_BLK_TYPE 1
#define ITAB_BLK_TYPE 2
#define RAW_DATA_BLK_TYPE 3
#define INDIRECT_BLK_TYPE 4

/**
 * @brief This structure is at a fixed location on-bdev and is used to find and
 * access the rest of the file system data and metadata.
 */
class SuperBlock {
public:
	SuperBlock();
	~SuperBlock();

	/* get the root inode number */
	bfs_ino_id_t get_root_ino();

	/* get the number of free inodes */
	bfs_ino_id_t get_no_inodes_free();

	/* get the total number of inodes */
	bfs_ino_id_t get_no_inodes();

	/* save the magic value to identify the start of a bfs partition */
	void set_magic(uint64_t);

	/* save the core bfs superblock parameters */
	void set_sb_params(uint32_t, uint32_t, bfs_vbid_t, bfs_vbid_t, bfs_ino_id_t,
					   bfs_vbid_t, bfs_ino_id_t, bfs_vbid_t);

	/* save the number of free inodes */
	void set_no_inodes_free(bfs_ino_id_t);

	/* save the special inode numbers */
	void set_reserved_inos(bfs_ino_id_t, bfs_ino_id_t, bfs_ino_id_t,
						   bfs_ino_id_t, bfs_ino_id_t);

	/* save the current state of the system (sync'd with BfsHandle::status) */
	void set_state(uint32_t);

	/* check if the superblock is dirty and needs to be sync'd with bdev */
	bool is_dirty();

	/* serialize into a format suitable to be put on bdev */
	int64_t serialize(VBfsBlock &, uint64_t);

	/* deserialize from an on-bdev format to in-memory structures */
	int64_t deserialize(VBfsBlock &, uint64_t);

	/* allocate the next available block id */
	bfs_vbid_t alloc_blk();

	/* release ownership of a device block id */
	int32_t dealloc_blk(bfs_vbid_t);

private:
	uint64_t magic;		   /* magic number */
	uint64_t blk_sz;	   /* block size for file operations */
	uint64_t ino_sz;	   /* on-bdev inode structure size */
	bfs_vbid_t no_blocks;  /* total number of free blocks to use */
	bfs_vbid_t no_dblocks; /* total number of data blocks (blocks-reservered) */
	bfs_ino_id_t no_inodes;		 /* number of free inodes to allocate */
	bfs_vbid_t no_dblocks_free;	 /* current number of free data blocks */
	bfs_ino_id_t no_inodes_free; /* current number of free inodes */
	bfs_vbid_t
		first_data_blk_loc;	  /* location of first data block not reserved */
	bfs_vbid_t next_vbid;	  /* next id to use for block allocation */
	bfs_ino_id_t root_ino;	  /* inode num of the root dir */
	bfs_ino_id_t ibm_ino;	  /* inode num of the ibitmap */
	bfs_ino_id_t itab_ino;	  /* inode num of the itable */
	bfs_ino_id_t journal_ino; /* inode number of the file system journal */
	bfs_ino_id_t first_unresv_ino; /* inode number to begin allocating from */
	uint64_t state;				   /* current state of the file system */
	bool dirty; /* flag indicating if the superblock is dirty and needs sync */

	/* set the dirty flag */
	void set_dirty(bool);
};

class IBitMap {
public:
	IBitMap();
	~IBitMap();

	/* get the list of inode bitmap blocks */
	std::vector<VBfsBlock *> &get_ibm_blks();

	/* append a block to the in-memory inode bitmap structure */
	void append_ibm_blk(VBfsBlock *);

	/* set a bit to one in the bitmap block to indicate the inode is used */
	void set_bit(bfs_ino_id_t);

	/* zero a bit in the bitmap block to indicate the inode is free */
	void clear_bit(bfs_ino_id_t);

private:
	std::vector<VBfsBlock *> ibm_blks; /* data blocks containing the ibitmap */
};

class Inode : public CacheableObject {
public:
	Inode(bfs_ino_id_t _i_no = 0, bfs_uid_t _uid = 0, uint32_t _mode = 0,
		  uint64_t _ref_cnt = 0, uint64_t _atime = 0, uint64_t _mtime = 0,
		  uint64_t _ctime = 0, uint64_t _size = 0, uint64_t _i_links_count = 0);
	~Inode() {}

	/* clear out inode data so it can be repurposed */
	void clear();

	/* get the inode number */
	bfs_ino_id_t get_i_no();

	/* get the user id of the inode owner */
	bfs_uid_t get_uid();

	/* get the mode bits of the inode */
	uint32_t get_mode();

	/* get the number of references to the inode */
	uint64_t get_ref_cnt();

	/* get the last access time of the inode */
	uint64_t get_atime();

	/* get the last modified time of the inode */
	uint64_t get_mtime();

	/* get the creation time of the inode */
	uint64_t get_ctime();

	/* get the current size of the inode */
	uint64_t get_size();

	/* get the number of hard links in the inode (for directories) */
	uint64_t get_i_links_count();

	/* get the inode's list of direct data blocks */
	const std::vector<bfs_vbid_t> &get_i_blks();

	/* set the inode number */
	void set_i_no(bfs_ino_id_t);

	/* set the inode owner as the given user id */
	void set_uid(bfs_uid_t);

	/* set the access permissions on the inode */
	void set_mode(uint32_t);

	/* set the number of references to the inode */
	void set_ref_cnt(uint64_t);

	/* set the last access time of the inode */
	void set_atime(uint64_t);

	/* set the last modified time of the inode */
	void set_mtime(uint64_t);

	/* set the creation time of the inode */
	void set_ctime(uint64_t);

	/* set the current size of the inode */
	void set_size(uint64_t);

	/* set the number of hard links in the inode (for directory files) */
	void set_i_links(uint64_t);

	/* set a value (allocated direct data block) in the iblks list */
	void set_i_blk(uint64_t, bfs_vbid_t);

	/* serialize into a format suitable to be put on bdev */
	int64_t serialize(VBfsBlock &, uint64_t);

	/* deserialize from an on-bdev format to in-memory structures */
	int64_t deserialize(VBfsBlock &, uint64_t);

private:
	bfs_ino_id_t i_no;		/* inode number for the inode */
	bfs_uid_t uid;			/* Owner of the inode */
	uint32_t mode;			/* permissions bits for the inode */
	uint64_t ref_cnt;		/* number of open handles for the inode */
	uint64_t atime;			/* last access time of inode */
	uint64_t mtime;			/* last modified time of inode */
	uint64_t ctime;			/* creation time of inode */
	uint64_t size;			/* size in bytes of the inode */
	uint64_t i_links_count; /* number of subdirectories (hard links) */
	std::vector<bfs_vbid_t>
		i_blks; /* number of data blocks (NUM_INODE_IBLKS) */
};

class DirEntry : public CacheableObject {
public:
	DirEntry(std::string n = "", bfs_ino_id_t i = 0, bfs_vbid_t = UINT64_MAX,
			 uint64_t = UINT64_MAX, uint32_t = 0, uint64_t = 0, uint32_t = 0,
			 uint32_t = 0, uint32_t = 0);
	~DirEntry();

	/* get the name in the dentry */
	std::string get_de_name();

	/* get the inode in the dentry */
	bfs_ino_id_t get_ino();

	/* get the absolute block location of the dentry */
	bfs_vbid_t get_blk_loc();

	/* get the absolute block index of the dentry */
	uint64_t get_idx_loc();

	uint32_t get_e_mode();

	uint64_t get_e_size();

	uint32_t get_atime();

	uint32_t get_mtime();

	uint32_t get_ctime();

	/* set the name of the dentry */
	void set_de_name(std::string);

	/* set the inode number that the dentry refers to */
	void set_ino(bfs_ino_id_t);

	/* set the absolute block location of the dentry (only stored in-memory) */
	void set_blk_loc(bfs_vbid_t);

	/* set the absolute block index of the dentry (only stored in-memory) */
	void set_blk_idx_loc(uint64_t);

	/* serialize into a format suitable to be put on bdev */
	int64_t serialize(VBfsBlock &, uint64_t);

	/* deserialize from an on-bdev format to in-memory structures */
	int64_t deserialize(VBfsBlock &, uint64_t);

private:
	std::string de_name;  /* name of the dentry (MAX_FILE_NAME_LEN) */
	bfs_ino_id_t ino;	  /* inode num of the dentry */
	bfs_vbid_t blk_loc;	  /* block location of the dentry */
	uint64_t blk_idx_loc; /* dentry index location into the block */

	/* For lwext4, we can grab these directly when we retrieve the dentry, so
	 * store here to send back to client. */
	uint32_t e_mode = 0;
	uint64_t e_size = 0;
	uint32_t atime;
	uint32_t mtime;
	uint32_t ctime;
};

/**
 * @brief An indirect block for an inode, holding more block ids (indirect_locs)
 * that store data for an inode.
 */
class IndirectBlock {
public:
	IndirectBlock();
	~IndirectBlock() {}

	/* get the list of indirect data block ids */
	const std::vector<bfs_vbid_t> &get_indirect_locs();

	/* set a value (allocated indirect data block) in the indirect block */
	void set_indirect_loc(uint64_t, bfs_vbid_t);

	/* serialize into a format suitable to be put on bdev */
	int64_t serialize(VBfsBlock &, uint64_t);

	/* deserialize from an on-bdev format to in-memory structures */
	int64_t deserialize(VBfsBlock &, uint64_t);

private:
	std::vector<bfs_vbid_t> indirect_locs; /* indirect block ids */
};

/**
 * @brief An open file in the open file table, for a single user. It holds an
 * inode number and offset, and is mapped by an open file handle to the inode.
 */
class OpenFile {
public:
	OpenFile(bfs_ino_id_t, uint64_t);
	~OpenFile() {}

	/* get the associated inode number */
	bfs_ino_id_t get_ino();

	/* get the current read/write offset */
	void set_offset(uint64_t);

private:
	bfs_ino_id_t ino; /* inode number that the object is associated with */
	uint64_t offset;  /* current read/write offset */
};

/**
 * @brief Main class for creating an instance of the BFS file
 * system, handling client requests, and communicating with the backend.
 */
class BfsHandle {
public:
	BfsHandle();
	~BfsHandle();

	/* Get reference to the dentry cache object */
	const BfsCache &get_dentry_cache();

	/* Get reference to the inode cache object */
	const BfsCache &get_ino_cache();

	/* Get the current status of the file system */
	int32_t get_status();

	/* Flush an in-memory inode to block device */
	void flush_inode(Inode *);

	/* Flush an in-memory directory entry to block device */
	void flush_dentry(DirEntry *);

	/**
	 * Core wrappers over the the VBlockDevice methods that support the public
	 * file system interface. For now, these should always be
	 * synchronous/blocking. Don't have a journal or complex write mechanisms in
	 * place yet. Eventually with a more complex device backend we will want
	 * some writes to be blocking on-demand (e.g., with O_SYNC/fsync/msync for
	 * raw I/O or SPDK_BDEV_IO_TYPE_FLUSH with spdk).
	 */

	/* Read a specified block from the backend block devices */
	void read_blk(VBfsBlock &);

	/* Write a specified block to the backend block devices */
	void write_blk(VBfsBlock &, op_flags_t);

	/* Format a block device with bfs */
	int32_t mkfs();

	/* Mount a formatted file system */
	int32_t mount();

	/* Read an on-bdev inode structure into memory */
	Inode *read_inode(bfs_ino_id_t, bool pop = false);

	/* Write an in-memory inode structure to a block device */
	int32_t write_inode(Inode *, op_flags_t, bfs_ino_id_t del_loc = 0,
						bool put_cache = true);

	/* Get the attributes of a file (regular or directory) */
	int32_t bfs_getattr(BfsUserContext *usr, std::string, bfs_uid_t *,
						bfs_ino_id_t *, uint32_t *, uint64_t *);

	/* Open a directory file */
	bfs_fh_t bfs_opendir(BfsUserContext *usr, std::string);

	/* Read director entries from an open directory file */
	int32_t bfs_readdir(BfsUserContext *usr, bfs_fh_t,
						std::vector<DirEntry *> *);

	/* Create a directory file */
	int32_t bfs_mkdir(BfsUserContext *usr, std::string, uint32_t);

	/* Delete a directory file */
	int32_t bfs_rmdir(BfsUserContext *usr, std::string);

	/* Delete a regular file */
	int32_t bfs_unlink(BfsUserContext *usr, std::string);

	/* Rename a file */
	int32_t bfs_rename(BfsUserContext *usr, std::string, std::string);

	/* Create and open a regular file */
	bfs_fh_t bfs_create(BfsUserContext *usr, std::string, uint32_t);

	/* Change file permissions */
	bfs_fh_t bfs_chmod(BfsUserContext *usr, std::string, uint32_t);

	/* Open a regular file for reading/writing */
	bfs_fh_t bfs_open(BfsUserContext *usr, std::string, uint32_t);

	/* Read data from an open regular file at a given offset */
	uint64_t bfs_read(BfsUserContext *usr, bfs_fh_t, char *, uint64_t,
					  uint64_t);

	/* Write data to an open regular file at a given offset */
	uint64_t bfs_write(BfsUserContext *usr, bfs_fh_t, char *, uint64_t,
					   uint64_t);

	/* Synchronize in-memory and on-bdev data */
	int32_t bfs_fsync(BfsUserContext *usr, bfs_fh_t, uint32_t);

	/* Close a file handle associated with an open file */
	int32_t bfs_release(BfsUserContext *usr, bfs_fh_t);

private:
	SuperBlock sb;		   /* holds the core info about the file system */
	BfsCache dentry_cache; /* directory entry cache */
	BfsCache ino_cache;	   /* inode cache */
	std::unordered_map<bfs_fh_t, OpenFile *>
		open_file_tab; /* maps file handles to open files (inode+offset) */
	uint32_t next_fd;  /* the next open file descriptor to allocate */
	int32_t status;	   /* current status of the file system */

	/**
	 *  Private helpers
	 */

	/* Allocate a new file descriptor for an open file */
	bfs_fh_t alloc_fd();

	/* Allocate a new inode number for a file */
	bfs_ino_id_t alloc_ino();

	/* Release ownership of an inode number */
	void dealloc_ino(Inode *);

	/* Deletes the iblocks for an inode */
	void delete_inode_iblks(Inode *);

	/* Walk the itable and data blocks to search for a directory entry */
	int32_t get_de(BfsUserContext *, DirEntry **, std::string,
				   bool pop = false);

	/* Search the inode (in)direct blocks for a specific/all dentry */
	bool check_direct_blks(BfsUserContext *, std::string, DirEntry **, bool *,
						   bfs_ino_id_t *, uint32_t *, int32_t,
						   std::vector<DirEntry *> *);
	bool check_indirect_blks(BfsUserContext *, std::string, DirEntry **, bool *,
							 bfs_ino_id_t *, uint32_t *, int32_t,
							 std::vector<DirEntry *> *);

	/* Check each dentry in a directory file's data blocks for a conditiion */
	bool check_each_dentry(VBfsBlock &, Inode *, DirEntry **, bool *,
						   uint32_t *, int32_t, std::string,
						   std::vector<DirEntry *> *);

	/* Initializes and writes a dentry object to an inode's indirect blocks */
	int32_t add_dentry_to_indirect_blks(Inode *, Inode *, std::string);

	/* Initializes and writes a dentry object to an inode's direct blocks */
	int32_t add_dentry_to_direct_blks(Inode *, Inode *, std::string);

	/* Retrieve an entry from the dentry cache by key */
	DirEntry *read_dcache(stringCacheKey, bool pop = false);

	/* Add a new entry to the dentry cache */
	void write_dcache(stringCacheKey, DirEntry *);

	/* Prealloc some direct and indirect blocks for an inode upon creation */
	uint32_t prealloc_blks(Inode *);
};

/**
 * @brief Exceptions thrown by core methods as a result of server
 * failures (e.g., bad channel with a device) such that it can no longer serve
 * client requests reliably. Fatal error. Forces the server to abort immediately
 * if error code is BFS_FAILURE, and graceful shutdown if BFS_SHUTDOWN (e.g., as
 * a result of a caught signal).
 */
class BfsServerError : public std::exception {
public:
	BfsServerError(std::string, Inode *, Inode *);
	~BfsServerError() {}

	/* Get the error message */
	std::string err();

private:
	std::string err_msg; /* Description of the error */
};

/**
 * @brief Exceptions thrown by core methods as a result of a client request
 * failure. Not fatal.
 */
class BfsClientRequestFailedError : public std::exception {
public:
	BfsClientRequestFailedError(std::string, Inode *, Inode *);
	~BfsClientRequestFailedError() {}

	/* Get the error message */
	std::string err();

private:
	std::string err_msg; /* Description of the error */
};

/**
 * @brief Exceptions thrown by access control methods when an operation is not
 * permitted. Not fatal.
 */
class BfsAccessDeniedError : public std::exception {
public:
	BfsAccessDeniedError(std::string, Inode *, Inode *);
	~BfsAccessDeniedError() {}

	/* Get the error message */
	std::string err();

private:
	std::string err_msg; /* Description of the error */
};

#endif /* BFS_CORE_H */
