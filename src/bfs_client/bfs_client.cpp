/**
 * @file bfs_client.cpp
 * @brief Definitions for the bfs_client FUSE hooks and helpers. Defines the
 * interface used by the fuse file system for communicating over the network to
 * the file server (enclave).
 */

#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <libgen.h>
#include <mutex>
#include <set>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <unordered_map>

#include "bfs_client.h"
#include <bfsConfigLayer.h>
#include <bfsConnectionMux.h>
#include <bfsCryptoError.h>
#include <bfsNetworkConnection.h>
#include <bfsSecAssociation.h>
#include <bfsUtilError.h>
#include <bfsUtilLayer.h>
#include <bfs_common.h>
#include <bfs_core.h>
#include <bfs_log.h>
#include <bfs_server.h>
#include <bfs_util.h>

class shared_mutex_t {
public:
	void lock_shared() {
		std::unique_lock<std::mutex> lock(mutex_);
		while (writer_active_) {
			cond_.wait(lock);
		}
		++reader_count_;
	}

	void unlock_shared() {
		std::unique_lock<std::mutex> lock(mutex_);
		if (--reader_count_ == 0) {
			cond_.notify_all();
		}
	}

	void lock() {
		std::unique_lock<std::mutex> lock(mutex_);
		while (writer_active_ || reader_count_ > 0) {
			cond_.wait(lock);
		}
		writer_active_ = true;
	}

	void unlock() {
		std::unique_lock<std::mutex> lock(mutex_);
		writer_active_ = false;
		cond_.notify_all();
	}

private:
	std::mutex mutex_;
	std::condition_variable cond_;
	bool writer_active_ = false;
	int reader_count_ = 0;
};

static bfsNetworkConnection *client = NULL;	 /* handle for sending data */
static bfsSecAssociation *secContext = NULL; /* handle for crypto ops */
static bfsConnectionMux *mux = NULL; /* for waiting on data from server */
uint64_t bfs_client_log_level = 0, bfs_client_vrb_log_level = 0; /* log lvls */
bool do_mkfs = false;			 /* flag indicating to format on init */
static pthread_mutex_t mux_lock; /* for concurrency with multi-threaded FUSE */
static bool direct_io_flag = false;
static std::string bfs_server_ip = "";
static unsigned short bfs_server_port = -1;
static uint32_t send_seq, recv_seq;
const struct fuse_operations bfs_oper = {
	/* bfs fops hooks */
	.getattr = bfs_getattr,
	.readlink = NULL,
	.mknod = NULL,
	.mkdir = bfs_mkdir,
	.unlink = bfs_unlink,
	.rmdir = bfs_rmdir,
	.symlink = NULL,
	.rename = bfs_rename,
	.link = NULL,
	.chmod = bfs_chmod,
	.chown = bfs_chown,
	.truncate = bfs_truncate,
	.open = bfs_open,
	.read = bfs_read,
	.write = bfs_write,
	.statfs = NULL,
	.flush = bfs_flush,
	.release = bfs_release,
	.fsync = bfs_fsync,
	.setxattr = NULL,
	.getxattr = NULL,
	.listxattr = NULL,
	.removexattr = NULL,
	.opendir = bfs_opendir,
	.readdir = bfs_readdir,
	.releasedir = bfs_releasedir,
	.fsyncdir = NULL,
	.init = bfs_init,
	.destroy = bfs_destroy,
	.access = NULL,
	.create = bfs_create,
	.lock = NULL,
	.utimens = bfs_utimens,
	.bmap = NULL,
	.ioctl = NULL,
	.poll = NULL,
	.write_buf = NULL,
	.read_buf = NULL,
	.flock = NULL,
	.fallocate = bfs_fallocate,
	.copy_file_range = NULL,
	.lseek = bfs_lseek,
};

// We are going to implement an NFS-like client-cache for file data. This will
// allow us to avoid sending data to the server for every read/write operation.
// The client will cache data in a local file, and will only send data to the
// server when the file is closed/flushed.
static std::unordered_map<bfs_fh_t, int> file_cache; /* file data cache, maps bfs file
							   handles to local file handles */
static std::unordered_map<bfs_fh_t, std::set<uint64_t> *>
	dirty_chunks; // track the dirty chunks for the file cache
// static pthread_mutex_t writeback_lock; /* for concurrency with writeback */
static shared_mutex_t writeback_lock;
static void *client_wb_worker_entry(void *);
static pthread_t writeback_thread;
static uint64_t total_dirty_chunks = 0;
const uint64_t chunk_size = 1024 * 1024;
static const uint64_t CONGESTION_THRESHOLD =
	(((long)1) << ((long)30)) / chunk_size;
static bool client_status = 0;
// static long num_files_opened = 0;
// static long num_files_closed = 0;

/* For performance testing */
static std::vector<double> c_read__lats, c_read__c_lats, c_read__net_send_lats,
	c_read__net_recv_lats, c_write__lats, c_write__c_lats,
	c_write__net_send_lats, c_write__net_recv_lats;
static void write_client_latencies();

static void send_msgp(bfsFlexibleBuffer &);
static uint64_t recv_msgp(bfsFlexibleBuffer &, uint32_t, msg_type_t, op_type_t,
						  bool);
static void cleanup();

/**
 * @brief Get file attributes. Same as 'stat' operation. Serializes and sends a
 * request message to the bfs server, then expects a serialized response
 * containing the file inode number, mode, and size.
 *
 * @param path: the file for which to get attributes
 * @param stbuf: buffer to fill with attributes
 * @param fi: unused
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int bfs_getattr(const char *path, struct stat *stbuf,
				struct fuse_file_info *fi) {
	(void)fi;
	int32_t mtype = INVALID_MSG, otype = INVALID_OP, ret = BFS_FAILURE;
	uint32_t recv_mode = 0;
	uint64_t recv_ino = 0, recv_sz = 0;
	bfs_uid_t recv_uid = 0;
	uint32_t recv_atime, recv_mtime, recv_ctime;
	uint32_t path_len = (uint32_t)(strlen(path) + 1),
			 total_send_msg_len =
				 (uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(path_len)),
			 total_recv_msg_len = (uint32_t)(
				 sizeof(mtype) + sizeof(otype) + sizeof(ret) +
				 sizeof(recv_uid) + sizeof(recv_ino) + sizeof(recv_mode) +
				 sizeof(recv_sz) + sizeof(recv_atime) + sizeof(recv_mtime) +
				 sizeof(recv_ctime));
	bfsFlexibleBuffer spkt, rpkt;

	logMessage(CLIENT_VRB_LOG_LEVEL, "Trying client getattr [path: %s].", path);
	try {
		/* Serialize and send getattr request to bfs server. Uses 32-bit length.
		 */
		spkt.resetWithAlloc(path_len, 0, total_send_msg_len, 0);
		spkt.setData(path, path_len);
		mtype = TO_SERVER;
		otype = CLIENT_GETATTR_OP;
		spkt << path_len << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;
		send_msgp(spkt);
		delete aad;

		/**
		 * Receive and deserialize getattr response from bfs server. Expects
		 * lengths to be 32-bit, and interprets a bad inode number as a
		 * non-existent file.
		 */
		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_GETATTR_OP,
				  false);
		pthread_mutex_unlock(&mux_lock);
		rpkt >> ret >> stbuf->st_uid >> recv_ctime >> recv_mtime >>
			recv_atime >> stbuf->st_ino; // mtype and otype already pulled out
		stbuf->st_ctime = recv_ctime;	 // force cast to 32-bit
		stbuf->st_mtime = recv_mtime;
		stbuf->st_atime = recv_atime;
		stbuf->st_gid = 1000;
		stbuf->st_uid = 1000;

		if ((ret != BFS_SUCCESS) && (stbuf->st_ino < ROOT_INO)) {
			logMessage(CLIENT_VRB_LOG_LEVEL,
					   "Client getattr request failed: %s, ret=%s\n", path,
					   strerror(-ret));
			return -ENOENT;
		}
		rpkt >> stbuf->st_mode >> stbuf->st_size;
		// ... other attrs

		logMessage(CLIENT_VRB_LOG_LEVEL,
				   "Client getattr OK: [path: %s, mode: %x, size: %lu]\n", path,
				   stbuf->st_mode, stbuf->st_size);

	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what(), path);
		cleanup();
		return BFS_FAILURE;
	}

	return BFS_SUCCESS;
}

/**
 * @brief Create a new directory file. Serializes and sends a request message to
 * the bfs server, then expects a serialized response containing a return code
 * for the request.
 *
 * @param path: absolute path for the directory file
 * @param mode: permissions to set on the directory
 * @return int: BFS_SUCCESS if success, BFS_FAILURE/-ENOENT if failure
 */
int bfs_mkdir(const char *path, uint32_t mode) {
	uint32_t _mode = mode;
	int32_t mtype = INVALID_MSG, otype = INVALID_OP, ret = BFS_FAILURE,
			path_len = (uint32_t)(strlen(path) + 1),
			total_send_msg_len = (uint32_t)(sizeof(mtype) + sizeof(otype) +
											sizeof(_mode) + sizeof(path_len)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;

	try {
		/* Serialize and send mkdir request to the bfs server */
		spkt.resetWithAlloc(path_len, 0, total_send_msg_len, 0);
		spkt.setData(path, path_len);
		mtype = TO_SERVER;
		otype = CLIENT_MKDIR_OP;
		spkt << path_len << _mode << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;
		send_msgp(spkt);
		delete aad;

		/* Receive and deserialize mkdir response from the server */
		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_MKDIR_OP,
				  false);
		pthread_mutex_unlock(&mux_lock);
		rpkt >> ret;
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what(), path);
		cleanup();
		return BFS_FAILURE;
	}

	if (ret != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Client mkdir request failed: %s\n", path);
		return ret;
	}

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client mkdir OK.\n");
	return BFS_SUCCESS;
}

/**
 * @brief Delete a regular file (unless some descriptors are still open). See:
 * https://linux.die.net/man/2/unlink. What 'rm' uses internally. Eventually
 * need to correctly deal with multiple hard/sym links.
 *
 * @param path: absolute path of the regular file
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int bfs_unlink(const char *path) {
	int32_t mtype = INVALID_MSG, otype = INVALID_OP, ret = BFS_FAILURE,
			path_len = (uint32_t)(strlen(path) + 1),
			total_send_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(path_len)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;

	try {
		/* Serialize and snd unlink request to the server */
		spkt.resetWithAlloc(path_len, 0, total_send_msg_len, 0);
		spkt.setData(path, path_len);
		mtype = TO_SERVER;
		otype = CLIENT_UNLINK_OP;
		spkt << path_len << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;
		send_msgp(spkt);
		delete aad;

		/* Receive and deserialize unlink response from the server */
		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_UNLINK_OP,
				  false);
		pthread_mutex_unlock(&mux_lock);
		rpkt >> ret;
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what(), path);
		cleanup();
		return BFS_FAILURE;
	}

	if (ret != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Client unlink request failed: %s\n", path);
		return ret;
	}

	// if (pthread_mutex_lock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to acquire writeback lock\n");
	// 	return BFS_FAILURE;
	// }

	// // Now delete the local file if it exists
	// char *tmp_path = (char *)malloc(strlen(path) + 5);
	// strcpy(tmp_path, "/tmp");
	// strcat(tmp_path, path);
	// if (access(tmp_path, F_OK) != -1) {
	// 	if (remove(tmp_path) != 0) {
	// 		logMessage(LOG_ERROR_LEVEL, "Failed to delete local file: %s\n",
	// 				   tmp_path);
	// 		return BFS_FAILURE;
	// 	}
	// }

	// if (pthread_mutex_unlock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to release writeback lock\n");
	// 	return BFS_FAILURE;
	// }

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client unlink OK.\n");
	return BFS_SUCCESS;
}

