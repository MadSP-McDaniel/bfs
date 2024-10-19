#
# @file Makefile
# @brief Makefile to build the block subsystem of the bfs filesystem.
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

# Name the targets to build
BFS_LIB_DEBUG:=libbfs_blk_debug.a
BFS_TEST_DEBUG:=bfs_blk_utest
BFS_TEST_OBJ_DEBUG:=bfs_blk_utest.test.debug.o
BFS_LIB_NONENCLAVE_MODE:=libbfs_blk_nonenclave.a
BFS_LIB_ENCLAVE_MODE:=libbfs_blk_enclave.a

# Specify source files for each build mode
lib_debug_cpp_files := bfsBlockLayer.cpp bfsVertBlockCluster.cpp
lib_debug_cpp_objects := $(lib_debug_cpp_files:.cpp=.debug.o)
debug_dep := Makefile.debug.dep
lib_nonenclave_cpp_files := 
lib_nonenclave_cpp_objects := $(lib_nonenclave_cpp_files:.cpp=.nonenclave.o)
lib_enclave_cpp_files := bfsBlockLayer.cpp bfsVertBlockCluster.cpp
lib_enclave_cpp_objects := $(lib_enclave_cpp_files:.cpp=.enclave.o)

# Subsystem specific lib dependencies
debug_common_link_flags+=-Wl,--start-group -lbfs_util_debug -lgcrypt -lbfs_blk_debug -lbfs_dev_debug -lbfs_comm_debug -Wl,--end-group
nonenclave_common_link_flags +=
enclave_common_link_flags += 

# Shared recipes for each subsystem
include $(BUILD_DIR)/main.mk
