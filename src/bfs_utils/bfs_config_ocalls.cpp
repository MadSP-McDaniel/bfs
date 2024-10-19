/**
 * @file bfs_config_ocalls.cpp
 * @brief Provides support for the enclave code to use configuration layer.
 */

#include "bfs_config_ocalls.h"
#include "bfs_common.h"
#include "bfs_log.h"
#include <bfsConfigLayer.h>
#include <cstring>
#include <stdio.h>

int32_t ocall_load_system_config() {
	if (bfsConfigLayer::loadSystemConfiguration() != 0)
		return BFS_FAILURE;

	return BFS_SUCCESS;
}

/**
 * @brief Get a pointer (in untrusted memory) to a config item. Use int64_t for
 * convenience of copying pointer addresses.
 *
 * @return int64_t: pointer to the config item
 */
int64_t ocall_getConfigItem(const char *cfgtag, long unsigned len) {
	(void)len;
	auto x = (int64_t)(bfsConfigLayer::getConfigItem(std::string(cfgtag)));
	return x;
}

int64_t ocall_bfsCfgItemType(int64_t config_ptr) {
	return ((bfsCfgItem *)config_ptr)->bfsCfgItemType();
}

int32_t ocall_bfsCfgItemNumSubItems(int64_t config_ptr) {
	return ((bfsCfgItem *)config_ptr)->bfsCfgItemNumSubItems();
}

int64_t ocall_getSubItemByIndex(int64_t config_ptr, int idx) {
	return (int64_t)(((bfsCfgItem *)config_ptr)->getSubItemByIndex(idx));
}

int64_t ocall_getSubItemByName(int64_t devconfig_ptr, const char *name,
							   long unsigned name_len) {
	(void)name_len;
	return (int64_t)(((bfsCfgItem *)devconfig_ptr)->getSubItemByName(name));
}

int64_t ocall_bfsCfgItemValue(int64_t subconfig_ptr, char *subitem_buf,
							  long unsigned buf_len) {
	std::string val = ((bfsCfgItem *)subconfig_ptr)->bfsCfgItemValue();

	if (val.size() == 0)
		return BFS_FAILURE;

	strncpy(subitem_buf, val.c_str(), buf_len);

	return BFS_SUCCESS;
}

int64_t ocall_bfsCfgItemValueLong(int64_t subconfig_ptr) {
	return ((bfsCfgItem *)subconfig_ptr)->bfsCfgItemValueLong();
}

uint64_t ocall_bfsCfgItemValueUnsigned(int64_t subconfig_ptr) {
	return ((bfsCfgItem *)subconfig_ptr)->bfsCfgItemValueUnsigned();
}