/**
 * @brief Remove a directory file. Serializes and sends a request message to the
 * bfs server, then expects a serialized response containing a return code for
 * the request.
 *
 * @param path: absolute path for the directory file
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int bfs_rmdir(const char *path) {
	int32_t mtype = INVALID_MSG, otype = INVALID_OP, ret = BFS_FAILURE,
			path_len = (uint32_t)(strlen(path) + 1),
			total_send_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(path_len)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;

	try {
		// Send rmdir request
		spkt.resetWithAlloc(path_len, 0, total_send_msg_len, 0);
		spkt.setData(path, path_len);
		mtype = TO_SERVER;
		otype = CLIENT_RMDIR_OP;
		spkt << path_len << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;
		send_msgp(spkt);
		delete aad;

		// Receive rmdir response
		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_RMDIR_OP,
				  false);
		pthread_mutex_unlock(&mux_lock);
		rpkt >> ret;
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what(), path);
		cleanup();
		return BFS_FAILURE;
	}

	if (ret != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Client rmdir request failed: %s\n", path);
		return ret;
	}

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client rmdir OK.\n");
	return BFS_SUCCESS;
}

/**
 * @brief Rename a file. Serializes and sends a request message to the bfs
 * server, then expects a serialized response containing a return code for the
 * request.
 *
 * Note (from FUSE): *flags* may be `RENAME_EXCHANGE` or `RENAME_NOREPLACE`. If
 * RENAME_NOREPLACE is specified, the filesystem must not overwrite *newname* if
 * it exists and return an error instead. If `RENAME_EXCHANGE` is specified, the
 * filesystem must atomically exchange the two files, i.e. both must exist and
 * neither may be deleted.
 *
 * @param from: absolute path for the file
 * @param to: absolute path for the new file name
 * @param flags: unused
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int bfs_rename(const char *from, const char *to, unsigned int flags) {
	(void)flags;
	int32_t mtype = INVALID_MSG, otype = INVALID_OP, ret = BFS_FAILURE,
			from_copy_len = (uint32_t)(strlen(from) + 1),
			to_copy_len = (uint32_t)(strlen(to) + 1),
			total_send_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) +
						   sizeof(from_copy_len) + sizeof(to_copy_len)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;
	char *combined = new char[from_copy_len + to_copy_len];

	logMessage(CLIENT_VRB_LOG_LEVEL, "Trying client rename [%s => %s].\n", from,
			   to);
	try {
		// Send rename request
		memcpy(combined, from, from_copy_len);
		memcpy(&combined[from_copy_len], to, to_copy_len);

		spkt.resetWithAlloc(from_copy_len + to_copy_len, 0, total_send_msg_len,
							0);
		spkt.setData(combined, from_copy_len + to_copy_len);
		delete combined;
		mtype = TO_SERVER;
		otype = CLIENT_RENAME_OP;
		spkt << to_copy_len << from_copy_len << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;
		send_msgp(spkt);
		delete aad;

		// Receive rename response
		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_RENAME_OP,
				  false);
		pthread_mutex_unlock(&mux_lock);
		rpkt >> ret;
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what(), from);
		cleanup();
		return BFS_FAILURE;
	}

	if (ret != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Client rename request failed: %s\n", from);
		return ret;
	}

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client rename OK [%s => %s].\n", from,
			   to);
	return BFS_SUCCESS;
}

/**
 * @brief Change the permission bits on a file.
 *
 * @param path: absolute path for the file
 * @param new_mode: new permission bits
 * @param fi: unused
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int bfs_chmod(const char *path, mode_t new_mode, struct fuse_file_info *fi) {
	(void)fi;
	bfs_fh_t ret = BFS_FAILURE;
	int32_t mtype = INVALID_MSG, otype = INVALID_OP,
			path_len = (uint32_t)(strlen(path) + 1),
			total_send_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(new_mode) +
						   sizeof(path_len)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;

	try {
		// Send chmod request
		spkt.resetWithAlloc(path_len, 0, total_send_msg_len, 0);
		spkt.setData(path, path_len);
		mtype = TO_SERVER;
		otype = CLIENT_CHMOD_OP;
		spkt << path_len << new_mode << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;
		send_msgp(spkt);
		delete aad;

		// Receive chmod response
		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_CHMOD_OP,
				  false);
		pthread_mutex_unlock(&mux_lock);
		rpkt >> ret;
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what());
		cleanup();
		return BFS_FAILURE;
	}

	if (ret != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Client chmod request failed: %s\n", path);
		return (int)ret;
	}

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client chmod OK.\n");
	return BFS_SUCCESS;
}

int bfs_chown(const char *path, uid_t new_uid, gid_t new_gid,
			  struct fuse_file_info *fi) {
	(void)fi;
	bfs_fh_t ret = BFS_FAILURE;
	int32_t mtype = INVALID_MSG, otype = INVALID_OP,
			path_len = (uint32_t)(strlen(path) + 1),
			total_send_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(new_uid) +
						   sizeof(new_gid) + sizeof(path_len)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;

	// try {
	//     // Send chown request
	//     spkt.resetWithAlloc(path_len, 0, total_send_msg_len, 0);
	//     spkt.setData(path, path_len);
	//     mtype = TO_SERVER;
	//     otype = CLIENT_CHOWN_OP;
	//     spkt << path_len << new_uid << new_gid << otype << mtype;
	//     pthread_mutex_lock(&mux_lock); bfsFlexibleBuffer *aad =
	//         new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
	//     secContext->encryptData(spkt, aad, true);
	//     send_seq++;
	//     send_msgp(spkt);
	//     delete aad;

	//     // Receive chown response
	//     recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_CHOWN_OP,
	//               false);
	//     pthread_mutex_unlock(&mux_lock); rpkt >> ret;
	// } catch (bfsCryptoError *err) {
	//     logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
	//                err->getMessage().c_str());
	//     delete err;

	//     return BFS_FAILURE;
	// } catch (bfsUtilError *err) {
	//     logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
	//     delete err;
	//     return BFS_FAILURE;
	// } catch (std::runtime_error &re) { // system errors (ie corrupt data
	// stream)
	//     if (strlen(re.what()) > 0)
	//         logMessage(LOG_ERROR_LEVEL, re.what());
	//     cleanup();
	//     return BFS_FAILURE;
	// }

	// if (ret != BFS_SUCCESS) {
	//     logMessage(LOG_ERROR_LEVEL, "Client chown request failed: %s\n",
	//     path); return (int)ret;
	// }

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client chown OK.\n");
	return BFS_SUCCESS;
}

int bfs_utimens(const char *path, const struct timespec tv[2],
				struct fuse_file_info *fi) {
	(void)fi;
	bfs_fh_t ret = BFS_FAILURE;
	int32_t mtype = INVALID_MSG, otype = INVALID_OP,
			path_len = (uint32_t)(strlen(path) + 1),
			total_send_msg_len = (uint32_t)(
				sizeof(mtype) + sizeof(otype) + sizeof(tv[0].tv_sec) +
				sizeof(tv[1].tv_sec) + sizeof(path_len)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;

	// try {
	//     // Send utimens request
	//     spkt.resetWithAlloc(path_len, 0, total_send_msg_len, 0);
	//     spkt.setData(path, path_len);
	//     mtype = TO_SERVER;
	//     otype = CLIENT_UTIMENS_OP;
	//     spkt << path_len << tv[0].tv_sec << tv[1].tv_sec << otype << mtype;
	//     pthread_mutex_lock(&mux_lock); bfsFlexibleBuffer *aad =
	//         new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
	//     secContext->encryptData(spkt, aad, true);
	//     send_seq++;
	//     send_msgp(spkt);
	//     delete aad;

	//     // Receive utimens response
	//     recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_UTIMENS_OP,
	//               false);
	//     pthread_mutex_unlock(&mux_lock); rpkt >> ret;
	// } catch (bfsCryptoError *err) {
	//     logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
	//                err->getMessage().c_str());
	//     delete err;

	//     return BFS_FAILURE;
	// } catch (bfsUtilError *err) {
	//     logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
	//     delete err;
	//     return BFS_FAILURE;
	// } catch (std::runtime_error &re) { // system errors (ie corrupt data
	// stream)
	//     if (strlen(re.what()) > 0)
	//         logMessage(LOG_ERROR_LEVEL, re.what());
	//
	//     cleanup();
	//     return BFS_FAILURE;
	// }

	// if (ret != BFS_SUCCESS) {
	//     logMessage(LOG_ERROR_LEVEL, "Client utimens request failed: %s\n",
	//     path); return (int)ret;
	// }

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client utimens OK.\n");
	return BFS_SUCCESS;
}

/**
 * @brief Open a file. Open flags are available in fi->flags (see fuse.h).
 * Serializes and sends a request message to the bfs server, then expects a
 * serialized response containing a file handle for the open file.
 *
 * @param path: absolute path for the regular file
 * @param fi: object containing context (file handle) for the open file
 * @return int: BFS_SUCCESS if success, BFS_FAILURE/-ENOENT if failure
 */
int bfs_open(const char *path, struct fuse_file_info *fi) {
	bfs_fh_t ret = BFS_FAILURE;
	int32_t mtype = INVALID_MSG, otype = INVALID_OP,
			path_len = (uint32_t)(strlen(path) + 1),
			total_send_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(path_len)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;

	logMessage(CLIENT_VRB_LOG_LEVEL, "Trying client open [%s].\n", path);
	try {
		// Send open request
		spkt.resetWithAlloc(path_len, 0, total_send_msg_len, 0);
		spkt.setData(path, path_len);
		mtype = TO_SERVER;
		otype = CLIENT_OPEN_OP;
		spkt << path_len << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;
		send_msgp(spkt);
		delete aad;

		// Receive open response
		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_OPEN_OP, false);
		pthread_mutex_unlock(&mux_lock);
		rpkt >> ret;
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what(), path);
		cleanup();
		return BFS_FAILURE;
	}

	// technically can open root dir but we don't allow open calls on dirs, only
	// opendir
	if ((int64_t)ret < START_FD) {
		logMessage(LOG_ERROR_LEVEL, "Client open request failed: %s\n", path);
		return (int)ret;
	}

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client open OK.\n");
	fi->fh = (bfs_fh_t)ret;

	// if (pthread_mutex_lock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to acquire writeback lock\n");
	// 	return BFS_FAILURE;
	// }
	writeback_lock.lock();

	// Open a local file with the same name and map the file handle to the bfs
	// file handle
	char *tmp_path = (char *)malloc(strlen(path) + 5);
	strcpy(tmp_path, "/tmp");
	strcat(tmp_path, path);

	char *dir_name = bfs_dirname(bfs_strdup(tmp_path));
	char *mkdir_cmd = (char *)malloc(strlen(dir_name) + 10);
	strcpy(mkdir_cmd, "mkdir -p ");
	strcat(mkdir_cmd, dir_name);
	system(mkdir_cmd);
	chmod(dir_name, 0777);
	free(mkdir_cmd);
	free(dir_name);

	int fd = open(tmp_path, O_RDWR | O_CREAT, 0777);
	if (fd < 0) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed to open local file: %s, errno: %s\n", tmp_path,
				   strerror(errno));
		return BFS_FAILURE;
	}
	// num_files_opened++;
	// logMessage(LOG_ERROR_LEVEL, "num_files_opened: %d\n", num_files_opened);

	file_cache[fi->fh] = fd;
	dirty_chunks[fi->fh] = new std::set<uint64_t>();
	free(tmp_path);

	// if (pthread_mutex_unlock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to release writeback lock\n");
	// 	return BFS_FAILURE;
	// }
	writeback_lock.unlock();

	return BFS_SUCCESS;
}

