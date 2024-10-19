/**
 * @file bfs_cache.cpp
 * @brief Definitions for the bfs cache types.
 */

#include <pthread.h>

#include "bfsUtilError.h"
#include "bfsUtilLayer.h"
#include "bfs_cache.h"
#include "bfs_log.h"
#include "bfs_util.h"

/*****************************************************************************/
/*                        CacheableObject definitions                        */
/*****************************************************************************/
CacheableObject::CacheableObject() {
	if (pthread_mutex_init(&obj_mutex, NULL) != 0)
		abort();
	// valid = true;
	dirty = true;

	// lock it on create
	lock();
}

CacheableObject::~CacheableObject() { pthread_mutex_destroy(&obj_mutex); }

bool CacheableObject::is_dirty() { return dirty; }

void CacheableObject::set_dirty(bool b) { dirty = b; }

/**
 * @brief Try to acquire a lock.
 *
 * @return bool: true if acquire successful, false if not
 */
bool CacheableObject::lock() { return pthread_mutex_lock(&obj_mutex) == 0; }

/**
 * @brief Try to release a lock.
 *
 * @return bool: true if release successful, false if not
 */
bool CacheableObject::unlock() { return pthread_mutex_unlock(&obj_mutex) == 0; }

/*****************************************************************************/
/*                            baseCacheKey definitions                       */
/*****************************************************************************/
baseCacheKey::baseCacheKey(void) : age(0) {}

void baseCacheKey::setAge(int ag) { age = ag; }

int baseCacheKey::getAge(void) const { return (age); }

baseCacheKey::~baseCacheKey(void) {}

/*****************************************************************************/
/*                            intCacheKey definitions                        */
/*****************************************************************************/
intCacheKey::intCacheKey(void) {}

intCacheKey::intCacheKey(uint64_t k) : key(k) {}

intCacheKey::intCacheKey(const baseCacheKey &k) {
	const intCacheKey *ptr = dynamic_cast<const intCacheKey *>(&k);

	if (ptr == NULL) // fatal
		throw new bfsUtilError("Failed int key cast in constructor\n");

	key = ptr->getKey();
}

intCacheKey::~intCacheKey(void) {}

uint64_t intCacheKey::getKey(void) const { return key; }

bool intCacheKey::compare(const baseCacheKey &ky) const {
	const intCacheKey *ptr = dynamic_cast<const intCacheKey *>(&ky);

	if (ptr == NULL) // fatal
		throw new bfsUtilError("Failed dynamic cast to int cache key\n");

	return (key == ptr->getKey());
}

std::string intCacheKey::toString(void) const { return (std::to_string(key)); }

baseCacheKey *intCacheKey::duplicate(void) const {
	return (new intCacheKey(*this));
};

/*****************************************************************************/
/*                            stringCacheKey definitions                     */
/*****************************************************************************/
stringCacheKey::stringCacheKey(void) {}

stringCacheKey::stringCacheKey(std::string k) : key(k) {}

stringCacheKey::stringCacheKey(const baseCacheKey &k) {
	const stringCacheKey *ptr = dynamic_cast<const stringCacheKey *>(&k);

	if (ptr == NULL) // fatal
		throw new bfsUtilError("Failed string key cast in constructor\n");

	key = ptr->getKey();
}

stringCacheKey::~stringCacheKey(void) {}

std::string stringCacheKey::getKey(void) const { return key; }

bool stringCacheKey::compare(const baseCacheKey &ky) const {
	const stringCacheKey *ptr = dynamic_cast<const stringCacheKey *>(&ky);

	if (ptr == NULL) // fatal
		throw new bfsUtilError("Failed dynamic cast to string cache key\n");

	return (key == ptr->getKey());
}

std::string stringCacheKey::toString(void) const { return key; }

baseCacheKey *stringCacheKey::duplicate(void) const {
	return (new stringCacheKey(*this));
};

/*****************************************************************************/
/*                              BfsCache definitions                         */
/*****************************************************************************/
BfsCache::BfsCache(void)
	: maxsize(bfsUtilLayer::getUtilLayerCacheSizeLimit()), size(0), vclock(0),
	  accesses(0), hits(0), table(NULL), table_end(NULL) {
	if (pthread_mutex_init(&cache_mutex, NULL) != 0)
		abort();

	debug = false;
}

BfsCache::BfsCache(long maxsz) : BfsCache() { maxsize = maxsz; }

BfsCache::~BfsCache(void) { pthread_mutex_destroy(&cache_mutex); }

/**
 * @brief Tries to insert value into the cache.
 *
 * @param key: key to insert by
 * @param val: value to insert with key
 * @return void*: a non-NULL pointer on success (either the new val itself or
 * the val it overwrote for the same key), throws bfsUtilError on failure
 */
