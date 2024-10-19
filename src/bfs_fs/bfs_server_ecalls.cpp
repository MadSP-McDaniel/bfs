/**
 * @file enclave.cpp
 * @brief ECall definitions and helpers for the bfs server.
 */

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>

#include "bfs_acl.h"
#include "bfs_core.h"
#include "bfs_fs_layer.h"
#include <bfsBlockLayer.h>
#include <bfsCryptoError.h>
#include <bfs_common.h>
#include <bfs_log.h>
#include <bfs_util.h>

#ifdef __BFS_ENCLAVE_MODE
#include "bfs_enclave_t.h" /* For ocalls */
#include "sgx_trts.h"	   /* For generating random numbers */
#elif defined(__BFS_DEBUG_NO_ENCLAVE)
#include "bfs_server.h" /* For non-enclave testing */
#include "bfs_server_ecalls.h"
#include "bfs_server_ocalls.h"
#include <bfs_util_ocalls.h>
#endif

// #include "bfsLocalDevice.h"
#include "bfs_core_ext4_helpers.h"

static BfsHandle *bfs_handle = NULL; /* Handle to a file system instance */
static bool fs_initialized = false;
static bool enclave_initialized = false;

/* For performance testing (perhaps to subtract off the overhead from
 * mode-switching to collect timers (only about 1.5us in and out anyway so
 * negligible)) */
// static bool collect_enclave_lats = true;
// static std::vector<long> e_read__lats, e_read__e_lats,
// e_read__net_c_send_lats, 	e_write__lats, e_write__e_lats,
// e_write__net_c_send_lats, 	e_blk_reads_per_fs_read_counts,
// e_blk_writes_per_fs_read_counts, 	e_blk_reads_per_fs_write_counts,
// e_blk_writes_per_fs_write_counts, 	e_blk_reads_per_fs_other_counts,
// e_blk_writes_per_fs_other_counts, 	fs_op_sequence; static void
// write_enclave_latencies(); static void write_lats_to_file(std::string,
// std::string);
static void reset_perf_counters();
// void // collect_measurements(int);

static int32_t handle_getattr(void *, BfsUserContext *,
							  bfsSecureFlexibleBuffer &);
static int32_t handle_mkdir(void *, BfsUserContext *,
							bfsSecureFlexibleBuffer &);
static int32_t handle_unlink(void *, BfsUserContext *,
							 bfsSecureFlexibleBuffer &);
static int32_t handle_rmdir(void *, BfsUserContext *,
							bfsSecureFlexibleBuffer &);
static int32_t handle_rename(void *, BfsUserContext *,
							 bfsSecureFlexibleBuffer &);
static int32_t handle_open(void *, BfsUserContext *, bfsSecureFlexibleBuffer &);
static int32_t handle_read(void *, BfsUserContext *, bfsSecureFlexibleBuffer &);
static int32_t handle_write(void *, BfsUserContext *,
							bfsSecureFlexibleBuffer &);
static int32_t handle_release(void *, BfsUserContext *,
							  bfsSecureFlexibleBuffer &);
static int32_t handle_fsync(void *, BfsUserContext *,
							bfsSecureFlexibleBuffer &);
static int32_t handle_opendir(void *, BfsUserContext *,
							  bfsSecureFlexibleBuffer &);
static int32_t handle_readdir(void *, BfsUserContext *,
							  bfsSecureFlexibleBuffer &);
static int32_t handle_init(void *, BfsUserContext *, bool);
static int32_t handle_destroy(void *, BfsUserContext *);
static int32_t handle_create(void *, BfsUserContext *,
							 bfsSecureFlexibleBuffer &);
static int32_t handle_chmod(void *, BfsUserContext *,
							bfsSecureFlexibleBuffer &);
static int32_t handle_ftruncate(void *, BfsUserContext *,
								bfsSecureFlexibleBuffer &);

/* lwext4 methods */
// static int do_lwext4_init(void);
// static int do_lwext4_mkfs(void);
// static int do_lwext4_mount(void);
// static int do_lwext4_getattr(BfsUserContext *usr, std::string path,
// 							 bfs_uid_t *uid, bfs_ino_id_t *fino,
// 							 uint32_t *fmode, uint64_t *fsize);

/**
 * @brief Wrapper to invoke initialization of the bfs dependencies that run in
 * the context of the enclave. This should be called in the main dispatcher
 * thread before beginning to listen for clients (so that we have active
 * connection to the block device cluster, etc.).
 *
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int enclave_init() {
	// initialize the entire stack (sets use_lwext4_impl)
	if (BfsFsLayer::bfsFsLayerInit() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed bfsFsLayerInit\n");
		return BFS_FAILURE;
	}

	if (BfsFsLayer::use_lwext4()) {
		logMessage(FS_LOG_LEVEL, "Initializing [lwext4] enclave ...");

		if (__do_lwext4_init(NULL) != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL,
					   "Failed to initalize [lwext4] enclave, aborting.");
			return BFS_FAILURE;
		}
		// return BFS_FAILURE; // just kill here for testing
	} else {
		logMessage(FS_LOG_LEVEL, "Initializing [bfs] enclave ...");

		if (bfsBlockLayer::set_vbc(bfsVertBlockCluster::bfsClusterFactory()) !=
			BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL,
					   "Failed to initalize virtual block cluster, aborting.");
			return BFS_FAILURE;
		}
	}

	return BFS_SUCCESS;
}

/**
 * @brief Deserializes the rpc message and executes the operation based on the
 * type (e.g., client open, read, etc.). Expects the first two fields to be the
 * message and operation types. Message structure:
 *              | msg type | operation type | variable args per op ... |
 * Note that the non-enclave code can tamper with args here (e.g., the client
 * conn ptr), and we rely on the security associations to ensure integrity and
 * secrecy of messages (i.e., message decryption will only succeed with if the
 * socket/conn ptr is correctly mapped).
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param rbuf: incoming buffer
 * @param rpkt_enc_ptr: buffer containing the encrypted received msg
 * @return int64_t: if doing performance testing, returns the opcode (for
 * collecting fine-grained latency metrics) on success, otherwise returns
 * BFS_SUCCESS if success, and always returns BFS_FAILURE if failure
 */