/**
 * @brief Read data from an open file. Serializes and sends a request message to
 * the bfs server, then expects a serialized response containing a return code
 * indicating the number of bytes read.
 *
 * @param path: unused
 * @param buf: buffer to fill with read bytes
 * @param size: length of the read
 * @param offset: offset in file to start reading
 * @param fi: object containing handle for the open file
 * @return int: the number of bytes read if success, BFS_FAILURE if failure
 */
int bfs_read(const char *path, char *buf, size_t size, off_t offset,
			 struct fuse_file_info *fi) {

	// if (pthread_mutex_lock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to acquire writeback lock\n");
	// 	return BFS_FAILURE;
	// }
	writeback_lock.lock_shared();

	// Check the file cache first. If the data is there, we don't need to send a
	// read request to the server.
	if (!direct_io_flag) {
		if (file_cache.find(fi->fh) != file_cache.end()) {
			int local_fd = file_cache[fi->fh];
			int br = pread(local_fd, buf, size, offset);
			if (br < 0) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failed to read from local file: %s\n", path);
				return BFS_FAILURE;
			}
			// if (pthread_mutex_unlock(&writeback_lock) != 0) {
			// 	logMessage(LOG_ERROR_LEVEL,
			// 			   "Failed to release writeback lock\n");
			// 	return BFS_FAILURE;
			// }
			writeback_lock.unlock_shared();
			return br;
		} else {
			logMessage(LOG_ERROR_LEVEL, "File handle not found in cache: %s\n",
					   path);
			abort();
		}
	}

	// if (pthread_mutex_unlock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to release writeback lock\n");
	// 	return BFS_FAILURE;
	// }
	writeback_lock.unlock_shared();

	uint64_t _size = size, _offset = offset;
	int32_t mtype = INVALID_MSG, otype = INVALID_OP,
			total_send_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(fi->fh) +
						   sizeof(_size) + sizeof(_offset)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(_size) +
						   _size); // prob want to leave size here so the check
								   // in recv goes through OK
	size_t bytes_read = 0;
	bfsFlexibleBuffer spkt, rpkt;
	double c_start_time = 0.0, c_end_time = 0.0, net_send_start_time = 0.0,
		   net_send_end_time = 0.0, net_recv_start_time = 0.0,
		   net_recv_end_time = 0.0;

	if (bfsUtilLayer::perf_test())
		c_start_time =
			(double)std::chrono::time_point_cast<std::chrono::microseconds>(
				std::chrono::high_resolution_clock::now())
				.time_since_epoch()
				.count();

	logMessage(CLIENT_VRB_LOG_LEVEL,
			   "Trying client read [path: %s, size: %lu, off: %lu].\n", path,
			   size, offset);
	try {
		assert(fi->fh >= START_FD); // sanity check; should be an open file
		spkt.resetWithAlloc(0, 0, total_send_msg_len, 0);
		mtype = TO_SERVER;
		otype = CLIENT_READ_OP;
		spkt << _offset << _size << fi->fh << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;

		if (bfsUtilLayer::perf_test())
			net_send_start_time =
				(double)std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();

		send_msgp(spkt);
		delete aad;

		if (bfsUtilLayer::perf_test()) {
			net_send_end_time =
				(double)std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();
			c_read__net_send_lats.push_back(net_send_end_time -
											net_send_start_time);

			net_recv_start_time = net_send_end_time;
		}

		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_READ_OP, true);
		pthread_mutex_unlock(&mux_lock);

		if (bfsUtilLayer::perf_test()) {
			net_recv_end_time =
				(double)std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();
			c_read__net_recv_lats.push_back(net_recv_end_time -
											net_recv_start_time);
		}

		rpkt >> bytes_read; // allow short reads (exception thrown if bad len)

		if (bytes_read < _size) {
			logMessage(CLIENT_VRB_LOG_LEVEL, "Client read short [%lu/%lu]\n",
					   bytes_read, _size);
		}

		memcpy(buf, rpkt.getBuffer(), bytes_read);
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what(), path);
		cleanup();
		return BFS_FAILURE;
	}

	if (bfsUtilLayer::perf_test()) {
		c_end_time =
			(double)std::chrono::time_point_cast<std::chrono::microseconds>(
				std::chrono::high_resolution_clock::now())
				.time_since_epoch()
				.count();
		c_read__c_lats.push_back((c_end_time - c_start_time) -
								 (net_recv_end_time - net_recv_start_time) -
								 (net_send_end_time - net_send_start_time));
		c_read__lats.push_back(c_end_time - c_start_time);
	}

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client read OK.\n");
	return (uint32_t)bytes_read;
}

/**
 * @brief Write data to an open file. Serializes and sends a request message to
 * the bfs server, then expects a serialized response containing a return code
 * indicating the number of bytes written.
 *
 * @param path: unused
 * @param buf: buffer containing writes bytes
 * @param size: length of the write
 * @param offset: offset in file to start writing
 * @param fi: object containing handle for the open file
 * @return int: the number of bytes written if success, BFS_FAILURE if failure
 */
