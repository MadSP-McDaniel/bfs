/**
 * @file bfs_acl.cpp
 * @brief Definitions for the access control layer interface.
 */

#include <pthread.h>

#include "bfs_acl.h"
#include "bfs_core.h"
#include "bfs_fs_layer.h"
#include "bfs_server.h"
#include <bfsConfigLayer.h>
#include <bfs_log.h>

uint64_t BfsACLayer::bfs_aclayer_log_level;
std::unordered_map<void *, BfsUserContext *>
	BfsACLayer::user_contexts;	/* Map of all users connected to the server */
bfs_uid_t BfsACLayer::next_uid; /* Counter for user id allocation */
bool BfsACLayer::ACLayerInitialized = false;

/**
 * @brief Clean up the access control layer by destroying all of the
 * user_context objects.
 */
BfsACLayer::~BfsACLayer() {
	for (auto it = user_contexts.begin(); it != user_contexts.end(); it++)
		delete it->second;
}

/**
 * @brief Initialize the (enclave-mode) utils and access control layer.
 *
 * @return int: BFS_SUCCESS if success, BFS_FAILURE if failure
 */
int BfsACLayer::BfsACLayer_init() {

	if (bfsUtilLayer::bfsUtilLayerInit() != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL, "Failed bfsUtilLayerInit\n");
		return BFS_FAILURE;
	}

	// TODO: grab these flags from config
	bfs_aclayer_log_level = registerLogLevel("AC_LOG_LEVEL", 1);
	next_uid = 1001; // reserve 0 for root and start at 1000 (first allocated
					 // linux uid)
	ACLayerInitialized = true;

	logMessage(AC_LOG_LEVEL, "Access control layer initialized.");

	return BFS_SUCCESS;
}

/**
 * @brief Check if the access control layer is initialized.
 *
 * @return bool: true if initialized, false if not
 */
bool BfsACLayer::initialized() { return ACLayerInitialized; }

/**
 * @brief Adds a new user context to the file system in-memory structures. For
 * now, the encryption key for the client (user)/server comms will just use the
 * hard-coded key in the config. Later we will add an auth protocol to establish
 * the key and identity.
 *
 * @param conn_ptr: socket/connection ptr to map to the user SA
 */
BfsUserContext *BfsACLayer::add_user_context(void *conn_ptr) {
	bfsCfgItem *config, *sacfg;

	if (!ACLayerInitialized)
		return NULL;

		// TODO: insert eg DH key exchange protocol here in place of using
		// pre-shared keys
#ifdef __BFS_ENCLAVE_MODE
	// Check to make sure we were able to load the configuration
	int64_t ret = 0, ocall_status = 0;

	if (((ocall_status = ocall_getConfigItem(&ret, BFS_SERVER_CONFIG,
											 strlen(BFS_SERVER_CONFIG) + 1)) !=
		 SGX_SUCCESS) ||
		(ret == (int64_t)NULL)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getConfigItem\n");
		return NULL;
	}
	config = (bfsCfgItem *)ret;

	if (((ocall_status = ocall_bfsCfgItemType(&ret, (int64_t)config)) !=
		 SGX_SUCCESS) ||
		(ret != bfsCfgItem_STRUCT)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Unable to find crypto configuration in system config "
				   "(ocall_bfsCfgItemType) : %s",
				   BFS_SERVER_CONFIG);
		return NULL;
	}

	// Now get the security context (keys etc.)
	sacfg = NULL;
	if (((ocall_status = ocall_getSubItemByName(
			  (int64_t *)&sacfg, (int64_t)config, "cl_serv_sa",
			  strlen("cl_serv_sa") + 1)) != SGX_SUCCESS)) {
		logMessage(LOG_ERROR_LEVEL, "Failed ocall_getSubItemByName");
		return NULL;
	}
#else
	config = bfsConfigLayer::getConfigItem(BFS_SERVER_CONFIG);

	if (config->bfsCfgItemType() != bfsCfgItem_STRUCT) {
		logMessage(LOG_ERROR_LEVEL,
				   "Unable to find crypto configuration in system config : %s",
				   BFS_SERVER_CONFIG);
		return NULL;
	}

	// Now get the security context (keys etc.)
	sacfg = config->getSubItemByName("cl_serv_sa");
#endif

	auto u = user_contexts.insert(std::make_pair(
		conn_ptr,
		new BfsUserContext(alloc_uid(), new bfsSecAssociation(sacfg))));

	return u.first->second;
}

/**
 * @brief Get the user context mapped by the given socket ptr.
 *
 * @param conn_ptr: the socket/conn ptr to search by
 * @return BfsUserContext*: the mapped user context
 */
BfsUserContext *BfsACLayer::get_user_context(void *conn_ptr) {
	if (ACLayerInitialized) {
		if (user_contexts.find(conn_ptr) != user_contexts.end())
			return user_contexts.at(conn_ptr);
		else
			return NULL;
	}

	return NULL;
}

/**
 * @brief Expects sockets to be mapped to BfsUserContext*.
 *
 * @param usr: the user to check against
 * @param owner: the owner of the object
 * @return bool: true if the user is the owner, false if not
 */
bool BfsACLayer::is_owner(BfsUserContext *usr, bfs_uid_t owner) {
	return ACLayerInitialized && usr && (usr->get_uid() == owner);
}

/**
 * @brief Checks whether or not an access by the given user should be permitted
 * or denied based on the permissions/mode of an object. For now just do coarse
 * grained permissions (i.e., RWX).
 * @param usr: user to check access for
 * @param mode: permissions on the object
 * @return bool: true if allow, false if not
 */
bool BfsACLayer::owner_access_ok(BfsUserContext *usr, uint32_t mode) {
	return ACLayerInitialized && usr && (mode & BFS__S_IRWXU);
}

/**
 * @brief Checks whether or not an access by the given user should be permitted
 * or denied based on the permissions/mode of an object. For now just do coarse
 * grained permissions (i.e., RWX).
 * @param usr: user to check access for
 * @param mode: permissions on the object
 * @return bool: true if allow, false if not
 */
bool BfsACLayer::world_access_ok(BfsUserContext *usr, uint32_t mode) {
	return ACLayerInitialized && usr && (mode & BFS__S_IRWXO);
}

/**
 * @brief Allocate an id for a newly connected user.
 *
 * @return bfs_uid_t: the allocated id
 */
bfs_uid_t BfsACLayer::alloc_uid() {
	if (ACLayerInitialized)
		return next_uid++;

	return 0;
}
