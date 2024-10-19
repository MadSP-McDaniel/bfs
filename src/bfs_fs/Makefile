#
# @file Makefile
# @brief Makefile to build the fs subsystem.
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

# Targets to build
BFS_LIB_DEBUG:=libbfs_core_debug.a # debug version of lib
BFS_EX_DEBUG:=bfs_server # name of main executable
BFS_EX_OBJ_DEBUG:=bfs_server_main.exec.debug.o # name of executable file with main()
BFS_TEST_DEBUG:=bfs_core_test # name of main test executable
BFS_TEST_OBJ_DEBUG:=bfs_core_test.test.debug.o # name of test executable file with main()
BFS_EX_NONENCLAVE_MODE:=bfs_server_ne # name of main executable
BFS_EX_OBJ_NONENCLAVE_MODE:=bfs_server_main.exec.nonenclave.o # name of executable file with main()
BFS_LIB_NONENCLAVE_MODE:=libbfs_core_nonenclave.a # nonenclave version of lib
BFS_SO_ENCLAVE_MODE:=libbfs_core_enclave.so # enclave version of lib
BFS_SO_ENCLAVE_MODE_SIGNED=libbfs_core_enclave.signed.so # signed enclave version of lib
BFS_TEST_NONENCLAVE_MODE:=bfs_core_test_ne
BFS_TEST_OBJ_NONENCLAVE_MODE:=bfs_core_test.test.nonenclave.o
BFS_TEST_SO_ENCLAVE_MODE:=libbfs_core_test_enclave.so # enclave version of lib
BFS_TEST_SO_ENCLAVE_MODE_SIGNED=libbfs_core_test_enclave.signed.so # signed enclave version of lib

# Specify source files for each build mode
common_c_files := ext4_balloc.c ext4_bcache.c ext4_bitmap.c ext4_blockdev.c ext4_block_group.c ext4.c ext4_crc32.c ext4_debug.c ext4_dir.c ext4_dir_idx.c ext4_extent.c ext4_fs.c ext4_hash.c ext4_ialloc.c ext4_inode.c ext4_journal.c ext4_mbr.c ext4_mkfs.c ext4_super.c ext4_trans.c ext4_xattr.c
lib_debug_cpp_files := bfs_server.cpp bfs_core.cpp bfs_server_ecalls.cpp bfs_server_ocalls.cpp bfs_acl.cpp bfs_usr.cpp bfs_fs_layer.cpp bfs_core_ext4_helpers.cpp
lib_debug_cpp_objects := $(lib_debug_cpp_files:.cpp=.debug.o)
lib_debug_c_objects := $(common_c_files:.c=.c.debug.o)
debug_dep := Makefile.debug.dep
lib_nonenclave_cpp_files := bfs_server.cpp bfs_server_ocalls.cpp
lib_nonenclave_cpp_objects := $(lib_nonenclave_cpp_files:.cpp=.nonenclave.o)
lib_nonenclave_c_objects := $(common_c_files:.c=.c.nonenclave.o)
so_enclave_cpp_files := bfs_core.cpp bfs_server_ecalls.cpp bfs_acl.cpp bfs_usr.cpp bfs_fs_layer.cpp bfs_core_ext4_helpers.cpp
so_enclave_cpp_objects := $(so_enclave_cpp_files:.cpp=.enclave.o)
so_enclave_c_objects := $(common_c_files:.c=.c.enclave.o)
test_so_enclave_cpp_files := bfs_core.cpp bfs_server_ecalls.cpp bfs_acl.cpp bfs_usr.cpp bfs_fs_layer.cpp bfs_core_test_ecalls.cpp bfs_core_ext4_helpers.cpp
test_so_enclave_cpp_objects := $(test_so_enclave_cpp_files:.cpp=.test.enclave.o)
test_so_enclave_c_objects := $(common_c_files:.c=.c.test.enclave.o)
# lib_enclave_cpp_files := bfs_core_test_ecalls.cpp
# lib_enclave_cpp_objects := $(lib_enclave_cpp_files:.cpp=.enclave.o)

# Subsystem specific lib dependencies
debug_common_link_flags += -Wl,--start-group -lbfs_core_debug -lbfs_comm_debug -lbfs_util_debug -lbfs_blk_debug -lbfs_dev_debug -lgcrypt -lpthread -Wl,--end-group
nonenclave_common_link_flags += -Wl,--start-group -lbfs_core_nonenclave -lbfs_comm_nonenclave -lbfs_util_nonenclave -lbfs_blk_nonenclave -lbfs_dev_nonenclave -lgcrypt -lpthread -l$(Urts_Library_Name) -Wl,--end-group
enclave_common_link_flags += -Wl,--start-group -lbfs_comm_enclave -lbfs_util_enclave -lbfs_blk_enclave -lbfs_dev_enclave -lsgx_tstdc -lsgx_tcxx -l$(Crypto_Library_Name) -l$(Trts_Library_Name) -lsgx_pthread -Wl,--end-group	

enclave_mode_bridge_deps += $(BIN_DIR)/bfs_enclave_t.o
enclave_mode_test_bridge_deps += $(BIN_DIR)/bfs_enclave_core_test_t.o
nonenclave_mode_bridge_deps += $(BIN_DIR)/bfs_enclave_u.o
nonenclave_mode_test_bridge_deps += $(BIN_DIR)/bfs_enclave_core_test_u.o

# Shared recipes for each subsystem
include $(BUILD_DIR)/main.mk
