#
# @file Makefile
# @brief Makefile to build the device subsystem of the bfs filesystem.
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
BFS_LIB_DEBUG:=libbfs_dev_debug.a
BFS_EX_DEBUG:=bfs_device
BFS_EX_OBJ_DEBUG:=bfs_device_main.exec.debug.o
BFS_TEST_DEBUG:=bfs_dev_utest
BFS_TEST_OBJ_DEBUG:=bfs_dev_utest.test.debug.o
BFS_LIB_NONENCLAVE_MODE:=libbfs_dev_nonenclave.a
BFS_LIB_ENCLAVE_MODE:=libbfs_dev_enclave.a

# Specify source files for each build mode
lib_debug_cpp_files := bfsRemoteDevice.cpp bfsLocalDevice.cpp bfsDeviceStorage.cpp bfsDeviceLayer.cpp bfsNetworkDevice.cpp bfs_device_ocalls.cpp bfs_device_ecalls.cpp
lib_debug_cpp_objects := $(lib_debug_cpp_files:.cpp=.debug.o)
debug_dep := Makefile.debug.dep
lib_nonenclave_cpp_files := bfsNetworkDevice.cpp bfs_device_ocalls.cpp bfsDeviceLayer.cpp bfsDeviceStorage.cpp bfsRemoteDevice.cpp bfsLocalDevice.cpp
lib_nonenclave_cpp_objects := $(lib_nonenclave_cpp_files:.cpp=.nonenclave.o)
lib_enclave_cpp_files := bfs_device_ecalls.cpp bfsRemoteDevice.cpp bfsLocalDevice.cpp bfsDeviceStorage.cpp bfsDeviceLayer.cpp
lib_enclave_cpp_objects := $(lib_enclave_cpp_files:.cpp=.enclave.o)

# Subsystem specific lib dependencies
debug_common_link_flags+=-Wl,--start-group -lbfs_dev_debug -lbfs_comm_debug -lbfs_util_debug -lgcrypt -Wl,--end-group
nonenclave_common_link_flags += 
enclave_common_link_flags += 

enclave_mode_bridge_deps += $(BIN_DIR)/bfs_enclave_t.o
nonenclave_mode_bridge_deps += $(BIN_DIR)/bfs_enclave_u.o

# Shared recipes for each subsystem
include $(BUILD_DIR)/main.mk