static int bfs_write_helper(const char *path, const char *buf, size_t orig_size,
							off_t orig_offset, struct fuse_file_info *fi,
							int wb_force_flush) {
	bool force_flush = false;
	size_t total_written_chunks = 0;

	// logMessage(LOG_ERROR_LEVEL, "Total dirty chunks: %lu\n",
	// 		   total_dirty_chunks);
	// logMessage(LOG_ERROR_LEVEL, "Congestion threshold: %lu\n",
	// 		   CONGESTION_THRESHOLD);

	// Check if the file is being flushed. If so, write the data to the server.
	// Otherwise, write the data to the file cache.
	if (!direct_io_flag && !fi->flush) {
		// if (pthread_mutex_lock(&writeback_lock) != 0) {
		// 	logMessage(LOG_ERROR_LEVEL, "Failed to acquire writeback lock\n");
		// 	return BFS_FAILURE;
		// }
		writeback_lock.lock();

		// Write the data to the local file at the correct offset.
		size_t size = orig_size;
		// logMessage(LOG_ERROR_LEVEL, "cached write size: %lu\n", size);
		off_t offset = orig_offset;
		int local_fd = file_cache[fi->fh];
		int wb = pwrite(local_fd, buf, size, offset);
		if (wb < 0) {
			logMessage(LOG_ERROR_LEVEL, "Failed to write to local file: %s\n",
					   path);
			return BFS_FAILURE;
		}

		// total_dirty_chunks += size / chunk_size + (size % chunk_size != 0);
		total_written_chunks = size / chunk_size + (size % chunk_size != 0);

		// mark all chunks between offset and offset+size as dirty (using
		// dirty_chunks)
		size_t start_block = offset / chunk_size;
		for (size_t i = start_block; i < start_block + total_written_chunks;
			 i++) {
			// check if the chunk is already dirty. if so, skip it
			if (dirty_chunks[fi->fh]->find(i) != dirty_chunks[fi->fh]->end())
				continue;
			else {
				dirty_chunks[fi->fh]->insert(i);
				total_dirty_chunks++;
			}
		}

		// Instead of doing writebacks in a background thread, force the client
		// to flush dirty chunks if we are beyond the congestion threshold. Then
		// allow them to proceed with the write.
		// Edit: dont do this; only do wb in bg thread for now
		assert(!wb_force_flush);
		// force_flush = total_dirty_chunks >= CONGESTION_THRESHOLD * 7.5
		// / 10.0;

		// if (pthread_mutex_unlock(&writeback_lock) != 0) {
		// 	logMessage(LOG_ERROR_LEVEL, "Failed to release writeback lock\n");
		// 	return BFS_FAILURE;
		// }
		writeback_lock.unlock();

		// if (!force_flush)
		return wb;
		// Otherwise continue below to flush the dirty chunks
	}

	// Otherwise we should check who called this function. If it was the
	// writeback thread, then we dont necessarily want to flush all the dirty
	// chunks.
	if (wb_force_flush)
		force_flush = total_dirty_chunks >= CONGESTION_THRESHOLD;

	// if (pthread_mutex_lock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to acquire writeback lock\n");
	// 	return BFS_FAILURE;
	// }
	writeback_lock.lock();

	// Otherwise if we are doing direct_io OR we are flushing data (called by
	// flush/fsync), then just write out the data (just size for direct_io but
	// the entire file for flush/fsync)
	size_t size = 0;
	off_t offset = 0;
	if (direct_io_flag) {
		size = orig_size;
		offset = orig_offset;
	} else {
		// get the size of the local file
		struct stat stbuf;
		// if (pthread_mutex_lock(&writeback_lock) != 0) {
		// 	logMessage(LOG_ERROR_LEVEL, "Failed to acquire writeback lock\n");
		// 	return BFS_FAILURE;
		// }
		int rr = fstat(file_cache[fi->fh], &stbuf);
		// if (pthread_mutex_unlock(&writeback_lock) != 0) {
		// 	logMessage(LOG_ERROR_LEVEL, "Failed to release writeback lock\n");
		// 	return BFS_FAILURE;
		// }
		if (rr < 0) {
			logMessage(LOG_ERROR_LEVEL, "Failed to get file size: %s\n", path);
			return BFS_FAILURE;
		}
		size = stbuf.st_size;
		offset = 0;
	}

	// Break it into 1MB size write chunks (same as NFS transfer size)
	size_t bytes_written = 0;
	size_t total_bytes_written = 0;
	size_t total_size = size;
	char *write_buf = (char *)malloc(total_size);
	std::vector<uint64_t> dirty_chunks_local;

begin_write:
	bytes_written = 0;
	total_bytes_written = 0;
	total_size = size;
	memset(write_buf, 0, total_size);

	// prep the write buffer then release lock, rather then doing network sends
	// while holding lock
	size_t write_bytes = 0;
	// if (pthread_mutex_lock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to acquire writeback lock\n");
	// 	return BFS_FAILURE;
	// }
	while ((total_bytes_written < total_size) ||
		   (dirty_chunks[fi->fh]->size() > 0)) {
		write_bytes = std::min(chunk_size, total_size - total_bytes_written);

		if (fi->flush || force_flush) {
			// if (force_flush)
			// 	logMessage(LOG_ERROR_LEVEL,
			// 			   "Forcing writeback of dirty chunks in file %lu\n",
			// 			   fi->fh);
			// if (fi->flush && !wb_force_flush)
			// 	logMessage(LOG_ERROR_LEVEL,
			// 			   "Flushing dirty chunks in file %lu (%d dirty)\n",
			// 			   fi->fh, dirty_chunks[fi->fh]->size());

			// check if the current chunk is dirty or not
			if (dirty_chunks[fi->fh]->find((offset + total_bytes_written) /
										   chunk_size) ==
				dirty_chunks[fi->fh]->end()) {
				// skip writing this chunk if it is not dirty
				logMessage(CLIENT_VRB_LOG_LEVEL,
						   "Skipping write for non-dirty chunk\n");
				total_bytes_written += write_bytes;
				continue;
			}

			// otherwise remove the chunk from the dirty set and write it
			dirty_chunks[fi->fh]->erase(dirty_chunks[fi->fh]->find(
				(offset + total_bytes_written) / chunk_size));
			dirty_chunks_local.push_back((offset + total_bytes_written) /
										 chunk_size);

			if (pread(file_cache[fi->fh], &write_buf[total_bytes_written],
					  write_bytes, offset + total_bytes_written) < 0) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failed to read from local file: %s\n", path);
				return BFS_FAILURE;
			}

			if (fi->flush || force_flush) {
				total_dirty_chunks--;

				// We always flush the file entirely when flush is explicitly
				// called (eg from release/fsync). But for force flush, we only
				// flush dirty chunks until we are below the congestion
				// threshold. We cannot do the same for the former case because
				// otherwise there will still be some dirty/unflushed file
				// chunks on the client (and we see in the logs that the client
				// total_dirty_chunks begins to creep up as the file is closed
				// and reopened under different file handles).
				if (force_flush) {
					// break if either we are below the dirty threshold or we've
					// written 1% of dirty chunks
					if (total_dirty_chunks * 1.0 <
						CONGESTION_THRESHOLD * 8.0 / 10.0) {
						logMessage(LOG_ERROR_LEVEL,
								   "Early breaking from force flush\n");
						break;
					}
					// if (total_bytes_written * 1.0 >=
					// 	total_dirty_chunks * chunk_size * 1.0 / 100.0)
					// 	break;
				}
			}
		} else {
			memcpy(&write_buf[total_bytes_written], buf + total_bytes_written,
				   write_bytes);
		}
		total_bytes_written += write_bytes;
	}
	// if (pthread_mutex_unlock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to release writeback lock\n");
	// 	return BFS_FAILURE;
	// }
	writeback_lock.unlock();

	total_bytes_written = 0;
	total_written_chunks = 0;
	while (1) {
		write_bytes = std::min(chunk_size, total_size - total_bytes_written);
		uint64_t _size = write_bytes;
		uint64_t _offset = 0;

		if (fi->flush || force_flush) {
			// logMessage(
			// 	LOG_ERROR_LEVEL,
			// 	"Finishing flushing dirty chunks in file %lu (%d dirty)\n",
			// 	fi->fh, dirty_chunks_local.size());
			if (dirty_chunks_local.size() == 0) {
				// logMessage(LOG_ERROR_LEVEL, "No more dirty chunks to flush\n");
				break;
			}
			_offset = dirty_chunks_local[0] * chunk_size;
			total_written_chunks++;
			dirty_chunks_local.erase(dirty_chunks_local.begin());
		} else if (total_bytes_written < total_size)
			_offset = offset + total_bytes_written;
		else
			break;

		int32_t mtype = INVALID_MSG, otype = INVALID_OP,
				total_send_msg_len =
					(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(fi->fh) +
							   sizeof(_size) + sizeof(_offset)),
				total_recv_msg_len =
					(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(_size));
		// size_t bytes_written = 0;
		bfsFlexibleBuffer spkt, rpkt;
		double c_start_time = 0.0, c_end_time = 0.0, net_send_start_time = 0.0,
			   net_send_end_time = 0.0, net_recv_start_time = 0.0,
			   net_recv_end_time = 0.0;

		if (bfsUtilLayer::perf_test())
			c_start_time =
				(double)std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();

		// logMessage(LOG_ERROR_LEVEL, "fi->flush: %d\n", fi->flush);
		// logMessage(LOG_ERROR_LEVEL,
		// 		   "Trying client write [path: %s, size: %lu, off: %lu].\n",
		// 		   path, write_bytes, _offset);
		try {
			// read, write, release use handles when communicating as opposed to
			// pathname Send write request
			assert(fi->fh >= START_FD); // sanity check; should be an open file
			spkt.resetWithAlloc((uint32_t)_size, 0, total_send_msg_len, 0);
			spkt.setData(&write_buf[total_bytes_written], (uint32_t)_size);
			mtype = TO_SERVER;
			otype = CLIENT_WRITE_OP;
			spkt << _offset << _size << fi->fh << otype << mtype;

			pthread_mutex_lock(&mux_lock);
			bfsFlexibleBuffer *aad =
				new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
			secContext->encryptData(spkt, aad, true);
			send_seq++;

			if (bfsUtilLayer::perf_test())
				net_send_start_time =
					(double)
						std::chrono::time_point_cast<std::chrono::microseconds>(
							std::chrono::high_resolution_clock::now())
							.time_since_epoch()
							.count();

			send_msgp(spkt);
			delete aad;

			if (bfsUtilLayer::perf_test()) {
				net_send_end_time =
					(double)
						std::chrono::time_point_cast<std::chrono::microseconds>(
							std::chrono::high_resolution_clock::now())
							.time_since_epoch()
							.count();
				c_write__net_send_lats.push_back(net_send_end_time -
												 net_send_start_time);

				net_recv_start_time = net_send_end_time;
			}

			recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_WRITE_OP,
					  true);
			pthread_mutex_unlock(&mux_lock);

			if (bfsUtilLayer::perf_test()) {
				net_recv_end_time =
					(double)
						std::chrono::time_point_cast<std::chrono::microseconds>(
							std::chrono::high_resolution_clock::now())
							.time_since_epoch()
							.count();
				c_write__net_recv_lats.push_back(net_recv_end_time -
												 net_recv_start_time);
				logMessage(CLIENT_VRB_LOG_LEVEL, "recv latency: %f\n",
						   net_recv_end_time - net_recv_start_time);
			}

			// Edit: dont allow short writes for simplicity
			rpkt >> bytes_written;

			if (bytes_written < _size) {
				logMessage(
					LOG_ERROR_LEVEL,
					"Client write short [path: %s, %d/%d bytes written]\n",
					path, bytes_written, size);
				return BFS_FAILURE;
			}
		} catch (bfsCryptoError *err) {
			logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
					   err->getMessage().c_str());
			delete err;

			return BFS_FAILURE;
		} catch (bfsUtilError *err) {
			logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
			delete err;
			return BFS_FAILURE;
		} catch (
			std::runtime_error &re) { // system errors (ie corrupt data stream)
			if (strlen(re.what()) > 0)
				logMessage(LOG_ERROR_LEVEL, re.what(), path);
			cleanup();
			return BFS_FAILURE;
		}

		if (bfsUtilLayer::perf_test()) {
			c_end_time =
				(double)std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();
			c_write__c_lats.push_back(
				(c_end_time - c_start_time) -
				(net_recv_end_time - net_recv_start_time) -
				(net_send_end_time - net_send_start_time));
			c_write__lats.push_back(c_end_time - c_start_time);
			logMessage(CLIENT_VRB_LOG_LEVEL, "write latency: %f\n",
					   c_end_time - c_start_time);
		}

		total_bytes_written += write_bytes;
		// if (fi->flush || force_flush) {
		// 	total_dirty_chunks--;

		// 	// We always flush the file entirely when flush is explicitly called
		// 	// (eg from release/fsync). But for force flush, we only flush dirty
		// 	// chunks until we are below the congestion threshold. We cannot do
		// 	// the same for the former case because otherwise there will still
		// 	// be some dirty/unflushed file chunks on the client (and we see in
		// 	// the logs that the client total_dirty_chunks begins to creep up as
		// 	// the file is closed and reopened under different file handles).
		// 	if (force_flush) {
		// 		// break if either we are below the dirty threshold or we've
		// 		// written 1% of dirty chunks
		// 		if (total_dirty_chunks * 1.0 <
		// 			CONGESTION_THRESHOLD * 8.0 / 10.0) {
		// 			logMessage(LOG_ERROR_LEVEL,
		// 					   "Early breaking from force flush\n");
		// 			break;
		// 		}
		// 		// if (total_bytes_written * 1.0 >=
		// 		// 	total_dirty_chunks * chunk_size * 1.0 / 100.0)
		// 		// 	break;
		// 	}
		// }
	}
	free(write_buf);

	// Once we finished the force_flush, we should go back up to the top and
	// write the data in buf if necessary (only necessary for directio, since
	// buffered io will go through the cache above)
	// Edit: NMV. The loop should only go through at most once. Buffered io will
	// go through the cache then force flush through the loop once if necessary.
	// Explicit flushes will go through the loop once. Direct io will go
	// directly through the loop.
	assert(fi->flush || force_flush || direct_io_flag);
	// if (direct_io_flag)
	// 	goto begin_write;

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client write OK.\n");
	if (direct_io_flag)
		return (int)total_bytes_written;
	else
		return BFS_SUCCESS;
}
int bfs_write(const char *path, const char *buf, size_t orig_size,
			  off_t orig_offset, struct fuse_file_info *fi) {
	int ret = 0;

	// if (pthread_mutex_lock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to acquire writeback lock\n");
	// 	return BFS_FAILURE;
	// }

	ret = bfs_write_helper(path, buf, orig_size, orig_offset, fi, false);

	// if (pthread_mutex_unlock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to release writeback lock\n");
	// 	return BFS_FAILURE;
	// }

	return ret;
}

