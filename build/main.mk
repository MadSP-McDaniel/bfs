#
# @file Makefile
# @brief Shared recipes for all bfs subsystems. Parameterized so that each
# subsystem Makefile can use them by setting the correct target variables.
#

# Determine which targets need to be built
BFS_DEBUG_TARGETS :=
BFS_ENCLAVE_TARGETS :=
ifneq (,$(BFS_LIB_DEBUG))
BFS_DEBUG_TARGETS += $(BFS_LIB_DEBUG)
endif
ifneq (,$(BFS_EX_DEBUG))
BFS_DEBUG_TARGETS += $(BFS_EX_DEBUG)
endif
ifneq (,$(BFS_TEST_DEBUG))
BFS_DEBUG_TARGETS += $(BFS_TEST_DEBUG)
endif
ifneq (,$(BFS_LIB_NONENCLAVE_MODE))
BFS_ENCLAVE_TARGETS += $(BFS_LIB_NONENCLAVE_MODE)
endif
ifneq (,$(BFS_EX_NONENCLAVE_MODE))
BFS_ENCLAVE_TARGETS += $(BFS_EX_NONENCLAVE_MODE)
endif
ifneq (,$(BFS_TEST_NONENCLAVE_MODE))
BFS_ENCLAVE_TARGETS += $(BFS_TEST_NONENCLAVE_MODE)
endif
ifneq (,$(BFS_LIB_ENCLAVE_MODE))
BFS_ENCLAVE_TARGETS += $(BFS_LIB_ENCLAVE_MODE)
endif
ifneq (,$(BFS_SO_ENCLAVE_MODE))
BFS_ENCLAVE_TARGETS += $(BFS_SO_ENCLAVE_MODE)
endif
ifneq (,$(BFS_SO_ENCLAVE_MODE_SIGNED))
BFS_ENCLAVE_TARGETS += $(BFS_SO_ENCLAVE_MODE_SIGNED)
endif
ifneq (,$(BFS_TEST_SO_ENCLAVE_MODE))
BFS_ENCLAVE_TARGETS += $(BFS_TEST_SO_ENCLAVE_MODE)
endif
ifneq (,$(BFS_TEST_SO_ENCLAVE_MODE_SIGNED))
BFS_ENCLAVE_TARGETS += $(BFS_TEST_SO_ENCLAVE_MODE_SIGNED)
endif

# Main recipes
.PHONY: all clean
all: deps bfs bfs_sgx
deps:
	@mkdir -p $(DEBUG_MODE_BUILD_SUBDIR) $(ENCLAVE_BUILD_SUBDIR) $(NONENCLAVE_BUILD_SUBDIR)
	@mkdir -p $(BIN_DIR)
bfs: deps $(addprefix $(BIN_DIR)/,$(BFS_DEBUG_TARGETS))
bfs_sgx: deps $(addprefix $(BIN_DIR)/,$(BFS_ENCLAVE_TARGETS))
clean:
	@rm -rf $(BIN_DIR) obj

##########   Debug (no-enclave based) targets  ##########
ifneq (,$(filter %.o,$(lib_debug_cpp_objects) $(lib_debug_c_objects)))
$(DEBUG_MODE_BUILD_SUBDIR)/%.debug.o: %.cpp %.h
	$(CXX) $(debug_common_cpp_flags) $< -o $@

$(DEBUG_MODE_BUILD_SUBDIR)/%.c.debug.o: lwext4/src/%.c lwext4/include/%.h
	$(CC) $(debug_common_c_flags) $(extra_c_flags) -c $< -o $@

$(DEBUG_MODE_BUILD_SUBDIR)/%.test.debug.o: %.cpp
	$(CXX) $(debug_common_cpp_flags) $< -o $@

$(DEBUG_MODE_BUILD_SUBDIR)/%.exec.debug.o: %.cpp
	$(CXX) $(debug_common_cpp_flags) $< -o $@
endif

ifneq (,$(BFS_LIB_DEBUG))
$(BIN_DIR)/$(BFS_LIB_DEBUG): $(addprefix $(DEBUG_MODE_BUILD_SUBDIR)/,$(lib_debug_cpp_objects)) $(addprefix $(DEBUG_MODE_BUILD_SUBDIR)/,$(lib_debug_c_objects))
	ar rcs $@ $^
