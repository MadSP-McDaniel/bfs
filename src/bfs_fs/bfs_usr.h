/**
 * @file bfs_usr.h
 * @brief The interface for user identities of the file system.
 */

#ifndef BFS_USR_H
#define BFS_USR_H

#include "bfsSecAssociation.h"
#include <bfs_common.h>

/**
 * @brief This contains the information about a connected user. It encapsulates
 * the context of a connected user (e.g., user attributes and security
 * associations).
 */
class BfsUserContext {
public:
	BfsUserContext(bfs_uid_t = 0, bfsSecAssociation * = NULL);
	~BfsUserContext() { delete sa; }

	/* Get the user id */
	bfs_uid_t get_uid();

	bfs_uid_t get_send_seq();
	bfs_uid_t get_recv_seq();
	void inc_send_seq();
	void inc_recv_seq();

	/* Get the security association between the user/server */
	bfsSecAssociation *get_SA();

private:
	bfs_uid_t uid;		   /* User id */
	bfsSecAssociation *sa; /* Security assoc between the user and server */
	uint32_t user_send_seq, user_recv_seq;
	// ... other attributes (e.g., groups, shared files to/from other users)
};

#endif /* BFS_USR_H */