/**
 * @brief Release an open regular file. For every open() call there will be
 * exactly one release() call (see fuse.h). Serializes and sends a request
 * message to the bfs server, then expects a serialized response containing a
 * return code for the request.
 *
 * @param path: unused
 * @param fi: object containing context (file handle) for the open file
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int bfs_release(const char *path, struct fuse_file_info *fi) {
	int32_t mtype = INVALID_MSG, otype = INVALID_OP, ret = BFS_FAILURE,
			total_send_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(fi->fh)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;

	double release_start_time = 0.0, release_end_time = 0.0;

	// if (pthread_mutex_lock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to acquire writeback lock\n");
	// 	return BFS_FAILURE;
	// }

    // release_start_time =
	// 			(double)std::chrono::time_point_cast<std::chrono::microseconds>(
	// 				std::chrono::high_resolution_clock::now())
	// 				.time_since_epoch()
	// 				.count();

	writeback_lock.lock();

	// Delete the file from the cache and delete the local file
	if (file_cache.find(fi->fh) != file_cache.end()) {
		// close the local file
		int rr = close(file_cache[fi->fh]);
		if (rr < 0) {
			logMessage(LOG_ERROR_LEVEL, "Failed to close local file: %s\n",
					   path);
			return BFS_FAILURE;
		}
		// num_files_closed++;
		// logMessage(LOG_ERROR_LEVEL, "num_files_closed: %d\n",
		// num_files_closed);

		if (dirty_chunks.find(fi->fh) != dirty_chunks.end()) {
			delete dirty_chunks[fi->fh];
			dirty_chunks.erase(fi->fh);
		}

		file_cache.erase(fi->fh);
        // logMessage(LOG_ERROR_LEVEL, "Number of open files: %lu\n", file_cache.size());
	}

	// if (pthread_mutex_unlock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to release writeback lock\n");
	// 	return BFS_FAILURE;
	// }
	writeback_lock.unlock();

    // release_end_time =
    //     (double)std::chrono::time_point_cast<std::chrono::microseconds>(
    //         std::chrono::high_resolution_clock::now())
    //         .time_since_epoch()
    //         .count();

	// logMessage(LOG_ERROR_LEVEL, "release latency: %.3f\n",
	// 		   release_end_time - release_start_time);

	logMessage(CLIENT_VRB_LOG_LEVEL, "Trying client release [%s].\n", path);
	try {
		// read, write, release use handles when communicating as opposed to
		// pathname Send release request
		assert(fi->fh >= START_FD); // sanity check; should be an open file
		spkt.resetWithAlloc(0, 0, total_send_msg_len, 0);
		mtype = TO_SERVER;
		otype = CLIENT_RELEASE_OP;
		spkt << fi->fh << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;

		if (bfsUtilLayer::perf_test())
			release_start_time =
				(double)std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();

		send_msgp(spkt);

		if (bfsUtilLayer::perf_test())
			release_end_time =
				(double)std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();

		logMessage(CLIENT_VRB_LOG_LEVEL, "send latency: %f\n",
				   release_end_time - release_start_time);

		delete aad;

		// Receive release response
		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_RELEASE_OP,
				  false);
		pthread_mutex_unlock(&mux_lock);
		rpkt >> ret;
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what(), path);
		cleanup();
		return BFS_FAILURE;
	}

	if (ret != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Client release request failed: %s\n",
				   path);
		return ret;
	}

	// if (bfsUtilLayer::perf_test())

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client release OK.\n");
	return BFS_SUCCESS;
}

/**
 * @brief Release an open directory file.
 *
 * @param path: unused
 * @param fi: object containing context (file handle) for the open file
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int bfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// route both directory file releases to same routine
	return bfs_release(path, fi);
}

int bfs_flush(const char *path, struct fuse_file_info *fi) {
	// Implement flush by calling bfs_write to write the data to the server
	fi->flush = true;
	int ret = bfs_write(path, NULL, 0, 0, fi);
	fi->flush = false;
	return ret;
}

/**
 * @brief Synchronize in-memory and on-disk file contents. Serializes and sends
 * a request message to the bfs server, then expects a serialized response
 * containing a return code for the request.
 *
 * @param path: unused
 * @param fi: object containing context (file handle) for the open file
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int bfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) {
	// Implement fsync by calling bfs_write to write the data to the server
	fi->flush = true;
	int ret = bfs_write(path, NULL, 0, 0, fi);
	fi->flush = false;
	return ret;

	/*
	int32_t mtype = INVALID_MSG, otype = INVALID_OP, ret = BFS_FAILURE,
			datasync = 1,
			total_send_msg_len = (uint32_t)(sizeof(mtype) + sizeof(otype) +
											sizeof(fi->fh) + sizeof(datasync)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;

	double fsync_start_time = 0.0, fsync_end_time = 0.0;
	logMessage(CLIENT_VRB_LOG_LEVEL, "fsync starting\n");

	try {
		// Send fsync request
		assert(fi->fh >= START_FD); // sanity check; should be an open file
		spkt.resetWithAlloc(0, 0, total_send_msg_len, 0);
		mtype = TO_SERVER;
		otype = CLIENT_FSYNC_OP;
		spkt << datasync << fi->fh << otype << mtype;
		pthread_mutex_lock(&mux_lock); bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;

		if (bfsUtilLayer::perf_test())
			fsync_start_time =
				(double)std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();

		send_msgp(spkt);

		if (bfsUtilLayer::perf_test())
			fsync_end_time =
				(double)std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();

		logMessage(CLIENT_VRB_LOG_LEVEL, "send latency: %f\n",
				   fsync_end_time - fsync_start_time);

		delete aad;

		// Receive fsync response
		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_FSYNC_OP,
				  false);

		if (bfsUtilLayer::perf_test())
			fsync_end_time =
				(double)std::chrono::time_point_cast<std::chrono::microseconds>(
					std::chrono::high_resolution_clock::now())
					.time_since_epoch()
					.count();
		logMessage(CLIENT_VRB_LOG_LEVEL, "recv latency: %f\n",
				   fsync_end_time - fsync_start_time);

		pthread_mutex_unlock(&mux_lock); rpkt >> ret;
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what(), path);
		cleanup();
		return BFS_FAILURE;
	}

	if (ret != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Client fsync request failed: %s\n", path);
		return ret;
	}

	if (bfsUtilLayer::perf_test())
		fsync_end_time =
			(double)std::chrono::time_point_cast<std::chrono::microseconds>(
				std::chrono::high_resolution_clock::now())
				.time_since_epoch()
				.count();

	logMessage(CLIENT_VRB_LOG_LEVEL, "fsync latency: %f\n",
			   fsync_end_time - fsync_start_time);

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client fsync OK.\n");
	return BFS_SUCCESS;
	*/
}

/**
 * @brief Open a directory file. Serializes and sends a request message to the
 * bfs server, then expects a serialized response containing a file handle for
 * the open directory file (to be used in a readdir operation).
 *
 * @param path: absolute path for the directory file
 * @param fi: object containing context (file handle) for the open file
 * @return int: BFS_SUCCESS if success, BFS_FAILURE/-ENOENT if failure
 */
int bfs_opendir(const char *path, struct fuse_file_info *fi) {
	bfs_fh_t ret = BFS_FAILURE;
	int32_t mtype = INVALID_MSG, otype = INVALID_OP,
			path_len = (uint32_t)(strlen(path) + 1),
			total_send_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(path_len)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;

	try {
		// Send opendir request
		spkt.resetWithAlloc(path_len, 0, total_send_msg_len, 0);
		spkt.setData(path, path_len);
		mtype = TO_SERVER;
		otype = CLIENT_OPENDIR_OP;
		spkt << path_len << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;
		send_msgp(spkt);
		delete aad;

		// Receive opendir response
		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_OPENDIR_OP,
				  false);
		pthread_mutex_unlock(&mux_lock);
		rpkt >> ret;
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what(), path);
		cleanup();
		return BFS_FAILURE;
	}

	if ((int64_t)ret <
		ROOT_INO) { // here we can allow the open to happen on root dir too
		logMessage(LOG_ERROR_LEVEL, "Client opendir request failed: %s\n",
				   path);
		return (int)ret;
	}

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client opendir OK.\n");
	fi->fh = (bfs_fh_t)ret;

	return BFS_SUCCESS;
}

/**
 * @brief Read a directory. Uses the file handle given by an opendir call.
 * Serializes and sends a request message to the bfs server, then expects a
 * serialized response containing a directory entries.
 *
 * @param path: unused
 * @param buf: buffer to fill with directory entries
 * @param filler: function called to fill the buffer
 * @param offset: unused
 * @param fi: object containing context (file handle) for the open file
 * @param flags: unused
 * @return int:  int: BFS_SUCCESS if success, BFS_FAILURE/-ENOENT if failure
 */
int bfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
				off_t offset, struct fuse_file_info *fi,
				enum fuse_readdir_flags flags) {
	(void)offset;
	(void)flags;
	int32_t mtype = INVALID_MSG, otype = INVALID_OP;
	uint64_t de_data_off = 0, de_off = 0, num_ents = 0;
	uint32_t recv_atime, recv_mtime, recv_ctime;
	uint32_t dentry_path_len = 0,
			 total_send_msg_len =
				 (uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(fi->fh)),
			 total_recv_hdr_len =
				 (uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(num_ents)),
			 de_len = (uint32_t)(sizeof(uint64_t) + sizeof(uint32_t) +
								 sizeof(uint64_t) + sizeof(dentry_path_len) +
								 sizeof(uint32_t) * 3),
			 total_recv_data_len = 0;
	char *read_path_copy = bfs_strdup(path),
		 *dirname = bfs_dirname(read_path_copy), *dentry_path = NULL;
	struct stat *st = NULL;
	bfsFlexibleBuffer spkt, rpkt;

	logMessage(CLIENT_VRB_LOG_LEVEL, "Trying client readdir [%s].\n", path);
	try {
		// Send readdir request
		assert(fi->fh >= START_FD); // sanity check; should be an open file
		spkt.resetWithAlloc(0, 0, total_send_msg_len, 0);
		mtype = TO_SERVER;
		otype = CLIENT_READDIR_OP;
		spkt << fi->fh << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;
		send_msgp(spkt);
		delete aad;

		// Receive readdir response
		recv_msgp(rpkt, total_recv_hdr_len, FROM_SERVER, CLIENT_READDIR_OP,
				  false);
		pthread_mutex_unlock(&mux_lock);

		// get number of entries
		rpkt >> num_ents;
		total_recv_data_len = (uint32_t)(num_ents * MAX_FILE_NAME_LEN);
		de_off = total_recv_data_len - MAX_FILE_NAME_LEN;
		rpkt.resetWithAlloc(total_recv_data_len, 0,
							(uint32_t)(num_ents * de_len), 0);

		// reuse pkt
		recv_msgp(rpkt, (uint32_t)(total_recv_data_len + num_ents * de_len),
				  INVALID_MSG, INVALID_OP, false);

		// Read the dentries. Assumes the message is structured like:
		// Headers: mtype | otype | num_ents | ino0 | mode0 | size0 | ino1 | ...
		// Data: | name0 | name1 | ...
		for (uint32_t i = 0; i < num_ents; i++) {
			st = (struct stat *)calloc(sizeof(struct stat), 1);

			rpkt >> recv_atime >> recv_mtime >> recv_ctime >> st->st_ino >>
				st->st_mode >> st->st_size >> dentry_path_len;
			st->st_atime = recv_atime;
			st->st_mtime = recv_mtime;
			st->st_ctime = recv_ctime;
			st->st_gid = 1000;
			st->st_uid = 1000;

			// Now update data offset (we pull headers off so the actual data
			// index should keep decreasing as i increases). Also update the de
			// offset after, since the headers are removed in reverse order, we
			// need to read from the end of the buffer (last dentries added from
			// server) to the front.
			de_data_off = (num_ents - i - 1) * de_len;

			dentry_path = new char[dentry_path_len];
			memcpy(dentry_path, &(rpkt.getBuffer()[de_data_off + de_off]),
				   dentry_path_len);				 // includes \0
			dentry_path[dentry_path_len - 1] = '\0'; // just make sure
			assert(rpkt.getBuffer()[de_data_off + de_off + dentry_path_len] ==
				   '\0'); // expected after end of path name
			de_off -= MAX_FILE_NAME_LEN;

			if (st->st_ino < FIRST_UNRESERVED_INO) {
				// if path is root dir
				if ((memcmp(path, "/", 2) == 0)) {
					if ((memcmp(dentry_path, ".", 2) != 0) &&
						(memcmp(dentry_path, "..", 3) != 0)) {
						// if the dentry is not special (special dirs . and ..
						// point to the root inode number 2)
						logMessage(LOG_ERROR_LEVEL,
								   "Client readdir entry request failed for "
								   "root: path=%s, "
								   "entry=%s, "
								   "inode=%d, "
								   "size==%d ",
								   path, dentry_path, st->st_ino, st->st_size);
						free(st);
						delete dentry_path;
						free(read_path_copy);
					}
				} else { // else if path is not root dir
					// the test parent should be of 'path'
					// if the dentry is parent and parent inode is <START, then
					// the parent must be the root, otherwise fail
					if ((memcmp(dentry_path, "..", 3) != 0) ||
						(memcmp(dirname, "/", 2) != 0)) {
						logMessage(LOG_ERROR_LEVEL,
								   "Client readdir entry request failed: "
								   "path=%s, entry=%s, "
								   "inode=%d, "
								   "size=%d",
								   path, dentry_path, st->st_ino, st->st_size);
						free(st);
						delete dentry_path;
						free(read_path_copy);
						break;
					}
				}
			}

			logMessage(CLIENT_LOG_LEVEL,
					   "Client readdir entry OK: path=%s, entry=%s, "
					   "inode=%d, "
					   "size=%d",
					   path, dentry_path, st->st_ino, st->st_size);

			if (filler(buf, dentry_path, st, 0, (enum fuse_fill_dir_flags)0) !=
				0) {
				break; // buffer full
			}
		}
		free(read_path_copy);
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what(), path);
		cleanup();
		return BFS_FAILURE;
	}

	logMessage(CLIENT_LOG_LEVEL, "Client readdir OK: %s\n", path);
	return BFS_SUCCESS;
}