endif

ifneq (,$(BFS_EX_DEBUG))
$(BIN_DIR)/$(BFS_EX_DEBUG): $(BIN_DIR)/$(BFS_LIB_DEBUG) $(DEBUG_MODE_BUILD_SUBDIR)/$(BFS_EX_OBJ_DEBUG)
	$(CXX) $^ -o $@ $(debug_common_link_flags)	
endif

ifneq (,$(BFS_TEST_DEBUG))
$(BIN_DIR)/$(BFS_TEST_DEBUG): $(BIN_DIR)/$(BFS_LIB_DEBUG) $(DEBUG_MODE_BUILD_SUBDIR)/$(BFS_TEST_OBJ_DEBUG)
	$(CXX) $^ -o $@ $(debug_common_link_flags)
endif

##########   Enclave-based targets  ##########
# Build the shared set of un/trusted bridge functions for all other subsystems
$(BIN_DIR)/bfs_enclave_u.c: $(BIN_DIR)/%_u.c: $(ENCLAVE_CONFIG_DIR)/%.edl $(SGX_EDGER8R)
	$(SGX_EDGER8R) --untrusted $< --search-path $(ENCLAVE_CONFIG_DIR)/ --search-path $(SGX_SDK)/include --untrusted-dir $(BIN_DIR)

$(BIN_DIR)/bfs_enclave_u.o: $(BIN_DIR)/bfs_enclave_u.c
	$(CC) $(nonenclave_common_c_flags) $(extra_c_flags) -c $< -o $@

$(BIN_DIR)/bfs_enclave_util_test_u.c: $(BIN_DIR)/%_u.c: $(ENCLAVE_CONFIG_DIR)/%.edl $(SGX_EDGER8R)
	$(SGX_EDGER8R) --untrusted $< --search-path $(ENCLAVE_CONFIG_DIR)/ --search-path $(SGX_SDK)/include --untrusted-dir $(BIN_DIR)

$(BIN_DIR)/bfs_enclave_util_test_u.o: $(BIN_DIR)/bfs_enclave_util_test_u.c
	$(CC) $(nonenclave_common_c_flags) $(extra_c_flags) -c $< -o $@

$(BIN_DIR)/bfs_enclave_core_test_u.c: $(BIN_DIR)/%_u.c: $(ENCLAVE_CONFIG_DIR)/%.edl $(SGX_EDGER8R)
	$(SGX_EDGER8R) --untrusted $< --search-path $(ENCLAVE_CONFIG_DIR)/ --search-path $(SGX_SDK)/include --untrusted-dir $(BIN_DIR)

$(BIN_DIR)/bfs_enclave_core_test_u.o: $(BIN_DIR)/bfs_enclave_core_test_u.c
	$(CC) $(nonenclave_common_c_flags) $(extra_c_flags) -c $< -o $@

$(BIN_DIR)/bfs_enclave_t.c: $(BIN_DIR)/%_t.c: $(ENCLAVE_CONFIG_DIR)/%.edl $(SGX_EDGER8R)
	$(SGX_EDGER8R) --trusted $< --search-path $(ENCLAVE_CONFIG_DIR)/ --search-path $(SGX_SDK)/include --trusted-dir $(BIN_DIR)

$(BIN_DIR)/bfs_enclave_t.o: $(BIN_DIR)/bfs_enclave_t.c
	$(CC) $(enclave_common_c_flags) $(extra_c_flags) -c $< -o $@

$(BIN_DIR)/bfs_enclave_util_test_t.c: $(BIN_DIR)/%_t.c: $(ENCLAVE_CONFIG_DIR)/%.edl $(SGX_EDGER8R)
	$(SGX_EDGER8R) --trusted $< --search-path $(ENCLAVE_CONFIG_DIR)/ --search-path $(SGX_SDK)/include --trusted-dir $(BIN_DIR)

$(BIN_DIR)/bfs_enclave_util_test_t.o: $(BIN_DIR)/bfs_enclave_util_test_t.c
	$(CC) $(enclave_common_c_flags) $(extra_c_flags) -c $< -o $@

