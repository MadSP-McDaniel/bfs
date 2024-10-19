/**
 * @file bfs_cache.h
 * @brief The declarations and definitions for the templated cache class.
 */

#ifndef BFS_CACHE_H
#define BFS_CACHE_H

#include <cstdint>
#include <pthread.h>
#include <string>
#include <unordered_map>

class CacheableObject {
public:
	CacheableObject();
	virtual ~CacheableObject();

	/* Check if object is dirty and needs to be sync'd with block device */
	bool is_dirty();

	void set_dirty(bool);

	// /*
	//  * Check if object is valid (might be marked invalid on when being
	//  deleted
	//  * to indicate to other threads with access to the pointer that it is no
	//  * longer valid).
	//  */
	// bool is_valid();
	// void set_valid(bool);

	/**
	 * Release the lock. See for more info about lock handling with
	 * exceptions:
	 * https://wiki.sei.cmu.edu/confluence/display/cplusplus/CON51-CPP. \
	 * +Ensure+actively+held+locks+are+released+on+exceptional+conditions
	 */
	bool lock();
	bool unlock();

protected:
	/**
	 * Used mainly as inode and dentry lock for concurrency control through the
	 * shared caches
	 */
	pthread_mutex_t obj_mutex;
	bool dirty;
};

class baseCacheKey {
public:
	baseCacheKey(void);
	virtual ~baseCacheKey(void);

	int getAge(void) const;
	void setAge(int);

	virtual bool compare(const baseCacheKey &) const = 0;
	virtual std::string toString(void) const = 0;
	virtual baseCacheKey *duplicate(void) const = 0;

private:
	int age;
};

class intCacheKey : public baseCacheKey {
public:
	intCacheKey(void);
	intCacheKey(uint64_t);
	intCacheKey(const baseCacheKey &);
	~intCacheKey(void);

	uint64_t getKey(void) const;

	bool compare(const baseCacheKey &) const override;
	std::string toString(void) const override;
	baseCacheKey *duplicate(void) const override;

private:
	uint64_t key;
};

class stringCacheKey : public baseCacheKey {
public:
	stringCacheKey(void);
	stringCacheKey(std::string);
	stringCacheKey(const baseCacheKey &);
	~stringCacheKey(void);

	std::string getKey(void) const;

	bool compare(const baseCacheKey &) const override;
	std::string toString(void) const override;
	baseCacheKey *duplicate(void) const override;

private:
	std::string key;
};

typedef struct cacheTableValueStruct {
	baseCacheKey *key;
	CacheableObject *value;
	struct cacheTableValueStruct *next;
	struct cacheTableValueStruct *prev;
} cacheTableValue;

class BfsCache {
public:
	BfsCache(void);
	BfsCache(long);
	~BfsCache(void);
	CacheableObject *checkCache(const baseCacheKey &, int key_type = 1,
								bool pop = false, bool wait_lock = true);
	CacheableObject *insertCache(const baseCacheKey &, int key_type = 1,
								 CacheableObject *o = NULL);
	double get_hit_rate(void) const;
	void set_debug(bool);
	void set_max_sz(long);

#ifdef __BFS_DEBUG_NO_ENCLAVE
	static bool unitTest(void);
#endif

private:
	long maxsize;
	int size, vclock, accesses, hits;
	cacheTableValue *table, *table_end;
	pthread_mutex_t cache_mutex;
	bool debug;

	// parallel data structure for fast finds/deletes
	std::unordered_map<std::string, cacheTableValue *> table_map_str;
	std::unordered_map<uint64_t, cacheTableValue *> table_map_int;

	cacheTableValue *find(const baseCacheKey &, int);
	cacheTableValue *remove(cacheTableValue *, int);
	std::string toString(void) const;
};

#endif /* BFS_CACHE_H */