int64_t ecall_bfs_handle_in_msg(void *in_conn_ptr, void *rpkt_enc_ptr) {
	if (!enclave_initialized) {
		if (enclave_init() != BFS_SUCCESS) {
			logMessage(LOG_ERROR_LEVEL, "Failed during enclave_init.");
			abort();
		}

		enclave_initialized = true;
	}

	/* Decrypt received pkt from untrusted memory into secure buffer, and
	 * encrypt the send pkt from secure memory to untrusted memory */
	// TODO: just pass raw buffer here instead of pointer to the rpkt object
	// (need to avoid vtable issues)
	bfsFlexibleBuffer *rpkt_enc = (bfsFlexibleBuffer *)rpkt_enc_ptr;
	bfsSecureFlexibleBuffer rpkt;

	int32_t mtype = INVALID_MSG;
	int32_t otype = INVALID_OP;
	int32_t ret = 0;
	BfsUserContext *usr;

	// Behaves weird if we invoke class methods on the untrusted pointer object
	// directly (but should be able to use casts from void pointers) (except for
	// some getters which are probably inlined so there's no issue)
	//   if (!(usr = BfsACLayer::get_user_context(
	//             ((bfsNetworkConnection *)in_conn_ptr)->getSocket()))) {
	logMessage(FS_VRB_LOG_LEVEL, "Getting user context.\n");
	if (!BfsACLayer::initialized()) {
		logMessage(LOG_ERROR_LEVEL, "BfsACLayer not initialized.\n");
		return BFS_FAILURE;
	}
	if (!(usr = BfsACLayer::get_user_context(in_conn_ptr))) {
		if (!(usr = BfsACLayer::add_user_context(in_conn_ptr))) {
			logMessage(LOG_ERROR_LEVEL, "Failed adding new user context.\n");
			return BFS_FAILURE;
		}

		if (rpkt_enc_ptr) {
			logMessage(LOG_ERROR_LEVEL,
					   "Received non-null msg ptr for new client.\n");
			return BFS_FAILURE;
		}

		return BFS_SUCCESS;
	}

	logMessage(FS_VRB_LOG_LEVEL, "Got user context.\n");

	// decrypt then get message and operation types
	try {
		uint32_t _client_recv_seq = usr->get_recv_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_recv_seq,
			sizeof(uint32_t)); // must be secure buffer
		usr->get_SA()->decryptData(*rpkt_enc, rpkt, aad, true);
		usr->inc_recv_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from crypto: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

	rpkt >> mtype >> otype;

	if (mtype != TO_SERVER) {
		logMessage(LOG_ERROR_LEVEL, "Server recv message invalid type\n");
		return BFS_FAILURE;
	}

	// push the opcode so we can parse out all the results correctly
	// if (bfsUtilLayer::perf_test())
	// 	fs_op_sequence.push_back(otype);

	switch (otype) {
	case CLIENT_GETATTR_OP:
		ret = handle_getattr(in_conn_ptr, usr, rpkt);
		break;
	case CLIENT_MKDIR_OP:
		ret = handle_mkdir(in_conn_ptr, usr, rpkt);
		break;
	case CLIENT_UNLINK_OP:
		ret = handle_unlink(in_conn_ptr, usr, rpkt);
		break;
	case CLIENT_RMDIR_OP:
		ret = handle_rmdir(in_conn_ptr, usr, rpkt);
		break;
	case CLIENT_RENAME_OP:
		ret = handle_rename(in_conn_ptr, usr, rpkt);
		break;
	case CLIENT_OPEN_OP:
		ret = handle_open(in_conn_ptr, usr, rpkt);
		break;
	case CLIENT_READ_OP:
		ret = handle_read(in_conn_ptr, usr, rpkt);
		break;
	case CLIENT_WRITE_OP:
		ret = handle_write(in_conn_ptr, usr, rpkt);
		break;
	case CLIENT_RELEASE_OP:
		ret = handle_release(in_conn_ptr, usr, rpkt);
		break;
	case CLIENT_FSYNC_OP:
		ret = handle_fsync(in_conn_ptr, usr, rpkt);
		break;
	case CLIENT_OPENDIR_OP:
		ret = handle_opendir(in_conn_ptr, usr, rpkt);
		break;
	case CLIENT_READDIR_OP:
		ret = handle_readdir(in_conn_ptr, usr, rpkt);
		break;
	case CLIENT_INIT_OP:
		ret = handle_init(in_conn_ptr, usr, false);
		break;
	case CLIENT_INIT_MKFS_OP:
		ret = handle_init(in_conn_ptr, usr, true);
		break;
	case CLIENT_DESTROY_OP:
		ret = handle_destroy(in_conn_ptr, usr);
		break;
	case CLIENT_CREATE_OP:
		ret = handle_create(in_conn_ptr, usr, rpkt);
		break;
	case CLIENT_CHMOD_OP:
		ret = handle_chmod(in_conn_ptr, usr, rpkt);
		break;
	case CLIENT_TRUNCATE_OP:
		ret = handle_ftruncate(in_conn_ptr, usr, rpkt);
		break;
	default: /* INVALID_OP */
		ret = BFS_FAILURE;
		break;
	}

	// return the opcode only for performance testing so we can collect data
	if (bfsUtilLayer::perf_test() && (ret != BFS_FAILURE))
		return otype;

	return ret;
}

/**
 * @brief Resets the global block read and write counters.
 *
 */
static void reset_perf_counters() {
	if (!bfsUtilLayer::perf_test())
		return;

#ifdef __BFS_ENCLAVE_MODE
		// num_reads = 0;
		// num_writes = 0;
#endif
}

// /**
//  * @brief Collect performance results and reset counters. Note that we append
//  * the results to different arrays (for reads, writes, and other ops). The to
//  * *_other arrays are used to indicate that the operation was not a read or
//  * write and how many device block reads/writes we should skip while parsing.
//  *
//  * @param optype: the operation type executed (and the one to associate the
//  * measurements with)
//  */
// void // collect_measurements(int optype) {
// 	if (!bfsUtilLayer::perf_test())
// 		return;

