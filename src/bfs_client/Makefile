#
# @file Makefile
# @brief Makefile to build the client subsystem.
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
BFS_LIB_DEBUG:=libbfs_client_debug.a
BFS_EX_DEBUG:=bfs_client
BFS_EX_OBJ_DEBUG:=bfs_client_main.exec.debug.o
BFS_TEST_DEBUG:=bfs_client_test
BFS_TEST_OBJ_DEBUG:=bfs_client_test.test.debug.o

# Specify source files for each build mode
lib_debug_cpp_files := bfs_client.cpp
lib_debug_cpp_objects := $(lib_debug_cpp_files:.cpp=.debug.o)
debug_dep := Makefile.debug.dep
lib_nonenclave_cpp_files :=
lib_nonenclave_cpp_objects := $(lib_nonenclave_cpp_files:.cpp=.nonenclave.o)
lib_enclave_cpp_files :=
lib_enclave_cpp_objects := $(lib_enclave_cpp_files:.cpp=.enclave.o)

debug_common_cpp_flags += `pkg-config fuse3 --cflags`

# Subsystem specific lib dependencies
debug_common_link_flags+=-Wl,--start-group `pkg-config fuse3 --libs` -lbfs_client_debug -lbfs_comm_debug -lbfs_util_debug -lgcrypt -Wl,--end-group
nonenclave_common_link_flags +=
enclave_common_link_flags +=

# Shared recipes for each subsystem
include $(BUILD_DIR)/main.mk
