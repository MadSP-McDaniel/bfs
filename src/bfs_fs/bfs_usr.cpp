/**
 * @file bfs_acl.cpp
 * @brief Definitions for user identity classes. The methods provide access to
 * the user context for access control, encryption, etc.
 */

#include "bfs_usr.h"

/**
 * @brief Constructs a new object that holds the context between the server and
 * the user.
 *
 * @param u: user id to identify the user
 * @param s: security association between the server and user
 */
BfsUserContext::BfsUserContext(bfs_uid_t u, bfsSecAssociation *s) {
	uid = u;
	sa = s;
	user_send_seq = 0;
	user_recv_seq = 0;
}

/**
 * @brief Returns the user id.
 *
 * @return bfs_uid_t: user id of the context object
 */
bfs_uid_t BfsUserContext::get_uid() { return uid; }

bfs_uid_t BfsUserContext::get_send_seq() { return user_send_seq; }

bfs_uid_t BfsUserContext::get_recv_seq() { return user_recv_seq; }

void BfsUserContext::inc_send_seq() { user_send_seq++; }

void BfsUserContext::inc_recv_seq() { user_recv_seq++; }

/**
 * @brief Returns the security association.
 *
 * @return bfsSecAssociation*: security association of the context object.
 */
bfsSecAssociation *BfsUserContext::get_SA() { return sa; }