// #ifdef __BFS_ENCLAVE_MODE
// 	switch (optype) {
// 	case CLIENT_READ_OP:
// 		e_blk_reads_per_fs_read_counts.push_back(num_reads);
// 		e_blk_writes_per_fs_read_counts.push_back(num_writes);
// 		break;
// 	case CLIENT_WRITE_OP:
// 		e_blk_reads_per_fs_write_counts.push_back(num_reads);
// 		e_blk_writes_per_fs_write_counts.push_back(num_writes);
// 		break;
// 	default:
// 		e_blk_reads_per_fs_other_counts.push_back(num_reads);
// 		e_blk_writes_per_fs_other_counts.push_back(num_writes);
// 		break;
// 	}

// 	reset_perf_counters();
// #endif
// }

/**
 * @brief Handles a getattr request for a file. Invokes the file operation
 * before constructing an encrypted response message containing the file
 * attributes.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param rpkt: the decrypted request packet
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_getattr(void *in_conn_ptr, BfsUserContext *usr,
							  bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute getattr request */
	uint32_t fname_len = 0, fmode = 0;
	bfs_ino_id_t fino = 0;
	bfs_uid_t uid = 0;
	uint64_t fsize = 0;
	uint32_t e_atime, e_mtime, e_ctime;
	int32_t ret = 0;
	std::string fname_str = "";

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	rpkt >> fname_len;
	assert(fname_len > 0);
	fname_str = std::string(rpkt.getBuffer()); // assumes null terminated

	try {
		if (BfsFsLayer::use_lwext4())
			ret =
				__do_lwext4_getattr(usr, fname_str.c_str(), &uid, &fino, &fmode,
									&fsize, &e_atime, &e_mtime, &e_ctime);
		else
			ret = bfs_handle->bfs_getattr(usr, fname_str, &uid, &fino, &fmode,
										  &fsize);

	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = -ENOENT; // OK on server side, but invalid ino indicates error
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	/* Send response to client */
	int32_t mtype = FROM_SERVER, otype = CLIENT_GETATTR_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret) + sizeof(uid) +
				   sizeof(fino) + sizeof(fmode) + sizeof(fsize) +
				   sizeof(e_atime) + sizeof(e_mtime) + sizeof(e_ctime));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << fsize << fmode << fino << e_atime << e_mtime << e_ctime << uid
		 << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE; // let client interpret the return codes (here ret
							// isnt used, but the fino indicates a failure)
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_GETATTR_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Handles a mkdir request for a directory file. Invokes the file
 * operation before constructing an encrypted response message containing the
 * return code of the operation.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param rpkt: the decrypted request packet
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_mkdir(void *in_conn_ptr, BfsUserContext *usr,
							bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute mkdir request */
	uint32_t fname_len = 0, fmode = 0;
	int32_t ret = 0;
	std::string fname_str = "";

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	rpkt >> fmode >> fname_len;
	assert(fname_len > 0);
	fname_str = std::string(rpkt.getBuffer()); // assumes null terminated

	try {
		if (BfsFsLayer::use_lwext4())
			ret = __do_lwext4_mkdir(usr, fname_str.c_str(), fmode);
		else
			ret = bfs_handle->bfs_mkdir(usr, fname_str, fmode);
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	/* Send response to client */
	int32_t mtype = FROM_SERVER, otype = CLIENT_MKDIR_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_MKDIR_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Handles an unlink request for a file. Invokes the file operation
 * before constructing an encrypted response message containing the return code
 * for the operation.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param rpkt: the decrypted request packet
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_unlink(void *in_conn_ptr, BfsUserContext *usr,
							 bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute unlink request */
	uint32_t fname_len = 0;
	std::string fname_str = "";
	int32_t ret = 0;

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	rpkt >> fname_len;
	assert(fname_len > 0);
	fname_str = std::string(rpkt.getBuffer()); // assumes null terminated

	try {
		if (BfsFsLayer::use_lwext4())
			ret = __do_lwext4_unlink(usr, fname_str.c_str());
		else
			ret = bfs_handle->bfs_unlink(usr, fname_str);
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	/* Send response to client */
	int32_t mtype = FROM_SERVER, otype = CLIENT_UNLINK_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_UNLINK_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Handles an rmdir request for a file. Invokes the file operation
 * before constructing an encrypted response message containing the return code
 * for the operation.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param rpkt: the decrypted request packet
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_rmdir(void *in_conn_ptr, BfsUserContext *usr,
							bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute rmdir request */
	uint32_t fname_len = 0;
	std::string fname_str = "";
	int32_t ret = 0;

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	rpkt >> fname_len;
	assert(fname_len > 0);
	fname_str = std::string(rpkt.getBuffer()); // assumes null terminated

	try {
		if (BfsFsLayer::use_lwext4())
			ret = __do_lwext4_rmdir(usr, fname_str.c_str());
		else
			ret = bfs_handle->bfs_rmdir(usr, fname_str);
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	/* Send response to client */
	int32_t mtype = FROM_SERVER, otype = CLIENT_RMDIR_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_RMDIR_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Handles an rename request for a file. Invokes the file operation
 * before constructing an encrypted response message containing the return code
 * for the operation.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param rpkt: the decrypted request packet
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_rename(void *in_conn_ptr, BfsUserContext *usr,
							 bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute rename request */
	uint32_t fr_fname_len = 0;
	uint32_t to_fname_len = 0;
	std::string fr_fname_str = "", to_fname_str = "";
	int32_t ret = 0;

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	rpkt >> fr_fname_len >> to_fname_len;
	assert((fr_fname_len > 0) && (to_fname_len > 0));
	fr_fname_str = std::string(rpkt.getBuffer()); // assumes null terminated
	to_fname_str = std::string(&(rpkt.getBuffer()[fr_fname_len]));

	try {
		if (BfsFsLayer::use_lwext4())
			ret = __do_lwext4_rename(usr, fr_fname_str.c_str(),
									 to_fname_str.c_str());
		else
			ret = bfs_handle->bfs_rename(usr, fr_fname_str, to_fname_str);
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	/* Send response to client */
	int32_t mtype = FROM_SERVER, otype = CLIENT_RENAME_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_RENAME_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Handles an open request for a file. Invokes the file operation
 * before constructing an encrypted response message containing the return code
 * for the operation.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param rpkt: the decrypted request packet
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_open(void *in_conn_ptr, BfsUserContext *usr,
						   bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute open request */
	uint32_t fname_len = 0;
	std::string fname_str = "";
	bfs_fh_t ret = 0;

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	rpkt >> fname_len;
	assert(fname_len > 0);
	fname_str = std::string(rpkt.getBuffer()); // assumes null terminated

	try {
		if (BfsFsLayer::use_lwext4())
			ret = __do_lwext4_open(usr, fname_str.c_str(), 0777);
		else
			ret = bfs_handle->bfs_open(usr, fname_str, 0777);
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	// otherwise send file descriptor in response to client
	int32_t mtype = FROM_SERVER, otype = CLIENT_OPEN_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_OPEN_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Handles a read request for a file. Invokes the file operation
 * before constructing an encrypted response message containing the return code
 * for the operation.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param rpkt: the decrypted request packet
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_read(void *in_conn_ptr, BfsUserContext *usr,
						   bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute read request */
	bfs_fh_t fh = 0;
	uint64_t size = 0, ret = 0;
	uint64_t rd_off = 0;
	// double e_start_time = 0.0, e_end_time = 0.0, e_net_c_send_start_time =
	// 0.0, 	   e_net_c_send_end_time = 0.0;

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level read
	// 	reset_perf_counters();
	// 	if (bfsUtilLayer::perf_test() && collect_enclave_lats) {
	// #ifdef __BFS_ENCLAVE_MODE
	// 		if ((ocall_get_time2(&e_start_time) != SGX_SUCCESS) ||
	// 			(e_start_time == -1))
	// 			return BFS_FAILURE;
	// #else
	// 		if ((e_start_time = ocall_get_time2()) == -1)
	// 			return BFS_FAILURE;
	// #endif
	// 	}

	rpkt >> fh >> size >> rd_off;

	/**
	 * Send response to client (execute the read and copy directly into buffer
	 * at the correct offset to avoid having to double copy).
	 */
	int32_t mtype = FROM_SERVER, otype = CLIENT_READ_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(size));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc((uint32_t)size, 0, total_out_msg_len,
						0); // just allocate big enough for now

	try {
		if (BfsFsLayer::use_lwext4())
			ret = __do_lwext4_read(usr, fh, spkt.getBuffer(), size, rd_off);
		else
			ret = bfs_handle->bfs_read(usr, fh, spkt.getBuffer(), size, rd_off);
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	// if ((ocall_get_time2(&e_net_c_send_start_time) != SGX_SUCCESS) ||
	// 	(e_net_c_send_start_time == -1))
	// 	return BFS_FAILURE;

	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;

		// if ((ocall_get_time2(&e_net_c_send_end_time) != SGX_SUCCESS) ||
		// 	(e_net_c_send_end_time == -1))
		// 	return BFS_FAILURE;
#else
	// if (bfsUtilLayer::perf_test() && collect_enclave_lats &&
	// 	((e_net_c_send_start_time = ocall_get_time2()) == -1))
	// 	return BFS_FAILURE;

	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;

		// if (bfsUtilLayer::perf_test() && collect_enclave_lats &&
		// 	((e_net_c_send_end_time = ocall_get_time2()) == -1))
		// 	return BFS_FAILURE;
#endif

	// 	if (bfsUtilLayer::perf_test() && collect_enclave_lats) {
	// 		e_read__net_c_send_lats.push_back(e_net_c_send_end_time -
	// 										  e_net_c_send_start_time);

	// #ifdef __BFS_ENCLAVE_MODE
	// 		if ((ocall_get_time2(&e_end_time) != SGX_SUCCESS) || (e_end_time ==
	// -1)) 			return BFS_FAILURE; #else 		if ((e_end_time =
	// ocall_get_time2())
	// == -1) 			return BFS_FAILURE; #endif

	// 		e_read__e_lats.push_back(
	// 			(e_end_time - e_start_time) -
	// 			(e_net_c_send_end_time - e_net_c_send_start_time));
	// 		e_read__lats.push_back(e_end_time - e_start_time);
	// 	}
	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_READ_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Handles a write request for a file. Invokes the file operation
 * before constructing an encrypted response message containing the return code
 * for the operation.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param rpkt: the decrypted request packet
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_write(void *in_conn_ptr, BfsUserContext *usr,
							bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute write request */
	bfs_fh_t fh = 0;
	uint64_t size = 0, ret = 0;
	uint64_t wr_off = 0;
	// double e_start_time = 0.0, e_end_time = 0.0, e_net_c_send_start_time =
	// 0.0, 	   e_net_c_send_end_time = 0.0;

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level write
	// 	reset_perf_counters();
	// 	if (bfsUtilLayer::perf_test() && collect_enclave_lats) {
	// #ifdef __BFS_ENCLAVE_MODE
	// 		if ((ocall_get_time2(&e_start_time) != SGX_SUCCESS) ||
	// 			(e_start_time == -1))
	// 			return BFS_FAILURE;
	// #else
	// 		if ((e_start_time = ocall_get_time2()) == -1)
	// 			return BFS_FAILURE;
	// #endif
	// 	}

	rpkt >> fh >> size >> wr_off;

	try {
		if (BfsFsLayer::use_lwext4())
			ret = __do_lwext4_write(usr, fh, rpkt.getBuffer(), size, wr_off);
		else
			ret =
				bfs_handle->bfs_write(usr, fh, rpkt.getBuffer(), size, wr_off);
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	/* Send response to client */
	int32_t mtype = FROM_SERVER, otype = CLIENT_WRITE_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	// if ((ocall_get_time2(&e_net_c_send_start_time) != SGX_SUCCESS) ||
	// 	(e_net_c_send_start_time == -1))
	// 	return BFS_FAILURE;

	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;

		// if ((ocall_get_time2(&e_net_c_send_end_time) != SGX_SUCCESS) ||
		// 	(e_net_c_send_end_time == -1))
		// 	return BFS_FAILURE;
#else
	// if (bfsUtilLayer::perf_test() && collect_enclave_lats &&
	// 	((e_net_c_send_start_time = ocall_get_time2()) == -1))
	// 	return BFS_FAILURE;

	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;

		// if (bfsUtilLayer::perf_test() && collect_enclave_lats &&
		// 	((e_net_c_send_end_time = ocall_get_time2()) == -1))
		// 	return BFS_FAILURE;
#endif

	// 	if (bfsUtilLayer::perf_test() && collect_enclave_lats) {
	// 		e_write__net_c_send_lats.push_back(e_net_c_send_end_time -
	// 										   e_net_c_send_start_time);

	// #ifdef __BFS_ENCLAVE_MODE
	// 		if ((ocall_get_time2(&e_end_time) != SGX_SUCCESS) || (e_end_time ==
	// -1)) 			return BFS_FAILURE; #else 		if ((e_end_time =
	// ocall_get_time2())
	// == -1) 			return BFS_FAILURE; #endif

	// 		e_write__e_lats.push_back(
	// 			(e_end_time - e_start_time) -
	// 			(e_net_c_send_end_time - e_net_c_send_start_time));
	// 		e_write__lats.push_back(e_end_time - e_start_time);
	// 	}
	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_WRITE_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Handles a release request for a file. Invokes the file operation
 * before constructing an encrypted response message containing the return code
 * for the operation.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param rpkt: the decrypted request packet
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_release(void *in_conn_ptr, BfsUserContext *usr,
							  bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute release request */
	bfs_fh_t fh = 0;
	int32_t ret = 0;

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	rpkt >> fh;
	assert(fh >= ROOT_INO);

	try {
		if (BfsFsLayer::use_lwext4())
			ret = __do_lwext4_release(usr, fh);
		else
			ret = bfs_handle->bfs_release(usr, fh);
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	/* Send response to client */
	int32_t mtype = FROM_SERVER, otype = CLIENT_RELEASE_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_RELEASE_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Handles an fsync request for a file. Invokes the file operation
 * before constructing an encrypted response message containing the return code
 * for the operation.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param rpkt: the decrypted request packet
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_fsync(void *in_conn_ptr, BfsUserContext *usr,
							bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute fsync request */
	bfs_fh_t fh = 0;
	uint32_t datasync = 0;
	int32_t ret = 0;

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	rpkt >> fh >> datasync;
	assert(fh >= ROOT_INO);

	try {
		if (BfsFsLayer::use_lwext4())
			ret = __do_lwext4_fsync(usr, fh, datasync);
		else
			ret = bfs_handle->bfs_fsync(usr, fh, datasync);
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	/* Send response to client */
	int32_t mtype = FROM_SERVER, otype = CLIENT_FSYNC_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_FSYNC_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Handles an opendir request for a file. Invokes the file operation
 * before constructing an encrypted response message containing the return code
 * for the operation.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param rpkt: the decrypted request packet
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_opendir(void *in_conn_ptr, BfsUserContext *usr,
							  bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute opendir request */
	uint32_t fname_len = 0;
	bfs_fh_t ret = 0;
	std::string fname_str = "";

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	rpkt >> fname_len;
	assert(fname_len > 0);
	fname_str = std::string(rpkt.getBuffer()); // assumes null terminated

	try {
		if (BfsFsLayer::use_lwext4())
			ret = __do_lwext4_opendir(usr, fname_str.c_str());
		else
			ret = bfs_handle->bfs_opendir(usr, fname_str);
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	// otherwise send file descriptor in response to client
	int32_t mtype = FROM_SERVER, otype = CLIENT_OPENDIR_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_OPENDIR_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Handles a readdir request for a directory file. Tries to read all of
 * the dentries of an open directory into a vector, then serializes each dentry
 * into a format suitable for the client to read. Then constructs an encrypted
 * response message containing the dentries.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param rpkt: the decrypted request packet
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_readdir(void *in_conn_ptr, BfsUserContext *usr,
							  bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute readdir request */
	bfs_fh_t fh = 0;
	uint64_t off = 0;
	std::vector<DirEntry *> ents;

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	rpkt >> fh;
	assert(fh >= ROOT_INO);

	try {
		if (BfsFsLayer::use_lwext4())
			__do_lwext4_readdir(usr, fh, &ents);
		else
			bfs_handle->bfs_readdir(usr, fh, &ents);
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	// otherwise send directory entries in response to client
	int32_t mtype = FROM_SERVER, otype = CLIENT_READDIR_OP;
	uint64_t num_ents = ents.size(), de_data_off = 0;
	int32_t total_data_msg_len = 0;
	int32_t total_hdr_msg_len = 0;

	// add dentries
	uint64_t e_ino = 0;
	uint32_t e_mode = 0;
	uint64_t e_sz = 0;
	uint32_t e_atime, e_mtime, e_ctime;
	// char *e_name = NULL;
	std::string e_name = "";
	uint32_t e_name_len = 0;
	Inode *ino_ptr = NULL;
	uint32_t de_len =
		(uint32_t)(sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint64_t) +
				   sizeof(e_name_len) + sizeof(uint32_t) * 3);
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	total_hdr_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(num_ents));
	total_data_msg_len = (uint32_t)(ents.size() * de_len);

	spkt.resetWithAlloc(0, 0, total_hdr_msg_len, 0);
	spkt << num_ents << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	spkt.resetWithAlloc((uint32_t)ents.size() * MAX_FILE_NAME_LEN, 0,
						total_data_msg_len, 0);
	for (DirEntry *e : ents) {
		e_name = std::string(bfs_basename(e->get_de_name().c_str()));
		e_name_len = (uint32_t)e_name.size() + 1;
		memcpy(&(spkt.getBuffer()[de_data_off + off]), e_name.c_str(),
			   e_name_len);
		off += e_name_len;
		memset(&(spkt.getBuffer()[de_data_off + off]), '\0',
			   MAX_FILE_NAME_LEN - e_name_len); // fill to MAX_FILE_NAME_LEN
		off += MAX_FILE_NAME_LEN - e_name_len;

		e_ino = e->get_ino();

		if (BfsFsLayer::use_lwext4()) {
			// Pull directly from dentry object (should be filled by
			// __do_lwext4_readdir).
			e_mode = e->get_e_mode();
			e_sz = e->get_e_size();

			e_atime = e->get_atime();
			e_mtime = e->get_mtime();
			e_ctime = e->get_ctime();
		} else {
			ino_ptr = bfs_handle->read_inode(e_ino);
			e_mode = ino_ptr->get_mode();
			e_sz = ino_ptr->get_size();

			if (!ino_ptr->unlock()) {
				logMessage(LOG_ERROR_LEVEL, "Error when releasing inode lock "
											"in server_ecalls handler\n");
				abort(); // very bad
			}
		}

		spkt << e_name_len << e_sz << e_mode << e_ino << e_atime << e_mtime
			 << e_ctime;

		// update the offset of the data buffer after we just added the headers
		de_data_off += de_len;
	}

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_READDIR_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Handles an init request for a file. Initializes the global bfs handle
 * object, checks if a format/mkfs needs to be done, then mounts the file system
 * to the server and makes it accessible to clients. Then constructs an
 * encrypted response message containing the return code for the operation.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param do_mkfs: flag indicating whether or not to execute a mkfs on init
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_init(void *in_conn_ptr, BfsUserContext *usr,
						   bool do_mkfs) {
	/* Receive and execute init request */
	int32_t ret = 0;

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	try {
		if (!fs_initialized) {
			if (!BfsFsLayer::use_lwext4())
				bfs_handle = new BfsHandle();

			// if (do_mkfs && !fs_initialized) {
			if (do_mkfs) {
				if (BfsFsLayer::use_lwext4())
					ret = __do_lwext4_mkfs();
				else
					ret = bfs_handle->mkfs();
			}

			if (BfsFsLayer::use_lwext4())
				ret += __do_lwext4_mount();
			else
				ret += bfs_handle->mount();

			fs_initialized = true;
		} else {
			ret = BFS_SUCCESS; // already mounted (for all clients)
		}
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	/* Send response to client */
	int32_t mtype = FROM_SERVER, otype = CLIENT_INIT_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_INIT_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Handles an destroy request for the server. Deletes the bfs handle to
 * invoke the destroy call chain before constructing an encrypted response
 * message containing the return code for the operation.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_destroy(void *in_conn_ptr, BfsUserContext *usr) {
	/* Receive and execute destroy request */
	int32_t ret = 0;

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	try {
		delete bfs_handle;
		bfs_handle = NULL;
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	/* Send response to client */
	int32_t mtype = FROM_SERVER, otype = CLIENT_DESTROY_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_DESTROY_OP);

	// Log all results before finishing
	// write_enclave_latencies();

	return BFS_SUCCESS;
}

/**
 * @brief Handles a create request for a file. Invokes the file operation
 * before constructing an encrypted response message containing the return code
 * for the operation.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param rpkt: the decrypted request packet
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_create(void *in_conn_ptr, BfsUserContext *usr,
							 bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute create request */
	uint32_t fname_len = 0, fmode = 0;
	bfs_fh_t ret = 0;
	std::string fname_str = "";

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	rpkt >> fmode >> fname_len;
	assert(fname_len > 0);
	fname_str = std::string(rpkt.getBuffer()); // assumes null terminated

	try {
		if (BfsFsLayer::use_lwext4())
			ret = __do_lwext4_create(usr, fname_str.c_str(), fmode);
		else
			ret = bfs_handle->bfs_create(usr, fname_str, fmode);
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	/* Send response to client */
	int32_t mtype = FROM_SERVER, otype = CLIENT_CREATE_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_CREATE_OP);

	return BFS_SUCCESS;
}

static int32_t handle_ftruncate(void *in_conn_ptr, BfsUserContext *usr,
								bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute ftruncate request */
	uint32_t fname_len = 0, new_size = 0;
	bfs_fh_t ret = 0;
    bfs_fh_t fh = 0;
	std::string fname_str = "";

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	rpkt >> fh >> new_size >> fname_len;
	assert(fname_len > 0);
	fname_str = std::string(rpkt.getBuffer()); // assumes null terminated

	try {
		if (BfsFsLayer::use_lwext4())
			ret = __do_lwext4_ftruncate(usr, fname_str.c_str(), fh, new_size);
		else
			ret = -1;
		// ret = bfs_handle->bfs_ftruncate(usr, fname_str, new_size);
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	/* Send response to client */
	int32_t mtype = FROM_SERVER, otype = CLIENT_TRUNCATE_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_TRUNCATE_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Handles a chmod request for a file.
 *
 * @param in_conn_ptr: pointer to client connection object (non-enclave memory)
 * @param usr: the user context that sent the request
 * @param rpkt: the decrypted request packet
 * @return int32_t: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
static int32_t handle_chmod(void *in_conn_ptr, BfsUserContext *usr,
							bfsSecureFlexibleBuffer &rpkt) {
	/* Receive and execute chmod request */
	uint32_t fname_len = 0, new_mode = 0;
	bfs_fh_t ret = 0;
	std::string fname_str = "";

	// Reset counter so we can count the number of _block_ reads and writes
	// associated with this fs-level operation
	reset_perf_counters();

	rpkt >> new_mode >> fname_len;
	assert(fname_len > 0);
	fname_str = std::string(rpkt.getBuffer()); // assumes null terminated

	try {
		if (BfsFsLayer::use_lwext4())
			ret = __do_lwext4_chmod(usr, fname_str.c_str(), new_mode);
		else
			ret = bfs_handle->bfs_chmod(usr, fname_str, new_mode);
	} catch (BfsAccessDeniedError &ade) {
		if (ade.err().size() > 0)
			logMessage(FS_LOG_LEVEL, ade.err().c_str());
		ret = -EPERM;
	} catch (BfsClientRequestFailedError &rfe) {
		// let client handle fs errors, just log what happened
		if (rfe.err().size() > 0)
			logMessage(FS_LOG_LEVEL, rfe.err().c_str());
		ret = BFS_FAILURE;
	} catch (bfsUtilError *err) {
		logMessage(LOG_ERROR_LEVEL, err->getMessage().c_str());
		delete err;
		return BFS_FAILURE;
	} catch (BfsServerError &se) {
		// log the server error and return fail
		if (se.err().size() > 0)
			logMessage(LOG_ERROR_LEVEL, se.err().c_str());

		return BFS_FAILURE;
	}

	/* Send response to client */
	int32_t mtype = FROM_SERVER, otype = CLIENT_CHMOD_OP;
	uint32_t total_out_msg_len =
		(uint32_t)(sizeof(mtype) + sizeof(otype) + sizeof(ret));
	bfsSecureFlexibleBuffer spkt;
	bfsSecureFlexibleBuffer spkt_enc;

	spkt.resetWithAlloc(0, 0, total_out_msg_len, 0);
	spkt << ret << otype << mtype;

	try {
		uint32_t _client_send_seq = usr->get_send_seq();
		bfsSecureFlexibleBuffer *aad = new bfsSecureFlexibleBuffer(
			(char *)&_client_send_seq, sizeof(uint32_t));
		usr->get_SA()->encryptData(spkt, spkt_enc, aad, true);
		usr->inc_send_seq();
	} catch (bfsCryptoError *err) {
		logMessage(LOG_ERROR_LEVEL, "Exception caught from encrypt: %s\n",
				   err->getMessage().c_str());
		delete err;

		return BFS_FAILURE;
	}

#ifdef __BFS_ENCLAVE_MODE
	int64_t ocall_ret = 0;
	if ((ocall_handle_out_msg(&ocall_ret, in_conn_ptr, spkt_enc.getLength(),
							  spkt_enc.getBuffer()) != SGX_SUCCESS) ||
		(ocall_ret != BFS_SUCCESS))
		return BFS_FAILURE;
#else
	if (ocall_handle_out_msg(in_conn_ptr, spkt_enc.getLength(),
							 spkt_enc.getBuffer()) != BFS_SUCCESS)
		return BFS_FAILURE;
#endif

	// Collect performance results and reset counters.
	// collect_measurements(CLIENT_CHMOD_OP);

	return BFS_SUCCESS;
}

/**
 * @brief Log information related to the overall operation call sequence and
 * fs-level reads/writes.
 */
// void write_enclave_latencies() {
// #ifdef __BFS_ENCLAVE_MODE
// 	if (!bfsUtilLayer::perf_test() || !collect_enclave_lats)
// 		return;

// 	// Log the op call sequence
// 	std::string __fs_op_sequence = vec_to_str<long>(fs_op_sequence);
// 	std::string __fs_op_sequence_fname("__fs_op_sequence");
// 	write_lats_to_file(__fs_op_sequence, __fs_op_sequence_fname);
// 	logMessage(
// 		FS_LOG_LEVEL,
// 		"Enclave op sequence by operation type code (us, %lu records):\n[%s]\n",
// 		fs_op_sequence.size(), __fs_op_sequence.c_str());

// 	// Log the number of block reads per read fs op
// 	std::string __e_blk_reads_per_fs_read_counts =
// 		vec_to_str<long>(e_blk_reads_per_fs_read_counts);
// 	std::string __e_blk_reads_per_fs_read_counts_fname(
// 		"__e_blk_reads_per_fs_read_counts");
// 	write_lats_to_file(__e_blk_reads_per_fs_read_counts,
// 					   __e_blk_reads_per_fs_read_counts_fname);
// 	logMessage(FS_LOG_LEVEL,
// 			   "block reads per fs read counts, %lu records:\n[%s]\n",
// 			   e_blk_reads_per_fs_read_counts.size(),
// 			   __e_blk_reads_per_fs_read_counts.c_str());

// 	// Log the number of block writes per read fs op
// 	std::string __e_blk_writes_per_fs_read_counts =
// 		vec_to_str<long>(e_blk_writes_per_fs_read_counts);
// 	std::string __e_blk_writes_per_fs_read_counts_fname(
// 		"__e_blk_writes_per_fs_read_counts");
// 	write_lats_to_file(__e_blk_writes_per_fs_read_counts,
// 					   __e_blk_writes_per_fs_read_counts_fname);
// 	logMessage(FS_LOG_LEVEL,
// 			   "block writes per fs read counts, %lu records:\n[%s]\n",
// 			   e_blk_writes_per_fs_read_counts.size(),
// 			   __e_blk_writes_per_fs_read_counts.c_str());

// 	// Log the number of block reads per write fs op
// 	std::string __e_blk_reads_per_fs_write_counts =
// 		vec_to_str<long>(e_blk_reads_per_fs_write_counts);
// 	std::string __e_blk_reads_per_fs_write_counts_fname(
// 		"__e_blk_reads_per_fs_write_counts");
// 	write_lats_to_file(__e_blk_reads_per_fs_write_counts,
// 					   __e_blk_reads_per_fs_write_counts_fname);
// 	logMessage(FS_LOG_LEVEL,
// 			   "block reads per fs write counts, %lu records:\n[%s]\n",
// 			   e_blk_reads_per_fs_write_counts.size(),
// 			   __e_blk_reads_per_fs_write_counts.c_str());

// 	// Log the number of block writes per write fs op
// 	std::string __e_blk_writes_per_fs_write_counts =
// 		vec_to_str<long>(e_blk_writes_per_fs_write_counts);
// 	std::string __e_blk_writes_per_fs_write_counts_fname(
// 		"__e_blk_writes_per_fs_write_counts");
// 	write_lats_to_file(__e_blk_writes_per_fs_write_counts,
// 					   __e_blk_writes_per_fs_write_counts_fname);
// 	logMessage(FS_LOG_LEVEL,
// 			   "block writes per fs write counts, %lu records:\n[%s]\n",
// 			   e_blk_writes_per_fs_write_counts.size(),
// 			   __e_blk_writes_per_fs_write_counts.c_str());

// 	// Log the number of block reads per other fs op
// 	std::string __e_blk_reads_per_fs_other_counts =
// 		vec_to_str<long>(e_blk_reads_per_fs_other_counts);
// 	std::string __e_blk_reads_per_fs_other_counts_fname(
// 		"__e_blk_reads_per_fs_other_counts");
// 	write_lats_to_file(__e_blk_reads_per_fs_other_counts,
// 					   __e_blk_reads_per_fs_other_counts_fname);
// 	logMessage(FS_LOG_LEVEL,
// 			   "block reads per fs other counts, %lu records:\n[%s]\n",
// 			   e_blk_reads_per_fs_other_counts.size(),
// 			   __e_blk_reads_per_fs_other_counts.c_str());

// 	// Log the number of block writes per other fs op
// 	std::string __e_blk_writes_per_fs_other_counts =
// 		vec_to_str<long>(e_blk_writes_per_fs_other_counts);
// 	std::string __e_blk_writes_per_fs_other_counts_fname(
// 		"__e_blk_writes_per_fs_other_counts");
// 	write_lats_to_file(__e_blk_writes_per_fs_other_counts,
// 					   __e_blk_writes_per_fs_other_counts_fname);
// 	logMessage(FS_LOG_LEVEL,
// 			   "block writes per fs other counts, %lu records:\n[%s]\n",
// 			   e_blk_writes_per_fs_other_counts.size(),
// 			   __e_blk_writes_per_fs_other_counts.c_str());

// 	// Log total enclave read latencies
// 	std::string __e_read__lats = vec_to_str<long>(e_read__lats);
// 	std::string __e_read__lats_fname("__e_read__lats");
// 	write_lats_to_file(__e_read__lats, __e_read__lats_fname);
// 	logMessage(FS_LOG_LEVEL,
// 			   "Read latencies (overall, us, %lu records):\n[%s]\n",
// 			   e_read__lats.size(), __e_read__lats.c_str());

// 	// Log non-network-related enclave read latencies
// 	std::string __e_read__e_lats = vec_to_str<long>(e_read__e_lats);
// 	std::string __e_read__e_lats_fname("__e_read__e_lats");
// 	write_lats_to_file(__e_read__e_lats, __e_read__e_lats_fname);
// 	logMessage(FS_LOG_LEVEL,
// 			   "Read latencies (handler, us, %lu records):\n[%s]\n",
// 			   e_read__e_lats.size(), __e_read__e_lats.c_str());

// 	// Log network-related sends for enclave writes
// 	std::string __e_read__net_c_send_lats =
// 		vec_to_str<long>(e_read__net_c_send_lats);
// 	std::string __e_read__net_c_send_lats_fname("__e_read__net_c_send_lats");
// 	write_lats_to_file(__e_read__net_c_send_lats,
// 					   __e_read__net_c_send_lats_fname);
// 	logMessage(
// 		FS_LOG_LEVEL,
// 		"Write latencies (network sends client, us, %lu records):\n[%s]\n",
// 		e_read__net_c_send_lats.size(), __e_read__net_c_send_lats.c_str());

// 	// Log total enclave write latencies
// 	std::string __e_write__lats = vec_to_str<long>(e_write__lats);
// 	std::string __e_write__lats_fname("__e_write__lats");
// 	write_lats_to_file(__e_write__lats, __e_write__lats_fname);
// 	logMessage(FS_LOG_LEVEL,
// 			   "Write latencies (overall, us, %lu records):\n[%s]\n",
// 			   e_write__lats.size(), __e_write__lats.c_str());

// 	// Log non-network-related enclave write latencies
// 	std::string __e_write__e_lats = vec_to_str<long>(e_write__e_lats);
// 	std::string __e_write__e_lats_fname("__e_write__e_lats");
// 	write_lats_to_file(__e_write__e_lats, __e_write__e_lats_fname);
// 	logMessage(FS_LOG_LEVEL,
// 			   "Write latencies (handler, us, %lu records):\n[%s]\n",
// 			   e_write__e_lats.size(), __e_write__e_lats.c_str());

// 	// Log network-related sends for enclave writes
// 	std::string __e_write__net_c_send_lats =
// 		vec_to_str<long>(e_write__net_c_send_lats);
// 	std::string __e_write__net_c_send_lats_fname("__e_write__net_c_send_lats");
// 	write_lats_to_file(__e_write__net_c_send_lats,
// 					   __e_write__net_c_send_lats_fname);
// 	logMessage(
// 		FS_LOG_LEVEL,
// 		"Write latencies (network sends client, us, %lu records):\n[%s]\n",
// 		e_write__net_c_send_lats.size(), __e_write__net_c_send_lats.c_str());
// }

// void write_lats_to_file(std::string __e__lats, std::string __e__lats_fname) {
// 	int ocall_ret = -1;
// 	if ((ocall_write_to_file(&ocall_ret, (uint32_t)__e__lats_fname.size() + 1,
// 							 __e__lats_fname.c_str(),
// 							 (uint32_t)__e__lats.size() + 1,
// 							 __e__lats.c_str()) != SGX_SUCCESS) ||
// 		(ocall_ret != BFS_SUCCESS)) {
// 		logMessage(LOG_ERROR_LEVEL, "Error in ocall_write_to_file for [%s]",
// 				   __e__lats.c_str());
// 		abort();
// 	}
// #endif
// }

// /* These are wrappers around the methods provided by the lwext4 backend. */
// static int do_lwext4_init(void) { return __do_lwext4_init(); }

// static int do_lwext4_mkfs(void) { return __do_lwext4_mkfs(); }

// static int do_lwext4_mount(void) { return __do_lwext4_mount(); }

// static int do_lwext4_getattr(BfsUserContext *usr, std::string path,
// 							 bfs_uid_t *uid, bfs_ino_id_t *fino,
// 							 uint32_t *fmode, uint64_t *fsize) {
// 	return __do_lwext4_getattr(usr, path.c_str(), uid, fino, fmode, fsize);
// }
