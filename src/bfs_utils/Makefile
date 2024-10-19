#
# @file Makefile
# @brief Makefile to build the utility subsystem.
#

BUILD_DIR:=$(BFS_HOME)/build
BIN_DIR:=$(BFS_HOME)/build/bin

ifeq ($(MAKECMDGOALS), bfs)
include $(BUILD_DIR)/debug.mk
else # bfs_sgx / all / clean
include $(BUILD_DIR)/debug.mk
include $(BUILD_DIR)/nonenclave.mk
include $(BUILD_DIR)/enclave.mk
endif

# Name the targets to build  (empty variable indicate not to build)
BFS_LIB_DEBUG:=libbfs_util_debug.a
BFS_TEST_DEBUG:=bfs_util_utest
BFS_TEST_OBJ_DEBUG:=bfs_util_utest.test.debug.o
BFS_LIB_NONENCLAVE_MODE:=libbfs_util_nonenclave.a
BFS_TEST_NONENCLAVE_MODE:=bfs_util_utest_ne
BFS_TEST_OBJ_NONENCLAVE_MODE:=bfs_util_utest.test.nonenclave.o
BFS_LIB_ENCLAVE_MODE:=libbfs_util_enclave.a
BFS_TEST_SO_ENCLAVE_MODE:=libbfs_util_test_enclave.so # enclave version of lib
BFS_TEST_SO_ENCLAVE_MODE_SIGNED=libbfs_util_test_enclave.signed.so # signed enclave version of lib

# Specify source files for each build mode
lib_debug_cpp_files := bfs_log.cpp bfs_util.cpp bfs_util_ocalls.cpp bfs_util_ecalls.cpp bfs_config_ocalls.cpp  bfs_config_ecalls.cpp  bfsFlexibleBuffer.cpp bfs_base64.cpp bfsUtilLayer.cpp bfs_cache.cpp \
					   bfsCfgParser.cpp bfsCfgStore.cpp bfsCfgItem.cpp bfsConfigLayer.cpp bfsCfgParserSymbol.cpp \
					   bfsCryptoLayer.cpp bfsSecAssociation.cpp bfsCryptoKey.cpp bfsRegExpression.cpp bfs_block.cpp
lib_debug_cpp_objects := $(lib_debug_cpp_files:.cpp=.debug.o)
debug_dep :=
lib_nonenclave_cpp_files := bfs_log.cpp bfs_util.cpp bfs_util_ocalls.cpp bfsFlexibleBuffer.cpp bfs_base64.cpp bfsUtilLayer.cpp bfs_cache.cpp \
							bfsCfgParser.cpp bfsCfgStore.cpp bfsCfgItem.cpp bfsConfigLayer.cpp bfsCfgParserSymbol.cpp bfs_config_ocalls.cpp \
							bfsCryptoLayer.cpp bfsSecAssociation.cpp bfsCryptoKey.cpp bfsRegExpression.cpp bfs_block.cpp
lib_nonenclave_cpp_objects := $(lib_nonenclave_cpp_files:.cpp=.nonenclave.o)
lib_enclave_cpp_files := bfs_log.cpp bfs_util.cpp bfsFlexibleBuffer.cpp bfs_base64.cpp bfsUtilLayer.cpp bfs_cache.cpp \
						 bfsConfigLayer.cpp bfsCfgStore.cpp bfsCfgItem.cpp \
						 bfsCryptoLayer.cpp bfsSecAssociation.cpp bfsCryptoKey.cpp bfsRegExpression.cpp bfs_util_ecalls.cpp bfs_config_ecalls.cpp bfs_block.cpp
lib_enclave_cpp_objects := $(lib_enclave_cpp_files:.cpp=.enclave.o)

# Subsystem specific lib dependencies
debug_common_link_flags +=-Wl,--start-group -lbfs_util_debug -lgcrypt -Wl,--end-group
nonenclave_common_link_flags +=-Wl,--start-group -lbfs_util_nonenclave -lgcrypt -l$(Urts_Library_Name) -Wl,--end-group
enclave_common_link_flags +=-Wl,--start-group -lbfs_util_enclave -lsgx_tstdc -lsgx_tcxx -l$(Crypto_Library_Name) -l$(Trts_Library_Name) -Wl,--end-group

enclave_mode_bridge_deps += $(BIN_DIR)/bfs_enclave_t.o
enclave_mode_test_bridge_deps += $(BIN_DIR)/bfs_enclave_util_test_t.o
nonenclave_mode_bridge_deps += $(BIN_DIR)/bfs_enclave_u.o
nonenclave_mode_test_bridge_deps += $(BIN_DIR)/bfs_enclave_util_test_u.o

# Shared recipes for each subsystem
include $(BUILD_DIR)/main.mk
