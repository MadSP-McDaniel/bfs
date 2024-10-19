#
# @file Makefile
# @brief Environment file for enclave-mode code in sgx-based builds.
# Contains common variables that will be used by the _enclave mode_
# recipes for each subsystem.
#

#
# Copyright (C) 2011-2020 Intel Corporation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#


##########################################################
#################### SGX SDK Settings ####################
##########################################################
SGX_SDK ?= /opt/intel/sgxsdk
SGX_MODE ?= SIM
SGX_ARCH ?= x64
BUILD_LVL ?= 0

-include $(SGX_SDK)/buildenv.mk

ifeq ($(shell getconf LONG_BIT), 32)
	SGX_ARCH := x86
else ifeq ($(findstring -m32, $(CXXFLAGS)), -m32)
	SGX_ARCH := x86
endif

ifeq ($(SGX_ARCH), x86)
	SGX_COMMON_FLAGS := -m32
	SGX_LIBRARY_PATH := $(SGX_SDK)/lib
	SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x86/sgx_sign
	SGX_EDGER8R := $(SGX_SDK)/bin/x86/sgx_edger8r
else
	SGX_COMMON_FLAGS := -m64
	SGX_LIBRARY_PATH := $(SGX_SDK)/lib64
	SGX_ENCLAVE_SIGNER := $(SGX_SDK)/bin/x64/sgx_sign
	SGX_EDGER8R := $(SGX_SDK)/bin/x64/sgx_edger8r
endif

ifeq ($(BUILD_LVL), 0)
	ifeq ($(SGX_PRERELEASE), 1)
		$(error Cannot set SGX_DEBUG (BUILD_LVL) and SGX_PRERELEASE at the same time!!)
	endif
endif

ifeq ($(BUILD_LVL), 0)
  SGX_COMMON_FLAGS += -O0 -g
else ifeq ($(BUILD_LVL), 1)
  SGX_COMMON_FLAGS += -O2 -g
else
  SGX_COMMON_FLAGS += -O2
endif

SGX_COMMON_FLAGS += -Wall -Wextra -Winit-self -Wpointer-arith -Wreturn-type \
                    -Waddress -Wsequence-point -Wformat-security \
                    -Wmissing-include-dirs -Wfloat-equal -Wundef -Wshadow \
                    -Wcast-align -Wcast-qual -Wconversion -Wredundant-decls

# Three configuration modes - Debug, prerelease, release
#   Debug - Macro DEBUG enabled.
#   Prerelease - Macro NDEBUG and EDEBUG enabled.
#   Release - Macro NDEBUG enabled.
ifeq ($(BUILD_LVL), 0)
        SGX_COMMON_FLAGS += -DDEBUG -UNDEBUG -UEDEBUG
else ifeq ($(BUILD_LVL), 1)
        SGX_COMMON_FLAGS += -DDEBUG -UNDEBUG -UEDEBUG
else ifeq ($(SGX_PRERELEASE), 1)
        SGX_COMMON_FLAGS += -DNDEBUG -DEDEBUG -UDEBUG
else
        SGX_COMMON_FLAGS += -DNDEBUG -UEDEBUG -UDEBUG
endif

SGX_COMMON_CFLAGS := $(SGX_COMMON_FLAGS)

ifneq ($(SGX_MODE), HW)
	Urts_Library_Name := sgx_urts_sim
else
	Urts_Library_Name := sgx_urts
endif

ifneq ($(SGX_MODE), HW)
	Trts_Library_Name := sgx_trts_sim
	Service_Library_Name := sgx_tservice_sim
else
	Trts_Library_Name := sgx_trts
	Service_Library_Name := sgx_tservice
endif

Crypto_Library_Name := sgx_tcrypto

# ifeq ($(SGX_MODE), HW)
# 	ifeq ($(BUILD_LVL), 1)
# 		Build_Mode = HW_DEBUG
# 	else ifeq ($(SGX_PRERELEASE), 1)
# 		Build_Mode = HW_PRERELEASE
# 	else
# 		Build_Mode = HW_RELEASE
# 	endif
# else
# 	ifeq ($(BUILD_LVL), 1)
# 		Build_Mode = SIM_DEBUG
# 	else ifeq ($(SGX_PRERELEASE), 1)
# 		Build_Mode = SIM_PRERELEASE
# 	else
# 		Build_Mode = SIM_RELEASE
# 	endif
# endif


##########################################################
############### Bfs settings and targets #################
##########################################################

# Set output directory for object files
ENCLAVE_BUILD_SUBDIR:=obj/enclave
ENCLAVE_CONFIG_DIR?=../../config

# Set common search paths
enclave_common_search_paths:=$(addprefix $(BFS_HOME)/src/,bfs_utils bfs_comms bfs_blk bfs_device bfs_fs bfs_client) $(BFS_HOME)/src/bfs_fs/lwext4/include $(BFS_HOME)/src/bfs_fs/lwext4/build_generic/include $(BIN_DIR)
enclave_common_include_paths:=$(addprefix -I,$(enclave_common_search_paths)) -I$(SGX_SDK)/include
enclave_common_link_paths:=-L$(BIN_DIR) -L$(SGX_LIBRARY_PATH)
extra_c_flags:=-Wjump-misses-init -Wstrict-prototypes -Wunsuffixed-float-constants

# Set common flags
enclave_common_c_flags:=$(SGX_COMMON_CFLAGS) -nostdinc -fvisibility=hidden -fpie -fPIC -fstack-protector $(enclave_common_include_paths) -I$(SGX_SDK)/include/libcxx -I$(SGX_SDK)/include -I$(SGX_SDK)/include/tlibc -c -Wall -D__BFS_ENCLAVE_MODE
enclave_common_cpp_flags:=$(enclave_common_c_flags) -nostdinc++ -std=c++11
enclave_common_link_flags:=$(SGX_COMMON_CFLAGS) -Wl,--no-undefined -nostdlib -nodefaultlibs -nostartfiles -L$(SGX_LIBRARY_PATH) \
	-Wl,--whole-archive -l$(Trts_Library_Name) -Wl,--no-whole-archive \
	-Wl,--whole-archive -lsgx_tcmalloc -Wl,--no-whole-archive \
	-Wl,--start-group -lsgx_tstdc -lsgx_tcxx -l$(Crypto_Library_Name) -l$(Service_Library_Name) -lsgx_pthread -Wl,--end-group \
	-Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined \
	-Wl,-pie,-eenclave_entry -Wl,--export-dynamic  \
	-Wl,--defsym,__ImageBase=0 \
	$(enclave_common_link_paths) -Wl,--start-group -l$(Trts_Library_Name) -Wl,--end-group	

# Declare enclave e/o-call dependencies
enclave_mode_bridge_deps :=
enclave_mode_test_bridge_deps :=
