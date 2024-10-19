#
# @file Makefile
# @brief Environment file for nonenclave-mode code in sgx-based builds.
# Contains common variables that will be used by the _nonenclave mode_
# recipes for each subsystem.
#

# Set output directory for object files
SGX_LIBRARY_PATH:=$(SGX_SDK)/lib64
NONENCLAVE_BUILD_SUBDIR:=obj/nonenclave
ENCLAVE_CONFIG_DIR?=../../config

BUILD_LVL ?= 0

ifeq ($(BUILD_LVL), 0)
  BFS_NONENCLAVE_MODE_COMMON_FLAGS += -O0 -g
else ifeq ($(BUILD_LVL), 1)
  BFS_NONENCLAVE_MODE_COMMON_FLAGS += -O2 -g
else
  BFS_NONENCLAVE_MODE_COMMON_FLAGS += -O2
endif

# Set common search paths
nonenclave_common_search_paths:=$(addprefix $(BFS_HOME)/src/,bfs_utils bfs_comms bfs_blk bfs_device bfs_fs bfs_client) $(BFS_HOME)/src/bfs_fs/lwext4/include $(BFS_HOME)/src/bfs_fs/lwext4/build_generic/include $(BIN_DIR)
nonenclave_common_include_paths:=$(addprefix -I,$(nonenclave_common_search_paths)) -I$(SGX_SDK)/include
nonenclave_common_link_paths:=-L$(BIN_DIR) -L$(SGX_LIBRARY_PATH)
extra_c_flags:=-Wjump-misses-init -Wstrict-prototypes -Wunsuffixed-float-constants

# Set common flags
nonenclave_common_c_flags:=
uname_m := $(shell uname -m)
ifeq ($(uname_m),armv7l)
nonenclave_common_c_flags += -mcpu=cortex-a72+crypto -D_FILE_OFFSET_BITS=64
else
nonenclave_common_c_flags += -m64
endif
nonenclave_common_c_flags += $(BFS_NONENCLAVE_MODE_COMMON_FLAGS) -fpie -fPIC -c -Wall -Wextra -Winit-self -Wpointer-arith -Wreturn-type \
                    -Waddress -Wsequence-point -Wformat-security \
                    -Wmissing-include-dirs -Wfloat-equal -Wundef -Wshadow \
                    -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls \
										$(nonenclave_common_include_paths) -D__BFS_NONENCLAVE_MODE
nonenclave_common_cpp_flags:=$(nonenclave_common_c_flags) -std=c++11
nonenclave_common_link_flags:=$(nonenclave_common_link_paths)

# Declare enclave e/o-call dependencies
nonenclave_mode_bridge_deps :=
nonenclave_mode_test_bridge_deps :=