CacheableObject *BfsCache::insertCache(const baseCacheKey &key, int key_type,
									   CacheableObject *val) {
	/**
	 * See if already in cache, otherwise look for oldest entry (as needed,
	 * remove it), then insert the new item into the cache.
	 */
	cacheTableValue *ptr;
	CacheableObject *ret = val;

	if (pthread_mutex_lock(&cache_mutex) != 0)
		throw new bfsUtilError("Error when acquiring lock\n");

	if (!(ptr = find(key, key_type))) {
		if (size >= maxsize) {
			// if (!(ptr = table)) { // sanity check
			// 	if (pthread_mutex_unlock(&cache_mutex) != 0)
			// 		throw new bfsUtilError("Error when releasing lock\n");

			// 	throw new bfsUtilError("Cache in inconsistent state\n");
			// }

			// oldest = NULL;

			// while (ptr != NULL) {
			// 	if ((oldest == NULL) ||
			// 		(ptr->key->getAge() < oldest->key->getAge())) {
			// 		oldest = ptr;
			// 	}

			// 	ptr = ptr->next;
			// }

			// sanity check
			if (!table || !table_end) {
				if (pthread_mutex_unlock(&cache_mutex) != 0)
					throw new bfsUtilError("Error when releasing lock\n");

				throw new bfsUtilError("Cache in inconsistent state\n");
			}

			// just pull oldest from front of list
			logMessage(LOG_INFO_LEVEL, "Removing oldest Key [%s]",
					   table->key->toString().c_str());

			/* Remove from the cache. */
			ret = table->value;		 // save before table ptr is updated
			remove(table, key_type); // do remove
		}

		logMessage(LOG_INFO_LEVEL, "Inserting value at Key [%s]",
				   key.toString().c_str());

		ptr = new cacheTableValue;
		ptr->key = key.duplicate();
		ptr->key->setAge(vclock);
		ptr->value = val;

		// check if cache was empty (if so, set head and tail ptr to the new
		// entry); note: should both be null or non-null
		// Note: goal here is to make a circular queue
		if (!table && !table_end) { // empty cache
			table = ptr;
			table_end = ptr;
			ptr->prev = table_end;		 // point back around to tail
			ptr->next = table;			 // point around to head
		} else if (table && table_end) { // non-empty cache
			// insert at back (and we will pop from front)
			ptr->prev = table_end;
			ptr->next = table;
			table_end->next = ptr;
			table_end = ptr;
		} else {
			logMessage(LOG_ERROR_LEVEL, "Cache head/tail ptrs inconsistent");
			abort(); // bad
		}

		// add to map now
		if (key_type == 1) {
			const intCacheKey *kptr = dynamic_cast<const intCacheKey *>(&key);
			table_map_int[kptr->getKey()] = ptr;
		} else {
			const stringCacheKey *kptr =
				dynamic_cast<const stringCacheKey *>(&key);
			table_map_str[kptr->getKey()] = ptr;
		}

		size++;
	} else {
		logMessage(LOG_INFO_LEVEL, "Update value at Key [%s]",
				   key.toString().c_str());
		ptr->key->setAge(vclock);

		/**
		 * If same key, but the caller wants to replace with diff value buffer
		 * (should be deleted by the caller since only they are aware of the
		 * actual non-void pointer type).
		 *
		 * Just update the cache entry to point to the new pointer, then move
		 * entry to end of queue.
		 */
		if (val != ptr->value) {
			ret = ptr->value; // save current val
			ptr->value = val; // update with new val

			// add new ptr to map now
			if (key_type == 1) {
				const intCacheKey *kptr =
					dynamic_cast<const intCacheKey *>(&key);
				table_map_int[kptr->getKey()] = ptr;
			} else {
				const stringCacheKey *kptr =
					dynamic_cast<const stringCacheKey *>(&key);
				table_map_str[kptr->getKey()] = ptr;
			}
		}

		// now move the cache entry to back of list
		if (!table || !table_end) {
			if (pthread_mutex_unlock(&cache_mutex) != 0)
				throw new bfsUtilError("Error when releasing lock\n");

			// should be at least 1 entry if it was indicated as found
			throw new bfsUtilError("Cache in inconsistent state\n");
		}

		// Note: Handles if ptr is either head, tail, or in the middle

		if (size > 1) {
			if (ptr == table_end) {
				// dont need to do anything
			} else {
				// update head pointer if needed (OK if size==1)
				if (ptr == table)
					table = table->next;

				// link ptr's prev and next
				if (ptr->prev)
					ptr->prev->next = ptr->next;
				if (ptr->next)
					ptr->next->prev = ptr->prev;

				// finally complete insert at back
				ptr->prev = table_end;
				ptr->next = table;
				table_end->next = ptr; // (OK if size==1)
				table_end = ptr;
			}
		}

		// if (ptr == table) {
		// 	// update the head ptr
		// 	table = ptr->next;
		// 	if (table)
		// 		table->prev = NULL;

		// 	// complete insert at back
		// 	ptr->prev = table_end;
		// 	ptr->next = NULL;
		// 	table_end->next = ptr;
		// 	table_end = ptr;
		// } else if (ptr == table_end) {
		// 	// dont need to do anything
		// } else {
		// 	// link ptr's prev and next
		// 	ptr->prev->next = ptr->next;
		// 	if (ptr->prev->next)
		// 		ptr->prev->next->prev = ptr->prev;

		// 	// complete insert at back
		// 	ptr->prev = table_end;
		// 	ptr->next = NULL;
		// 	table_end->next = ptr;
		// 	table_end = ptr;
		// }
	}

	vclock++;

	if (pthread_mutex_unlock(&cache_mutex) != 0)
		throw new bfsUtilError("Error when releasing lock\n");

	return ret;
}