$(BIN_DIR)/bfs_enclave_core_test_t.c: $(BIN_DIR)/%_t.c: $(ENCLAVE_CONFIG_DIR)/%.edl $(SGX_EDGER8R)
	$(SGX_EDGER8R) --trusted $< --search-path $(ENCLAVE_CONFIG_DIR)/ --search-path $(SGX_SDK)/include --trusted-dir $(BIN_DIR)

$(BIN_DIR)/bfs_enclave_core_test_t.o: $(BIN_DIR)/bfs_enclave_core_test_t.c
	$(CC) $(enclave_common_c_flags) $(extra_c_flags) -c $< -o $@


# Build nonenclave-mode targets
ifneq (,$(filter %.o,$(lib_nonenclave_cpp_objects) $(lib_nonenclave_c_objects)))
$(NONENCLAVE_BUILD_SUBDIR)/%.nonenclave.o: %.cpp %.h $(nonenclave_mode_bridge_deps)
	$(CXX) $(nonenclave_common_cpp_flags) $< -o $@

$(NONENCLAVE_BUILD_SUBDIR)/%.c.nonenclave.o: lwext4/src/%.c lwext4/include/%.h
	$(CC) $(nonenclave_common_c_flags) $(extra_c_flags) -c $< -o $@

$(NONENCLAVE_BUILD_SUBDIR)/%.test.nonenclave.o: %.cpp $(nonenclave_mode_test_bridge_deps)
	$(CXX) $(nonenclave_common_cpp_flags) $< -o $@

$(NONENCLAVE_BUILD_SUBDIR)/%.exec.nonenclave.o: %.cpp $(nonenclave_mode_bridge_deps)
	$(CXX) $(nonenclave_common_cpp_flags) $< -o $@
endif

ifneq (,$(BFS_LIB_NONENCLAVE_MODE))
$(BIN_DIR)/$(BFS_LIB_NONENCLAVE_MODE): $(addprefix $(NONENCLAVE_BUILD_SUBDIR)/,$(lib_nonenclave_cpp_objects)) $(addprefix $(NONENCLAVE_BUILD_SUBDIR)/,$(lib_nonenclave_c_objects))
	ar rcs $@ $^
endif

ifneq (,$(BFS_EX_NONENCLAVE_MODE))
$(BIN_DIR)/$(BFS_EX_NONENCLAVE_MODE): $(nonenclave_mode_bridge_deps) $(BIN_DIR)/$(BFS_LIB_NONENCLAVE_MODE) $(NONENCLAVE_BUILD_SUBDIR)/$(BFS_EX_OBJ_NONENCLAVE_MODE)
	$(CXX) $^ -o $@ $(nonenclave_common_link_flags)
endif

ifneq (,$(BFS_TEST_NONENCLAVE_MODE))
$(BIN_DIR)/$(BFS_TEST_NONENCLAVE_MODE): $(nonenclave_mode_test_bridge_deps) $(BIN_DIR)/$(BFS_LIB_NONENCLAVE_MODE) $(NONENCLAVE_BUILD_SUBDIR)/$(BFS_TEST_OBJ_NONENCLAVE_MODE)
	$(CXX) $^ -o $@ $(nonenclave_common_link_flags)
endif

# Build enclave-mode targets (static libraries or shared objects)
ifneq (,$(filter %.o,$(lib_enclave_cpp_objects) $(so_enclave_cpp_objects) $(test_so_enclave_cpp_objects) $(so_enclave_c_objects) $(test_so_enclave_c_objects)))
$(ENCLAVE_BUILD_SUBDIR)/%.enclave.o: %.cpp %.h $(enclave_mode_bridge_deps)
	$(CXX) $(enclave_common_cpp_flags) $< -o $@

$(ENCLAVE_BUILD_SUBDIR)/%.c.enclave.o: lwext4/src/%.c lwext4/include/%.h $(enclave_mode_bridge_deps)
	$(CC) $(enclave_common_c_flags) $(extra_c_flags) -c $< -o $@

