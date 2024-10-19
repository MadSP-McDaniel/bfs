#ifndef BFS_CORE_TEST_ECALLS_EXT4_H
#define BFS_CORE_TEST_ECALLS_EXT4_H

#include "ext4.h"
#include "ext4_mkfs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DROP_LINUXCACHE_BUFFERS 0

struct ext4_io_stats {
	float io_read;
	float io_write;
	float cpu;
};

/* Main block device interface */
int file_dev_open(struct ext4_blockdev *bdev);
int file_dev_bread(struct ext4_blockdev *bdev, void *buf, uint64_t blk_id,
				   uint32_t blk_cnt);
int file_dev_bwrite(struct ext4_blockdev *bdev, const void *buf,
					uint64_t blk_id, uint32_t blk_cnt);
int file_dev_close(struct ext4_blockdev *bdev);

/* Helper methods for unit tests */
int init_bfs_core_ext4_blk_test(void);
int fini_bfs_core_ext4_blk_test();
int __do_file_dev_bwrite(const void *);
int __do_file_dev_bread(void *);
int __do_get_block(bfs_vbid_t, void *);
int __do_put_block(bfs_vbid_t, void *);
int run_bfs_core_ext4_file_test(void);
double __get_time(void);

/* Helper methods for bfs server using lwext4 backend */
int __do_lwext4_init(void *);
int __do_lwext4_mount(void);
int __do_lwext4_mkfs(void);
int __do_lwext4_getattr(void *usr, const char *path, uint32_t *uid,
						uint64_t *fino, uint32_t *fmode, uint64_t *fsize,
						uint32_t *atime, uint32_t *mtime, uint32_t *ctime);
int __do_lwext4_mkdir(void *usr, const char *path, uint32_t fmode);
int __do_lwext4_unlink(void *usr, const char *path);
int __do_lwext4_rename(void *usr, const char *fpath, const char *tpath);
int __do_lwext4_create(void *usr, const char *path, uint32_t mode);
int __do_lwext4_ftruncate(void *usr, const char *path, bfs_fh_t fh,
						  uint32_t len);
int __do_lwext4_chmod(void *usr, const char *path, uint32_t new_mode);
int __do_lwext4_open(void *usr, const char *path, uint32_t fmode);
int __do_lwext4_read(void *usr, bfs_fh_t fh, char *buf, uint64_t rsize,
					 uint64_t off);
int __do_lwext4_write(void *usr, bfs_fh_t fh, char *buf, uint64_t wsize,
					  uint64_t off);
int __do_lwext4_fsync(void *usr, bfs_fh_t fh, uint32_t datasync);
int __do_lwext4_release(void *usr, bfs_fh_t fh);
int __do_lwext4_opendir(void *usr, const char *path);
int __do_lwext4_readdir(void *usr, uint64_t fh, void *_ents);
int __do_lwext4_rmdir(void *usr, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* BFS_CORE_TEST_ECALLS_EXT4_H */