/**
 * @brief Tries to find an entry in the cache associated with the given key.
 *
 * @param key: key to search by
 * @param pop: flag indicating whether to remove the item if found
 * @param wait_lock: flag indicating to wait/get the object lock during check
 * @return void*: pointer to the cache entry's value
 */
CacheableObject *BfsCache::checkCache(const baseCacheKey &key, int key_type,
									  bool pop, bool wait_lock) {
	cacheTableValue *ptr;

	if (pthread_mutex_lock(&cache_mutex) != 0)
		throw new bfsUtilError("Error when acquiring lock\n");

	if (!(ptr = find(key, key_type))) {
		if (pthread_mutex_unlock(&cache_mutex) != 0)
			throw new bfsUtilError("Error when releasing lock\n");

		return NULL;
	}

	ptr->key->setAge(vclock);
	vclock++;

	if (pop && !remove(ptr, key_type))
		throw new bfsUtilError("Cache pop failed\n");

	/**
	 * Acquire the lock for the cacheable object before unlocking the cache
	 * mutex. Problem: dangling pointers if the caller gets the pointer but does
	 * not win the race to get the lock on the object. So doing this ensures
	 * consistency on the cacheable object across multiple threads. For example:
	 * if we just release the cache lock and the caller then tries to acquire
	 * the object lock, then another thread might also enter e.g. an insert for
	 * a new inode, somehow force an ejection on the current inode from the
	 * cache, which invokes the callback that ends up deleting the pointer. If
	 * the other thread gets the inode lock before this thread does, then this
	 * thread will have a dangling pointer.
	 *
	 * If we ensure that the caller that accesses the cache can actually acquire
	 * the lock for the object (by holding the cache lock until the object lock
	 * is acquired, even if this caller has to wait for another thread using the
	 * object to finish up), we can prevent those races. Doesn't matter for
	 * multiple readers but undefined behavior for multiple writers/deleters.
	 *
	 * Note that we don't have to check if the pointer is valid or not.  The
	 * remove operation occurs before the callback is invoked, so the find won't
	 * ever return an invalid pointer.
	 */
	if (wait_lock && !ptr->value->lock())
		throw new bfsUtilError("Error when acquiring lock\n");

	if (pthread_mutex_unlock(&cache_mutex) != 0)
		throw new bfsUtilError("Error when releasing lock\n");

	return ptr->value;
}

cacheTableValue *BfsCache::find(const baseCacheKey &key, int key_type) {
	// cacheTableValue *ptr;

	accesses++;

	// if (!(ptr = table))
	// 	return NULL;

	if (!table || !table_end)
		return NULL;

	// return key.do_find(table_map);

	if (key_type == 1) {
		const intCacheKey *kptr = dynamic_cast<const intCacheKey *>(&key);
		if (table_map_int.find(kptr->getKey()) != table_map_int.end()) {
			hits++; // count both cold+capacity misses
			return table_map_int[kptr->getKey()];
		}
	} else {
		const stringCacheKey *kptr = dynamic_cast<const stringCacheKey *>(&key);
		if (table_map_str.find(kptr->getKey()) != table_map_str.end()) {
			hits++; // count both cold+capacity misses
			return table_map_str[kptr->getKey()];
		}
	}

	// while (ptr != NULL) {
	// 	if (ptr->key->compare(key)) {
	// 		hits++; // count both cold+capacity misses
	// 		return ptr;
	// 	}

	// 	ptr = ptr->next;
	// }

	return NULL;
}

