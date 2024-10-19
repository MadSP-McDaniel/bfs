#
# @file Makefile
# @brief Makefile to build the comms subsystem.
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
BFS_LIB_DEBUG:=libbfs_comm_debug.a
BFS_TEST_DEBUG:=bfs_comm_utest
BFS_TEST_OBJ_DEBUG:=bfs_commutest.test.debug.o
BFS_LIB_NONENCLAVE_MODE:=libbfs_comm_nonenclave.a
BFS_LIB_ENCLAVE_MODE:=libbfs_comm_enclave.a

# Specify source files for each build mode
lib_debug_cpp_files := bfs_rawnet.cpp bfsNetworkConnection.cpp bfsConnectionMux.cpp bfs_comms_ocalls.cpp bfs_comms_ecalls.cpp
lib_debug_cpp_objects := $(lib_debug_cpp_files:.cpp=.debug.o)
debug_dep := Makefile.debug.dep
lib_nonenclave_cpp_files := bfs_rawnet.cpp bfs_comms_ocalls.cpp bfsConnectionMux.cpp bfsNetworkConnection.cpp
lib_nonenclave_cpp_objects := $(lib_nonenclave_cpp_files:.cpp=.nonenclave.o)
lib_enclave_cpp_files := bfs_comms_ecalls.cpp bfsConnectionMux.cpp bfsNetworkConnection.cpp
lib_enclave_cpp_objects := $(lib_enclave_cpp_files:.cpp=.enclave.o)

# Subsystem specific lib dependencies
debug_common_link_flags+=-Wl,--start-group -lbfs_comm_debug -lbfs_util_debug -lgcrypt -Wl,--end-group
nonenclave_common_link_flags +=-Wl,--start-group -lbfs_comm_nonenclave -lbfs_util_nonenclave -lgcrypt -Wl,--end-group
enclave_common_link_flags += 

enclave_mode_bridge_deps += $(BIN_DIR)/bfs_enclave_t.o

# Shared recipes for each subsystem
include $(BUILD_DIR)/main.mk
