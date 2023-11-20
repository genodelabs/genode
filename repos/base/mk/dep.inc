#
# Common parts of dep_prg.mk, dep_lib.mk, and dep_abi.mk
#
# The target/library description file is already included.
# Hence, the LIBS variable contains the library dependencies.
#

.NOTPARALLEL:

##
# Return path to symbol file for a given library name
#
_symbol_file = $(firstword $(wildcard $(addsuffix /$1,\
                  $(foreach REP,$(REPOSITORIES),$(REP)/lib/symbols))))

include $(LIB_PROGRESS_LOG)

#
# Categorize LIBS into ABIs and actual libraries. Dependencies of libraries
# must be recursively discovered whereas ABIs don't have any dependencies.
#

#
# Pattern rules for generating ABI and library dependencies for var/libdeps
#

generate_abi_dep.%: log_progress
	$(VERBOSE_MK)$(MAKE) $(VERBOSE_DIR) -f $(BASE_DIR)/mk/dep_abi.mk SYMBOLS=$(call _symbol_file,$*)

generate_lib_dep.%: log_progress
	$(VERBOSE_MK)$(MAKE) $(VERBOSE_DIR) -f $(BASE_DIR)/mk/dep_lib.mk REP_DIR=$(REP_DIR) LIB=$*

generate_missing_ports:
	$(if $(DEP_MISSING_PORTS),\
	   @echo "MISSING_PORTS += $(DEP_MISSING_PORTS)" >> $(LIB_DEP_FILE),\
	   @true)

generate_dep_lists:
	@(echo "$(DEP_A_VAR_NAME)  = $(foreach l,$(LIBS),\$${ARCHIVE_NAME($l)} \$$(DEP_A_$l))"; \
	  echo "$(DEP_SO_VAR_NAME) = $(foreach l,$(LIBS),\$${SO_NAME($l)} \$$(DEP_SO_$l))"; \
	  echo "") >> $(LIB_DEP_FILE)

log_progress:

generate: log_progress
generate: generate_missing_ports

_unvisited_libs = $(filter-out $(LIBS_READY),$1)
_abis_to_visit  = $(foreach L,$(call _unvisited_libs,$1),$(if $(call _symbol_file,$L),$L))
_libs_to_visit  = $(filter-out $(call _abis_to_visit,$1),$(call _unvisited_libs,$1))
deps_for_libs   = $(addprefix generate_abi_dep.,$(call _abis_to_visit,$1)) \
                  $(addprefix generate_lib_dep.,$(call _libs_to_visit,$1)) \
                  $(if $1,generate_dep_lists)

UNSATISFIED_REQUIREMENTS = $(filter-out $(SPECS),$(REQUIRES))
