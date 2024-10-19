#
# @file Makefile
# @brief Master Makefile to build all bfs subsystems.
#

ifndef BFS_HOME
$(error BFS_HOME is not defined)
endif

all: bfs bfs_sgx

bfs: bfs_utils bfs_comms bfs_client bfs_device bfs_blk bfs_fs

device: bfs_utils bfs_comms bfs_device # TODO: remove, bfs_device handles this

bfs_utils:
	@make -C $(BFS_HOME)/src/bfs_utils bfs

bfs_comms: bfs_utils
	@make -C $(BFS_HOME)/src/bfs_comms bfs

bfs_client: bfs_utils bfs_comms
	@make -C $(BFS_HOME)/src/bfs_client bfs

bfs_device: bfs_utils bfs_comms
	@make -C $(BFS_HOME)/src/bfs_device bfs

bfs_blk: bfs_utils bfs_comms bfs_device
	@make -C $(BFS_HOME)/src/bfs_blk bfs

bfs_fs: bfs_utils bfs_comms bfs_device bfs_blk
	@make -C $(BFS_HOME)/src/bfs_fs bfs

bfs_sgx: bfs_utils_sgx bfs_comms_sgx bfs_client_sgx bfs_device_sgx bfs_blk_sgx bfs_fs_sgx

bfs_utils_sgx:
	@make -C $(BFS_HOME)/src/bfs_utils bfs_sgx

bfs_comms_sgx: bfs_utils_sgx
	@make -C $(BFS_HOME)/src/bfs_comms all

bfs_client_sgx: bfs_utils_sgx bfs_comms_sgx
	@make -C $(BFS_HOME)/src/bfs_client bfs_sgx

bfs_device_sgx: bfs_utils_sgx bfs_comms_sgx
	@make -C $(BFS_HOME)/src/bfs_device bfs_sgx

bfs_blk_sgx: bfs_utils_sgx bfs_comms_sgx bfs_device_sgx
	@make -C $(BFS_HOME)/src/bfs_blk bfs_sgx

bfs_fs_sgx: bfs_utils_sgx bfs_comms_sgx bfs_device_sgx bfs_blk_sgx
	@make -C $(BFS_HOME)/src/bfs_fs bfs_sgx

clean:
	@rm -f ./*.c ./*.h ./*.o
	@make -C $(BFS_HOME)/src/bfs_utils clean
	@make -C $(BFS_HOME)/src/bfs_comms clean
	@make -C $(BFS_HOME)/src/bfs_client clean
	@make -C $(BFS_HOME)/src/bfs_device clean
	@make -C $(BFS_HOME)/src/bfs_blk clean
	@make -C $(BFS_HOME)/src/bfs_fs clean
