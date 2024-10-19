/**
 * @file bfs_client.h
 * @brief Interface, types, and macros for the bfs_client subsystem. These are
 * the FUSE hooks for the client that allow them to communicate with the bfs
 * server (enclave) to execute file operations.
 */

#ifndef BFS_CLIENT_H
#define BFS_CLIENT_H

#define FUSE_USE_VERSION 39
#include <fuse.h>

#define CLIENT_LOG_LEVEL bfs_client_log_level
#define CLIENT_VRB_LOG_LEVEL bfs_client_vrb_log_level

#define BFS_CLIENT_LAYER_CONFIG "bfsClientLayer"

extern const struct fuse_operations bfs_oper; /* struct for fops hooks */

/* Get the attributes of a file (regular file or directory). */
int bfs_getattr(const char *, struct stat *, struct fuse_file_info *);

/* Create a new directory file. */
int bfs_mkdir(const char *, uint32_t);

/* Delete a regular file. */
int bfs_unlink(const char *);

/* Delete a directory file. */
int bfs_rmdir(const char *);

/* Rename a file. */
int bfs_rename(const char *, const char *, unsigned int);

int bfs_flush(const char *path, struct fuse_file_info *fi);

/* Change the permission bits of a file */
int bfs_chmod(const char *, mode_t, struct fuse_file_info *);

int bfs_chown(const char *path, uid_t new_uid, gid_t new_gid,
			  struct fuse_file_info *fi);

int bfs_utimens(const char *path, const struct timespec tv[2],
				struct fuse_file_info *fi);

/* Open a regular file. */
int bfs_open(const char *, struct fuse_file_info *);

/* Read data from an open regular file. */
int bfs_read(const char *, char *, size_t, off_t, struct fuse_file_info *);

/* Write data to an open regular file. */
int bfs_write(const char *, const char *, size_t, off_t,
			  struct fuse_file_info *);

/* Close a file handle. */
int bfs_release(const char *, struct fuse_file_info *);

/* Release directory */
int bfs_releasedir(const char *, struct fuse_file_info *);

/* Synchronize in-memory and on-bdev file contents. */
int bfs_fsync(const char *, int, struct fuse_file_info *);

/* Open a directory file. */
int bfs_opendir(const char *, struct fuse_file_info *);

/* Read directory entries from an open directory file */
int bfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
				struct fuse_file_info *, enum fuse_readdir_flags);

/* Initialize the bfs system. */
void *bfs_init(struct fuse_conn_info *, struct fuse_config *);

/* Shutdown the bfs system. */
void bfs_destroy(void *);

/* Create and open a new regular file. */
int bfs_create(const char *, uint32_t, struct fuse_file_info *);

int bfs_truncate(const char *, off_t, struct fuse_file_info *);

/* Pre-allocate space for a regular file. */
int bfs_fallocate(const char *, int, off_t, off_t, struct fuse_file_info *);

/* Seek to specified location in file */
off_t bfs_lseek(const char *, off_t, int, struct fuse_file_info *);

/* Initialize the client subsystem */
int client_init();

#endif /* BFS_CLIENT_H */