static void *client_wb_worker_entry(void *arg) {
	// This function just loops in the background through the file cache and
	// writes out dirty file chunks to the server.
	uint64_t total_dirty_chunks_begin = 0;
	while (1) {
		if (!client_status)
			break;

		// sleep for 5 s
		usleep(5000000);

		// This races to read total_dirty_chunks but this is fine because this
		// writeback is heuristic anyway.
		if (total_dirty_chunks >= CONGESTION_THRESHOLD) {
			// Grab the lock so we can access file_cache and dirty_chunks
			// if (pthread_mutex_lock(&writeback_lock) != 0) {
			// 	logMessage(LOG_ERROR_LEVEL,
			// 			   "Failed to acquire writeback lock\n");
			// 	return NULL;
			// }

			total_dirty_chunks_begin = total_dirty_chunks;

			// pick a file randomly from the file cache
			bfs_fh_t f = 0;
			for (auto it = file_cache.begin(); it != file_cache.end(); it++) {
				if (dirty_chunks.find(it->first) != dirty_chunks.end()) {
					f = it->first;
					break;
				}
			}

			logMessage(LOG_ERROR_LEVEL,
					   "Writing back dirty chunks in file %lu\n", f);
			struct fuse_file_info fi;
			fi.fh = f;
			fi.flush = true;
			if (bfs_write_helper(NULL, NULL, 0, 0, &fi, true) != BFS_SUCCESS) {
				logMessage(LOG_ERROR_LEVEL,
						   "Failed to write back dirty chunks in file %lu\n",
						   f);
				return NULL;
			}

			logMessage(LOG_ERROR_LEVEL,
					   "Done writing back dirty chunks in file %lu\n", f);
			logMessage(LOG_ERROR_LEVEL, "Wrote %lu dirty chunks, %lu left\n",
					   total_dirty_chunks_begin - total_dirty_chunks,
					   total_dirty_chunks);

			// if (pthread_mutex_unlock(&writeback_lock) != 0) {
			// 	logMessage(LOG_ERROR_LEVEL,
			// 			   "Failed to release writeback lock\n");
			// 	return NULL;
			// }
		}
	}

	return NULL;
}

/**
 * @brief Initialize the bfs system. Serializes and sends a request message to
 * the bfs server, then expects a serialized response containing a return code
 * for the request.
 *
 * @param conn: unused (fuse user->kernel connection info)
 * @param cfg: unused (configuration settings for fuse)
 * @return void*: buffer passed in the `private_data` field of `struct
 * fuse_context` to all file operations
 */
void *bfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
	(void)conn;

	// disable both file _metadata_ and _data_ caching for perf tests so we can
	// get per-read/write latency
	// add NULL checks for client_test to not throw error
	if (cfg) {
		cfg->use_ino = 1;
		// cfg->hard_remove = 1;
		if (direct_io_flag) {
			cfg->direct_io = 1;
			cfg->ac_attr_timeout = 0;
			cfg->attr_timeout = 0;
			cfg->kernel_cache = 0;
			// conn->max_background = 0;
			// conn->max_readahead = 524288;
			// conn->want |= FUSE_CAP_WRITEBACK_CACHE;
			conn->want |= FUSE_CAP_ASYNC_DIO;
			conn->max_write = 1048576;
		} else {
			cfg->direct_io = 0;
			cfg->ac_attr_timeout = 600;
			cfg->attr_timeout = 600;
			cfg->kernel_cache = 1;
			conn->max_background = 8;
			// conn->max_readahead = 524288;
			conn->want |= FUSE_CAP_WRITEBACK_CACHE;
			// conn->want &=
			// 	~FUSE_CAP_WRITEBACK_CACHE; // disable so writes are faster; our
			// read cache should be fast enough
			// anyway so we dont need this
			// conn->want |= FUSE_CAP_ASYNC_DIO;
			conn->max_write = 1048576;
		}
	}

	// if (conn)
	// 	conn->max_readahead = 0;

	if (conn && cfg) {
		logMessage(LOG_ERROR_LEVEL, "conn->max_readahead: %d\n",
				   conn->max_readahead);
		logMessage(LOG_ERROR_LEVEL, "cfg->direct_io: %d\n", cfg->direct_io);
		logMessage(LOG_ERROR_LEVEL, "cfg->ac_attr_timeout: %d\n",
				   cfg->ac_attr_timeout);
		logMessage(LOG_ERROR_LEVEL, "cfg->attr_timeout: %d\n",
				   cfg->attr_timeout);
		logMessage(LOG_ERROR_LEVEL, "conn->max_background: %d\n",
				   conn->max_background);
		logMessage(LOG_ERROR_LEVEL, "cfg->kernel_cache: %d\n",
				   cfg->kernel_cache);
		logMessage(LOG_ERROR_LEVEL, "cfg->auto_cache: %d\n", cfg->auto_cache);
		logMessage(LOG_ERROR_LEVEL, "cfg->ac_attr_timeout_set: %d\n",
				   cfg->ac_attr_timeout_set);
		logMessage(LOG_ERROR_LEVEL, "cfg->entry_timeout: %d\n",
				   cfg->entry_timeout);
		if (conn->capable & FUSE_CAP_WRITEBACK_CACHE) {
			logMessage(LOG_ERROR_LEVEL, "FUSE_CAP_WRITEBACK_CACHE supported\n");
		}
		logMessage(LOG_ERROR_LEVEL, "FUSE_CAP_WRITEBACK_CACHE: %d\n",
				   conn->want & FUSE_CAP_WRITEBACK_CACHE);
		if (conn->capable & FUSE_CAP_ASYNC_DIO) {
			logMessage(LOG_ERROR_LEVEL, "FUSE_CAP_ASYNC_DIO supported\n");
		}
		logMessage(LOG_ERROR_LEVEL, "FUSE_CAP_ASYNC_DIO: %d\n",
				   conn->want & FUSE_CAP_ASYNC_DIO);
		logMessage(LOG_ERROR_LEVEL, "negative_timeout: %d\n",
				   cfg->negative_timeout);
	}

	int32_t mtype = INVALID_MSG, otype = INVALID_OP, ret = BFS_FAILURE,
			total_send_msg_len = (uint32_t)(sizeof(mtype) + sizeof(otype)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;

	try {
		logMessage(CLIENT_LOG_LEVEL, "Initializing client...\n");

		client = bfsNetworkConnection::bfsChannelFactory(bfs_server_ip,
														 bfs_server_port);
		logMessage(CLIENT_LOG_LEVEL, "Connected to server [%s:%d]\n",
				   bfs_server_ip.c_str(), bfs_server_port);

		if (client->connect() != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL, "Client connection failed, aborting.");
			abort();
		}

		mux = new bfsConnectionMux();
		mux->addConnection(client);

		if (pthread_mutex_init(&mux_lock, NULL) != 0) {
			logMessage(LOG_ERROR_LEVEL,
					   "Client failed to initialize mux lock, aborting.");
			abort();
		}

		// pthread_mutexattr_t attr;
		// pthread_mutexattr_init(&attr);
		// pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		// pthread_mutex_init(&writeback_lock, NULL);
		// if (pthread_mutex_init(&writeback_lock, NULL) != 0) {
		// 	logMessage(LOG_ERROR_LEVEL,
		// 			   "Client failed to initialize writeback lock, aborting.");
		// 	abort();
		// }

		// Send init request
		spkt.resetWithAlloc(0, 0, total_send_msg_len, 0);
		mtype = TO_SERVER;
		otype = do_mkfs ? CLIENT_INIT_MKFS_OP : CLIENT_INIT_OP;
		spkt << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;
		send_msgp(spkt);
		delete aad;

		// Receive init response
		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_INIT_OP, false);
		pthread_mutex_unlock(&mux_lock);
		rpkt >> ret; // mtype and otype already pulled out
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return NULL;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what());
		cleanup();
		return NULL;
	}

	if (ret != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Client init request failed.\n");
		return NULL;
	}

	client_status = 1;

	// Now that connection to server was OK, we should spawn the writeback
	// thread.
	if (!direct_io_flag) {
		if (pthread_create(&writeback_thread, NULL, client_wb_worker_entry,
						   NULL) != 0) {
			logMessage(LOG_ERROR_LEVEL,
					   "Failed to spawn writeback thread, aborting.");
			abort();
		}
	}

	logMessage(CLIENT_LOG_LEVEL, "Client initialization OK.\n");

	return NULL;
}

/**
 * @brief Clean up filesystem. Called on filesystem exit. Serializes and sends a
 * request message to the bfs server, then expects a serialized response
 * containing a return code for the request.
 *
 * @param private_data: unused (buffer containing interal data for bfs client)
 */
void bfs_destroy(void *private_data) {
	(void)private_data;
	int32_t mtype = INVALID_MSG, otype = INVALID_OP, ret = BFS_FAILURE,
			total_send_msg_len = (uint32_t)(sizeof(mtype) + sizeof(otype)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;

	client_status = 0;

	if (!direct_io_flag && (pthread_join(writeback_thread, NULL) != 0))
		logMessage(LOG_ERROR_LEVEL, "Failed to join writeback thread\n");

	try {
		// Send destroy request
		spkt.resetWithAlloc(0, 0, total_send_msg_len, 0);
		mtype = TO_SERVER;
		otype = CLIENT_DESTROY_OP;
		spkt << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;
		send_msgp(spkt);
		delete aad;

		// Receive destroy response
		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_DESTROY_OP,
				  false);
		pthread_mutex_unlock(&mux_lock);
		rpkt >> ret;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what());
		return; // dont terminate connection yet
	}

	if (ret != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Client destroy request failed\n");
		return; // dont terminate connection yet
	}

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client destroy OK.\n");

	// log and write the perf results to a file
	write_client_latencies();

	cleanup();
}