$(ENCLAVE_BUILD_SUBDIR)/%.test.enclave.o: %.cpp $(enclave_mode_test_bridge_deps)
	$(CXX) $(enclave_common_cpp_flags) $< -o $@

$(ENCLAVE_BUILD_SUBDIR)/%.c.test.enclave.o: lwext4/src/%.c $(enclave_mode_test_bridge_deps)
	$(CC) $(enclave_common_c_flags) $(extra_c_flags) -c $< -o $@

$(ENCLAVE_BUILD_SUBDIR)/%.exec.enclave.o: %.cpp $(enclave_mode_bridge_deps)
	$(CXX) $(enclave_common_cpp_flags) $< -o $@
endif

ifneq (,$(BFS_LIB_ENCLAVE_MODE))
$(BIN_DIR)/$(BFS_LIB_ENCLAVE_MODE): $(addprefix $(ENCLAVE_BUILD_SUBDIR)/,$(lib_enclave_cpp_objects))
	ar rcs $@ $^
endif

ifneq (,$(BFS_SO_ENCLAVE_MODE))
$(BIN_DIR)/$(BFS_SO_ENCLAVE_MODE): $(enclave_mode_bridge_deps) $(addprefix $(ENCLAVE_BUILD_SUBDIR)/,$(so_enclave_cpp_objects)) $(addprefix $(ENCLAVE_BUILD_SUBDIR)/,$(so_enclave_c_objects))
	$(CXX) $^ -o $@ $(enclave_common_link_flags)
endif

ifneq (,$(BFS_TEST_SO_ENCLAVE_MODE))
$(BIN_DIR)/$(BFS_TEST_SO_ENCLAVE_MODE): $(enclave_mode_test_bridge_deps) $(addprefix $(ENCLAVE_BUILD_SUBDIR)/,$(test_so_enclave_cpp_objects)) $(addprefix $(ENCLAVE_BUILD_SUBDIR)/,$(test_so_enclave_c_objects))
	$(CXX) $^ -o $@ $(enclave_common_link_flags)
endif

ifneq (,$(BFS_SO_ENCLAVE_MODE_SIGNED))
$(BIN_DIR)/$(BFS_SO_ENCLAVE_MODE_SIGNED): $(BIN_DIR)/$(BFS_SO_ENCLAVE_MODE)
	$(SGX_ENCLAVE_SIGNER) sign -key $(ENCLAVE_CONFIG_DIR)/enclave_private.pem -enclave $(BIN_DIR)/$(BFS_SO_ENCLAVE_MODE) -out $@ -config $(ENCLAVE_CONFIG_DIR)/enclave.config.xml
endif

ifneq (,$(BFS_TEST_SO_ENCLAVE_MODE_SIGNED))
$(BIN_DIR)/$(BFS_TEST_SO_ENCLAVE_MODE_SIGNED): $(BIN_DIR)/$(BFS_TEST_SO_ENCLAVE_MODE)
	$(SGX_ENCLAVE_SIGNER) sign -key $(ENCLAVE_CONFIG_DIR)/enclave_private.pem -enclave $(BIN_DIR)/$(BFS_TEST_SO_ENCLAVE_MODE) -out $@ -config $(ENCLAVE_CONFIG_DIR)/enclave.config.xml
endif

ifneq (,$(BFS_EX_ENCLAVE_MODE))
$(BIN_DIR)/$(BFS_EX_ENCLAVE_MODE): $(enclave_mode_bridge_deps) $(BIN_DIR)/$(BFS_SO_ENCLAVE_MODE_SIGNED) $(ENCLAVE_BUILD_SUBDIR)/$(BFS_EX_OBJ_ENCLAVE_MODE)
	$(CXX) $^ -o $@ $(enclave_common_link_flags)
endif

ifneq (,$(BFS_TEST_ENCLAVE_MODE))
$(BIN_DIR)/$(BFS_TEST_ENCLAVE_MODE): $(enclave_mode_test_bridge_deps) $(BIN_DIR)/$(BFS_SO_ENCLAVE_MODE_SIGNED) $(ENCLAVE_BUILD_SUBDIR)/$(BFS_TEST_OBJ_ENCLAVE_MODE)
	$(CXX) $^ -o $@ $(enclave_common_link_flags)
endif
