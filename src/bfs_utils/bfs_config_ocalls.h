/**
 * @file bfs_config_ocalls.h
 * @brief Ocall interface.
 */

#ifndef BFS_CONFIG_OCALLS_H
#define BFS_CONFIG_OCALLS_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

int32_t ocall_load_system_config();
int64_t ocall_getConfigItem(const char *, long unsigned);
int64_t ocall_bfsCfgItemType(int64_t);
int32_t ocall_bfsCfgItemNumSubItems(int64_t);
int64_t ocall_getSubItemByIndex(int64_t, int);
int64_t ocall_getSubItemByName(int64_t, const char *, long unsigned);
int64_t ocall_bfsCfgItemValue(int64_t, char *, long unsigned);
int64_t ocall_bfsCfgItemValueLong(int64_t);
uint64_t ocall_bfsCfgItemValueUnsigned(int64_t);

#ifdef __cplusplus
}
#endif

#endif /* BFS_CONFIG_OCALLS_H */