cacheTableValue *BfsCache::remove(cacheTableValue *ptr, int key_type) {
	if (!ptr || !table || !table_end)
		throw new bfsUtilError("Pointer bad in cache remove\n");

	// remove from map first (before deleting key)
	if (key_type == 1) {
		const intCacheKey *kptr = dynamic_cast<const intCacheKey *>(ptr->key);
		if (table_map_int.find(kptr->getKey()) != table_map_int.end())
			table_map_int.erase(kptr->getKey());
		else
			abort(); // the map entry should be there
	} else {
		const stringCacheKey *kptr =
			dynamic_cast<const stringCacheKey *>(ptr->key);
		if (table_map_str.find(kptr->getKey()) != table_map_str.end())
			table_map_str.erase(kptr->getKey());
		else
			abort(); // the map entry should be there
	}

	delete ptr->key;

	// See if we need to advance head or tail pointers forward by one entry
	// if (ptr == table) // update head pointer if needed
	// 	table = ptr->next;
	// else if (ptr == table_end) // update tail pointer if needed
	// 	table_end = ptr->prev;
	if (size == 1) {
		// prevent circular buffer from borking
		table = NULL;
		table_end = NULL;
	} else {
		if (ptr == table)
			table = table->next;
		else if (ptr == table_end)
			table_end = table_end->prev;

		// link ptr's prev and next
		if (ptr->prev)
			ptr->prev->next = ptr->next;
		if (ptr->next)
			ptr->next->prev = ptr->prev;
	}

	// if (ptr == table) {
	// 	delete ptr->key; // delete the key but not the val (returned to caller)
	// 	table = ptr->next; // now update the head ptr
	// 	if (table)
	// 		table->prev = NULL;
	// } else if (ptr == table_end) {
	// 	delete ptr->key;
	// 	table_end = table_end->prev;
	// 	if (table_end)
	// 		table_end->next = NULL;
	// } else {
	// 	delete ptr->key;
	// 	ptr->prev->next = ptr->next; // link prev and next
	// 	if (ptr->prev->next)
	// 		ptr->prev->next->prev = ptr->prev;
	// }

	size--;

	return ptr;
}

string BfsCache::toString(void) const {
	cacheTableValue *ptr = table;
	std::string str = "BFS_CACHE( ";

	while (ptr) {
		str += "[" + ptr->key->toString() +
			   ", age=" + to_string(ptr->key->getAge()) + "]";
		ptr = ptr->next;
	}

	str += " )";

	return str;
}

double BfsCache::get_hit_rate(void) const { return hits * 1.0 / accesses; }

void BfsCache::set_debug(bool d) { debug = d; }

void BfsCache::set_max_sz(long s) { maxsize = s; }

#ifdef __BFS_DEBUG_NO_ENCLAVE
bool BfsCache::unitTest(void) {
	intCacheKey ikey;
	stringCacheKey skey;
	const int test_cache_sz = 50;
	BfsCache icache(test_cache_sz), scache(test_cache_sz);
	int i, r;
	CacheableObject *check_ptr = new CacheableObject();
	const int CACHE_UTEST_ITERATIONS = 10000, MAX_KEY_VAL = 100;

	logMessage(LOG_INFO_LEVEL, "Starting cache test ...");
	icache.set_debug(true);
	scache.set_debug(true);

	for (i = 0; i < CACHE_UTEST_ITERATIONS; i++) {
		r = get_random_value(0, MAX_KEY_VAL);
		logMessage(LOG_INFO_LEVEL,
				   "Iteration [%i]: inserting icache entry [%d]\n", i, r);
		ikey = intCacheKey(r);

		if (icache.insertCache(ikey, 1, check_ptr) != check_ptr) {
			logMessage(LOG_INFO_LEVEL, "Failed inserting icache entry\n");
			return false;
		}

		if (icache.checkCache(ikey, 1) != check_ptr) {
			logMessage(LOG_INFO_LEVEL,
					   "Failed getting inserted icache entry\n");
			return false;
		}
	}

	for (i = 0; i < CACHE_UTEST_ITERATIONS; i++) {
		r = get_random_value(0, MAX_KEY_VAL);
		logMessage(LOG_INFO_LEVEL,
				   "Iteration [%i]: inserting scache entry [%d]\n", i, r);
		skey = stringCacheKey(std::to_string(r));

		if (scache.insertCache(skey, 0, check_ptr) != check_ptr) {
			logMessage(LOG_INFO_LEVEL, "Failed inserting scache entry\n");
			return false;
		}

		if (scache.checkCache(skey, 0) != check_ptr) {
			logMessage(LOG_INFO_LEVEL,
					   "Failed getting inserted scache entry\n");
			return false;
		}
	}

	logMessage(LOG_INFO_LEVEL, "Cache test completed successfully.");
	logMessage(LOG_INFO_LEVEL, "Integer key cache hit rate : %.2f%%\n",
			   icache.get_hit_rate() * 100.0);
	logMessage(LOG_INFO_LEVEL, "String  key cache hit rate : %.2f%%\n",
			   scache.get_hit_rate() * 100.0);

	return true;
}
#endif
