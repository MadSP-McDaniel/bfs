/**
 * @file bfs_acl.h
 * @brief The interface for the access control layer of the file system.
 */

#ifndef BFS_ACL_H
#define BFS_ACL_H

#include <unordered_map>

#include "bfs_usr.h"
#include <bfs_common.h>

#define AC_LOG_LEVEL bfs_aclayer_log_level

#define BFS__S_IRWXU 00700 /* read, write, execute/search by owner */
#define BFS__S_IRUSR 00400 /* read permission, owner */
#define BFS__S_IWUSR 00200 /* write permission, owner */
#define BFS__S_IXUSR 00100 /* execute/search permission, owner */

#define BFS__S_IRWXO 00007 /* read, write, execute/search by others */
#define BFS__S_IROTH 00004 /* read permission, others */
#define BFS__S_IWOTH 00002 /* write permission, others */
#define BFS__S_IXOTH 00001 /* execute/search permission, others */

/**
 * @brief This represents the shared access control layer interface for the file
 * system. Can be queried to allocate user identities and check access
 * permissions on objects.
 */
class BfsACLayer {
public:
	/* Initialize the access control layer */
	static int BfsACLayer_init();

	/* Check if the access control layer is initialized */
	static bool initialized();

	/* Adds a new user context to track */
	static BfsUserContext *add_user_context(void *);

	/*  Get the user context given a socket descriptor */
	static BfsUserContext *get_user_context(void *);

	/* Check if the user is the owner of the file */
	static bool is_owner(BfsUserContext *, bfs_uid_t);

	/* Check if owner access is permitted */
	static bool owner_access_ok(BfsUserContext *, uint32_t);

	/* Check if world access is permitted */
	static bool world_access_ok(BfsUserContext *, uint32_t);

	/* Allocate a new user id for a (unidentified) connected client */
	static bfs_uid_t alloc_uid();

private:
	BfsACLayer();
	~BfsACLayer();

	static uint64_t bfs_aclayer_log_level;

	static std::unordered_map<void *, BfsUserContext *>
		user_contexts; /* Map of all connected users */

	static bfs_uid_t next_uid; /* Counter for user id allocation*/
	static bool ACLayerInitialized;
};

#endif /* BFS_ACL_H */