/**
 * @brief Create and open a file. If the file does not exist, first create it
 * with the specified mode, and then open it. Serializes and sends a request
 * message to the bfs server, then expects a serialized response containing a
 * file handle for the open file.
 *
 * @param path: absolute path for the regular file
 * @param mode: file mode
 * @param fi: object containing context (file handle) for the open file
 * @return int: BFS_SUCCESS if success, BFS_FAILURE/-ENOENT if failure
 */
int bfs_create(const char *path, uint32_t mode, struct fuse_file_info *fi) {
	bfs_fh_t ret = BFS_FAILURE;
	uint32_t _mode = mode;
	int32_t mtype = INVALID_MSG, otype = INVALID_OP,
			path_len = (uint32_t)(strlen(path) + 1),
			total_send_msg_len = (uint32_t)(sizeof(mtype) + sizeof(otype) +
											sizeof(_mode) + sizeof(path_len)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;

	logMessage(CLIENT_VRB_LOG_LEVEL, "Trying client create [%s].\n", path);
	try {
		// Send create request
		spkt.resetWithAlloc(path_len, 0, total_send_msg_len, 0);
		spkt.setData(path, path_len);
		mtype = TO_SERVER;
		otype = CLIENT_CREATE_OP;
		spkt << path_len << _mode << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;
		send_msgp(spkt);
		delete aad;

		// Receive create response
		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_CREATE_OP,
				  false);
		pthread_mutex_unlock(&mux_lock);
		rpkt >> ret;
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what());
		cleanup();
		return BFS_FAILURE;
	}

	if ((int64_t)ret < START_FD) { // creates must always be >= start_fd
		logMessage(LOG_ERROR_LEVEL, "Client create request failed: %s\n", path);
		return (int)ret;
	}

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client create OK.\n");
	fi->fh = (bfs_fh_t)ret;

	// if (pthread_mutex_lock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to lock writeback lock\n");
	// 	return BFS_FAILURE;
	// }
	writeback_lock.lock();

	// Open a local file with the same name and map the file handle to the bfs
	// file handle
	char *tmp_path = (char *)malloc(strlen(path) + 5);
	strcpy(tmp_path, "/tmp");
	strcat(tmp_path, path);

	char *dir_name = bfs_dirname(bfs_strdup(tmp_path));
	char *mkdir_cmd = (char *)malloc(strlen(dir_name) + 10);
	strcpy(mkdir_cmd, "mkdir -p ");
	strcat(mkdir_cmd, dir_name);
	system(mkdir_cmd);
	chmod(dir_name, 0777);
	free(mkdir_cmd);
	free(dir_name);

	int fd = open(tmp_path, O_RDWR | O_CREAT | O_TRUNC, 0777);
	if (fd < 0) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed to open local file: %s, errno: %s\n", tmp_path,
				   strerror(errno));
		return BFS_FAILURE;
	}
	// num_files_opened++;
	// logMessage(LOG_ERROR_LEVEL, "num_files_opened: %d\n", num_files_opened);

	// char *dd_cmd = (char *)malloc(strlen(tmp_path) + 50);
	// strcpy(dd_cmd, "dd if=/dev/zero of=");
	// strcat(dd_cmd, tmp_path);
	// strcat(dd_cmd, " bs=1M count=1024");
	// system(dd_cmd);
	// free(dd_cmd);

	file_cache[fi->fh] = fd;
	dirty_chunks[fi->fh] = new std::set<uint64_t>();
	free(tmp_path);

	// if (pthread_mutex_unlock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to unlock writeback lock\n");
	// 	return BFS_FAILURE;
	// }
	writeback_lock.unlock();

	return BFS_SUCCESS;
}

int bfs_truncate(const char *path, off_t length, struct fuse_file_info *fi) {
	// This is similar to chmod, but we just need to send the truncate request
	// to the server and the server will truncate the file to the specified
	// length
	(void)fi;
	bfs_fh_t ret = BFS_FAILURE;
	uint32_t _length = length;
	int32_t mtype = INVALID_MSG, otype = INVALID_OP,
			path_len = (uint32_t)(strlen(path) + 1),
			total_send_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(fi->fh) +
						   sizeof(_length) + sizeof(path_len)),
			total_recv_msg_len =
				(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsFlexibleBuffer spkt, rpkt;

	logMessage(CLIENT_VRB_LOG_LEVEL, "Trying client truncate [%s].\n", path);
	try {
		// Send truncate request
		assert(fi->fh >= START_FD); // sanity check; should be an open file
		spkt.resetWithAlloc(path_len, 0, total_send_msg_len, 0);
		spkt.setData(path, path_len);
		mtype = TO_SERVER;
		otype = CLIENT_TRUNCATE_OP;
		spkt << path_len << _length << fi->fh << otype << mtype;
		pthread_mutex_lock(&mux_lock);
		bfsFlexibleBuffer *aad =
			new bfsFlexibleBuffer((char *)&send_seq, sizeof(uint32_t));
		secContext->encryptData(spkt, aad, true);
		send_seq++;
		send_msgp(spkt);
		delete aad;

		// Receive truncate response
		recv_msgp(rpkt, total_recv_msg_len, FROM_SERVER, CLIENT_TRUNCATE_OP,
				  false);
		pthread_mutex_unlock(&mux_lock);
		rpkt >> ret;
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (std::runtime_error &re) { // system errors (ie corrupt data stream)
		if (strlen(re.what()) > 0)
			logMessage(LOG_ERROR_LEVEL, re.what());
		cleanup();
		return BFS_FAILURE;
	}

	if (ret != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Client truncate request failed: %s\n",
				   path);
		return (int)ret;
	}

	logMessage(CLIENT_VRB_LOG_LEVEL, "Client truncate OK.\n");

	// if (pthread_mutex_lock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to lock writeback lock\n");
	// 	return BFS_FAILURE;
	// }
	writeback_lock.lock_shared();

	// Truncate the local file to the specified length
	int fd = file_cache[fi->fh];
	if (ftruncate(fd, length) < 0) {
		logMessage(LOG_ERROR_LEVEL, "Failed to truncate local file: %s\n",
				   path);
		return BFS_FAILURE;
	}

	// if (pthread_mutex_unlock(&writeback_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Failed to unlock writeback lock\n");
	// 	return BFS_FAILURE;
	// }
	writeback_lock.unlock_shared();

	return BFS_SUCCESS;
}

int bfs_fallocate(const char *path, int mode, off_t offset, off_t length,
				  struct fuse_file_info *fi) {
	// (void)path;
	// (void)mode;
	// (void)offset;
	// (void)length;
	// (void)fi;
	// logMessage(LOG_ERROR_LEVEL, "FALLOC called, exiting\n");
	// return -1;

	// We can simulate an fallocate by creating a file with the desired size and
	// then writing to it

	// Create the file
	int ret = bfs_create(path, mode, fi);
	if (ret != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed to create file for fallocate\n");
		return -1;
	}
	logMessage(CLIENT_VRB_LOG_LEVEL, "Client create OK in fallocate.\n");

	// Write to the file
	char *buf = (char *)malloc(offset + length);
	if (buf == NULL) {
		logMessage(LOG_ERROR_LEVEL,
				   "Failed to allocate buffer for fallocate\n");
		return -1;
	}

	ret = bfs_write(path, buf, offset + length, 0, fi);
	if (ret != offset + length) {
		logMessage(LOG_ERROR_LEVEL, "Failed to write to file for fallocate\n");
		free(buf);
		return -1;
	}
	free(buf);
	logMessage(CLIENT_VRB_LOG_LEVEL, "Client write OK in fallocate.\n");

	return 0;
}

off_t bfs_lseek(const char *path, off_t off, int whence,
				struct fuse_file_info *fi) {
	(void)path;
	(void)off;
	(void)whence;
	(void)fi;
	logMessage(LOG_ERROR_LEVEL, "LSEEK called, exiting\n");
	return -1;
}

/**
 * @brief Send a message containing a request to the server.
 *
 * @param len: the number of bytes to send
 * @param buf: buffer containing bytes to send
 * @return uint32_t: 0 if success, throw exception if failure
 */
static void send_msgp(bfsFlexibleBuffer &spkt) {
	int32_t bytes_sent = 0;

	// if (pthread_mutex_lock(&mux_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Error when acquiring lock in send_msg\n");
	// 	return;
	// }

	// bytes_sent = client->sendPacketizedDataL(len, buf);
	bytes_sent = client->sendPacketizedBuffer(spkt);

	// if (pthread_mutex_unlock(&mux_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Error when releasing lock in send_msg\n");
	// 	abort(); // very bad
	// }

	if ((bytes_sent < 0) || ((uint32_t)bytes_sent != spkt.getLength()))
		throw std::runtime_error("Send message failed, aborting\n");
}

/**
 * @brief Receive a message from the server. Calls wait on the socket to block
 * for an incoming message, returning the length received. Technically reads
 * shouldn't be short since the rawnet code blocks until length is read. The
 * function expects a message of a certain message type and operation type, and
 * it returns the current offset in the buffer that the caller should begin
 * reading from. Throws an exception if the read message is invalid.
 *
 * @param len: the number of bytes to read
 * @param buf: the buffer to fill
 * @param off_out: the offset for the caller to continue reading at
 * @param mtype: message type to expect
 * @param otype: operation type to expect
 * @param allow_short: flag for whether or not to allow short reads/writes
 * @return uint64_t: number of bytes read on success; exception on failure
 */
static uint64_t recv_msgp(bfsFlexibleBuffer &rpkt, uint32_t len,
						  msg_type_t mtype, op_type_t otype, bool allow_short) {
	bfsConnectionList ready;
	int64_t bytes_read = 0;
	int32_t r_mtype = INVALID_MSG;
	int32_t r_otype = INVALID_OP;
	char err_msg[MAX_LOG_MESSAGE_SIZE] = {0};

	// if (pthread_mutex_lock(&mux_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Error when acquiring lock in recv_msg\n");
	// 	return 0;
	// }

	while (1) { // just block and wait for incoming data
		if (mux->waitConnections(ready, 0)) {
			throw std::runtime_error("Mux wait failed, aborting recv_msg\n");
		} else {
			// Check to see if the client is in the ready list
			if (ready.find(client->getSocket()) != ready.end()) {
				// Just receive the incoming data
				bytes_read = client->recvPacketizedBuffer(rpkt);
				if ((bytes_read == 0) || (bytes_read == -1)) {
					// if 0, server side socket closed, bail out; if -1, failed
					// during recvPacketizedDataL (buffer too short for incoming
					// data), ignore message and bail out
					if (pthread_mutex_unlock(&mux_lock) != 0) {
						logMessage(LOG_ERROR_LEVEL,
								   "Error when releasing lock in recv_msg\n");
						abort(); // very bad
					}
					snprintf(err_msg, MAX_LOG_MESSAGE_SIZE,
							 "Failed during recvPacketizedDataL on [%d] in "
							 "main loop: "
							 "bytes_read is %ld\n",
							 client->getSocket(), bytes_read);
					throw std::runtime_error(err_msg);
				} else {
					// Data received, check to see if the data is sane (might be
					// short, but let the caller handle that)
					logMessage(CLIENT_VRB_LOG_LEVEL,
							   "Received [%d] bytes on connection [%d]",
							   bytes_read, client->getSocket());
					break;
				}
			}
		}
	}

	// if (pthread_mutex_unlock(&mux_lock) != 0) {
	// 	logMessage(LOG_ERROR_LEVEL, "Error when releasing lock in recv_msg\n");
	// 	abort(); // very bad
	// }

	// decrypt the data (headers are encrypted now for protection so decrypt
	// before beginning processing)
	// pthread_mutex_lock(&mux_lock);
	bfsFlexibleBuffer *aad =
		new bfsFlexibleBuffer((char *)&recv_seq, sizeof(uint32_t));
	secContext->decryptData(rpkt, aad, true);
	recv_seq++;
	delete aad;
	bytes_read = rpkt.getLength(); // set to the actual number of bytes read
								   // (minus iv+mac+pad)

	// only copy out a header if caller gives valid message/op types
	if ((mtype != INVALID_MSG) && (otype != INVALID_OP)) {
		rpkt >> r_mtype >> r_otype;

		// only compare if the caller gives a valid type
		if ((r_mtype != mtype) || (r_otype != otype)) {
			snprintf(err_msg, MAX_LOG_MESSAGE_SIZE,
					 "Client recv message/op invalid type\n");
			throw std::runtime_error(err_msg);
		}
	}

	// must be able to at least read the message header
	if ((allow_short &&
		 (bytes_read < (int64_t)(sizeof(mtype) + sizeof(otype)))) ||
		(!allow_short && (bytes_read != len))) {
		snprintf(err_msg, MAX_LOG_MESSAGE_SIZE,
				 "Client recv message is too short\n");
		throw std::runtime_error(err_msg);
	}

	return bytes_read;
}

