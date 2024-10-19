#
# @file Makefile
# @brief Environment file for non-sgx builds. Contains common variables that
# will be used by the _debug_ recipes for each subsystem.
#

# Set output directory for object files
DEBUG_MODE_BUILD_SUBDIR:=obj/debug

# Flag indicating whether or not to include debug symbols
# in this debug (ie no-enclave) mode build.
BUILD_LVL ?= 0

ifeq ($(BUILD_LVL), 0)
  BFS_DEBUG_MODE_COMMON_FLAGS += -O0 -g
else ifeq ($(BUILD_LVL), 1)
  BFS_DEBUG_MODE_COMMON_FLAGS += -O2 -g
else
  BFS_DEBUG_MODE_COMMON_FLAGS += -O2
endif

# Set common search paths
debug_common_search_paths:=$(addprefix $(BFS_HOME)/src/,bfs_utils bfs_comms bfs_blk bfs_device bfs_fs bfs_client) $(BFS_HOME)/src/bfs_fs/lwext4/include $(BFS_HOME)/src/bfs_fs/lwext4/build_generic/include $(BIN_DIR)
debug_common_include_paths:=$(addprefix -I,$(debug_common_search_paths))
debug_common_link_paths:=-L$(BIN_DIR)
extra_c_flags:=-Wjump-misses-init -Wstrict-prototypes -Wunsuffixed-float-constants

# Get the architecture and set common flags
debug_common_c_flags:=
uname_m := $(shell uname -m)
ifeq ($(uname_m),armv7l)
debug_common_c_flags += -mcpu=cortex-a72+crypto -D_FILE_OFFSET_BITS=64
else
debug_common_c_flags += -m64
endif
# debug_common_c_flags += $(BFS_DEBUG_MODE_COMMON_FLAGS) -fpie -fPIC -c -Wall -Wextra -Wpointer-arith \
#                     -Wformat-security -Wshadow -Wredundant-decls \
# 										$(debug_common_include_paths) -D__BFS_DEBUG_NO_ENCLAVE
debug_common_c_flags += $(BFS_DEBUG_MODE_COMMON_FLAGS) -fpie -fPIC -c -Wall -Wextra -Winit-self -Wpointer-arith -Wreturn-type \
                    -Waddress -Wsequence-point -Wformat-security \
                    -Wmissing-include-dirs -Wfloat-equal -Wundef -Wshadow \
                    -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls \
										$(debug_common_include_paths) -D__BFS_DEBUG_NO_ENCLAVE

debug_common_cpp_flags:=$(debug_common_c_flags) -std=c++11
debug_common_link_flags:=$(debug_common_link_paths)
