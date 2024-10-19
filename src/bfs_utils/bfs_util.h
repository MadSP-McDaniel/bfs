#ifndef BFS_UTIL_INCLUDED
#define BFS_UTIL_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : bfs_util.h
//  Description   : This file contains a collection of utility functions that
//                  all layers of the BFS can access.
//
//  Author   : Patrick McDaniel
//  Created  : Tue 16 Mar 2021 08:22:23 AM EDT
//

// Include files
#include "bfs_common.h"
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// Defines
#define BFSUTIL_HASH_TYPE GCRY_MD_SHA256
#define BFSUTIL_HASH_LENGTH (gcry_md_get_algo_dlen(BFSUTIL_HASH_TYPE))
#define BFSUTIL_ALLCHARS                                                       \
	" !#$%&()*+,-./"                                                           \
	"0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_"                          \
	"abcdefghijklmnopqrstuvwxyz{"                                              \
	"|}~"

// Functions

int generate_md5_signature(char *buf, uint32_t size, char *sig,
						   uint32_t *sigsz);
// Generate MD5 signature from buffer

uint32_t get_random_value(uint64_t min, uint64_t max);
// Using strong randomness, generate random number

int32_t get_random_signed_value(int32_t min, int32_t max);
// Using strong randomness, generate random number (signed)

int get_random_data(char *blk, uint32_t sz);
// Using gcrypt randomness, generate random data

int get_random_alphanumeric_data(char *blk, uint32_t sz);
// Using gcrypt randomness, generate random alphanumberic text data

/* File directory/name operations */
const char *bfs_basename(const char *);

char *bfs_dirname(char *);

long compareTimes(struct timeval *tm1, struct timeval *tm2);
// Compare two timer values

/* Bitmap operations */

// set a bit in a bitmap
void bfs_set_bit(uint64_t nr, void *addr);

// zero out a bit in a bitmap
void bfs_clear_bit(uint64_t nr, void *addr);

// test if a bit in the bitmap is 1
bool bfs_test_bit(uint64_t nr, void *addr);

char *bfs_strdup(const char *);

template <typename T> std::string vec_to_str(std::vector<T> &v) {
	std::string vstr("");

	if (v.empty())
		return vstr;

	for (auto it = v.begin(); it != v.end() - 1; it++)
		vstr += std::to_string(*it) + ", ";

	vstr += std::to_string(*(v.end() - 1));

	return vstr;
}

#endif