/**
 * @brief Initializes the utils, config, and crypto layers. Then finishes
 * initializing the client so the FUSE entry point can be called.
 *
 * @return int: 0 if success, -1 if failure
 */
int client_init() {
	bfsCfgItem *config, *sacfg;
	bool clientlog, vrblog, log_to_file;
	std::string logfile;

	try {
		// initializes utils, config, comms (not really an init), and crypto
		bfsUtilLayer::bfsUtilLayerInit();

		config = bfsConfigLayer::getConfigItem(BFS_CLIENT_LAYER_CONFIG);

		if (config->bfsCfgItemType() != bfsCfgItem_STRUCT) {
			logMessage(LOG_ERROR_LEVEL,
					   "Unable to find configuration in system config: %s",
					   BFS_CLIENT_LAYER_CONFIG);
			return BFS_FAILURE;
		}

		clientlog =
			(config->getSubItemByName("log_enabled")->bfsCfgItemValue() ==
			 "true");
		bfs_client_log_level = registerLogLevel("CLIENT_LOG_LEVEL", clientlog);
		vrblog = (config->getSubItemByName("log_verbose")->bfsCfgItemValue() ==
				  "true");
		bfs_client_vrb_log_level =
			registerLogLevel("CLIENT_VRB_LOG_LEVEL", vrblog);
		log_to_file =
			(config->getSubItemByName("log_to_file")->bfsCfgItemValue() ==
			 "true");

		if (log_to_file) {
			logfile = config->getSubItemByName("logfile")->bfsCfgItemValue();
			initializeLogWithFilename(logfile.c_str());
		} else {
			initializeLogWithFilehandle(STDOUT_FILENO);
		}

		// TODO: server only allows mkfs once; later should add per-client
		// config to specify per-client
		do_mkfs =
			(config->getSubItemByName("do_mkfs")->bfsCfgItemValue() == "true");

		direct_io_flag =
			(config->getSubItemByName("direct_io")->bfsCfgItemValue() ==
			 "true");

		bfs_server_ip =
			config->getSubItemByName("bfs_server_ip")->bfsCfgItemValue();
		bfs_server_port =
			(unsigned short)config->getSubItemByName("bfs_server_port")
				->bfsCfgItemValueLong();

		// Now get the security context (keys etc.)
		sacfg = config->getSubItemByName("cl_serv_sa");
		secContext = new bfsSecAssociation(sacfg);

		// get common configs
		config = bfsConfigLayer::getConfigItem(BFS_COMMON_CONFIG);

		if (config->bfsCfgItemType() != bfsCfgItem_STRUCT) {
			logMessage(LOG_ERROR_LEVEL,
					   "Unable to find configuration in system config: %s",
					   BFS_COMMON_CONFIG);
			return BFS_FAILURE;
		}

		logMessage(CLIENT_LOG_LEVEL, "Client configured.");
	} catch (bfsCfgError *e) {
		logMessage(LOG_ERROR_LEVEL, "Failure reading system config: %s",
				   e->getMessage().c_str());
		return BFS_FAILURE;
	}

	return BFS_SUCCESS;
}

/**
 * @brief Cleans up client data structures on a fatal failure.
 *
 * @return none
 */
static void cleanup() {
	logMessage(CLIENT_LOG_LEVEL, "Cleaning up client\n");
	mux->removeConnection(client);
	client->disconnect();
	delete secContext;
	delete client;
	delete mux;
}

/**
 * @brief Log all of the collect client results.
 *
 */
void write_client_latencies() {
	if (!bfsUtilLayer::perf_test())
		return;

	// Log total client read latencies
	std::string __c_read__lats = vec_to_str<double>(c_read__lats);
	std::string __c_read__lats_fname(getenv("BFS_HOME"));
	__c_read__lats_fname += "/benchmarks/micro/output/__c_read__lats.csv";
	std::ofstream __c_read__lats_f;
	__c_read__lats_f.open(__c_read__lats_fname.c_str(), std::ios::trunc);
	__c_read__lats_f << __c_read__lats.c_str();
	__c_read__lats_f.close();
	logMessage(CLIENT_LOG_LEVEL,
			   "Read latencies (overall, us, %lu records):\n[%s]\n",
			   c_read__lats.size(), __c_read__lats.c_str());

	// Log non-network-related client read latencies
	std::string __c_read__c_lats = vec_to_str<double>(c_read__c_lats);
	std::string __c_read__c_lats_fname(getenv("BFS_HOME"));
	__c_read__c_lats_fname += "/benchmarks/micro/output/__c_read__c_lats.csv";
	std::ofstream __c_read__c_lats_f;
	__c_read__c_lats_f.open(__c_read__c_lats_fname.c_str(), std::ios::trunc);
	__c_read__c_lats_f << __c_read__c_lats.c_str();
	__c_read__c_lats_f.close();
	logMessage(CLIENT_LOG_LEVEL,
			   "Read latencies (non-network, us, %lu records):\n[%s]\n",
			   c_read__c_lats.size(), __c_read__c_lats.c_str());

	// Log network-related sends for client reads
	std::string __c_read__net_send_lats =
		vec_to_str<double>(c_read__net_send_lats);
	std::string __c_read__net_send_lats_fname(getenv("BFS_HOME"));
	__c_read__net_send_lats_fname +=
		"/benchmarks/micro/output/__c_read__net_send_lats.csv";
	std::ofstream __c_read__net_send_lats_f;
	__c_read__net_send_lats_f.open(__c_read__net_send_lats_fname.c_str(),
								   std::ios::trunc);
	__c_read__net_send_lats_f << __c_read__net_send_lats.c_str();
	__c_read__net_send_lats_f.close();
	logMessage(CLIENT_LOG_LEVEL,
			   "Read latencies (network sends, us, %lu records):\n[%s]\n",
			   c_read__net_send_lats.size(), __c_read__net_send_lats.c_str());

	// Log network-related receives for client reads
	std::string __c_read__net_recv_lats =
		vec_to_str<double>(c_read__net_recv_lats);
	std::string __c_read__net_recv_lats_fname(getenv("BFS_HOME"));
	__c_read__net_recv_lats_fname +=
		"/benchmarks/micro/output/__c_read__net_recv_lats.csv";
	std::ofstream __c_read__net_recv_lats_f;
	__c_read__net_recv_lats_f.open(__c_read__net_recv_lats_fname.c_str(),
								   std::ios::trunc);
	__c_read__net_recv_lats_f << __c_read__net_recv_lats.c_str();
	__c_read__net_recv_lats_f.close();
	logMessage(CLIENT_LOG_LEVEL,
			   "Read latencies (network recvs, us, %lu records):\n[%s]\n",
			   c_read__net_recv_lats.size(), __c_read__net_recv_lats.c_str());

	// Log total client write latencies
	std::string __c_write__lats = vec_to_str<double>(c_write__lats);
	std::string __c_write__lats_fname(getenv("BFS_HOME"));
	__c_write__lats_fname += "/benchmarks/micro/output/__c_write__lats.csv";
	std::ofstream __c_write__lats_f;
	__c_write__lats_f.open(__c_write__lats_fname.c_str(), std::ios::trunc);
	__c_write__lats_f << __c_write__lats.c_str();
	__c_write__lats_f.close();
	logMessage(CLIENT_LOG_LEVEL,
			   "Write latencies (overall, us, %lu records):\n[%s]\n",
			   c_write__lats.size(), __c_write__lats.c_str());

	// Log non-network-related client write latencies
	std::string __c_write__c_lats = vec_to_str<double>(c_write__c_lats);
	std::string __c_write__c_lats_fname(getenv("BFS_HOME"));
	__c_write__c_lats_fname += "/benchmarks/micro/output/__c_write__c_lats.csv";
	std::ofstream __c_write__c_lats_f;
	__c_write__c_lats_f.open(__c_write__c_lats_fname.c_str(), std::ios::trunc);
	__c_write__c_lats_f << __c_write__c_lats.c_str();
	__c_write__c_lats_f.close();
	logMessage(CLIENT_LOG_LEVEL,
			   "Write latencies (non-network, us, %lu records):\n[%s]\n",
			   c_write__c_lats.size(), __c_write__c_lats.c_str());

	// Log network-related sends for client writes
	std::string __c_write__net_send_lats =
		vec_to_str<double>(c_write__net_send_lats);
	std::string __c_write__net_send_lats_fname(getenv("BFS_HOME"));
	__c_write__net_send_lats_fname +=
		"/benchmarks/micro/output/__c_write__net_send_lats.csv";
	std::ofstream __c_write__net_send_lats_f;
	__c_write__net_send_lats_f.open(__c_write__net_send_lats_fname.c_str(),
									std::ios::trunc);
	__c_write__net_send_lats_f << __c_write__net_send_lats.c_str();
	__c_write__net_send_lats_f.close();
	logMessage(CLIENT_LOG_LEVEL,
			   "Write latencies (network sends, us, %lu records):\n[%s]\n",
			   c_write__net_send_lats.size(), __c_write__net_send_lats.c_str());

	// Log network-related receives for client writes
	std::string __c_write__net_recv_lats =
		vec_to_str<double>(c_write__net_recv_lats);
	std::string __c_write__net_recv_lats_fname(getenv("BFS_HOME"));
	__c_write__net_recv_lats_fname +=
		"/benchmarks/micro/output/__c_write__net_recv_lats.csv";
	std::ofstream __c_write__net_recv_lats_f;
	__c_write__net_recv_lats_f.open(__c_write__net_recv_lats_fname.c_str(),
									std::ios::trunc);
	__c_write__net_recv_lats_f << __c_write__net_recv_lats.c_str();
	__c_write__net_recv_lats_f.close();
	logMessage(CLIENT_LOG_LEVEL,
			   "Write latencies (network recvs, us, %lu records):\n[%s]\n",
			   c_write__net_recv_lats.size(), __c_write__net_recv_lats.c_str());
}