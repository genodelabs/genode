#
# Determine dependencies of a library from other libraries
#
# The following variables must be defined by the caller:
#
#   LIB          - library name
#   REPOSITORIES - repository directories to search for the library
#

ACCUMULATE_MISSING_PORTS = 1

include $(BASE_DIR)/mk/util.inc

all:
	@true

LIB_MK := $(firstword $(wildcard $(addsuffix /$(LIB).mk,\
             $(foreach REP,$(REPOSITORIES),\
                           $(addprefix $(REP)/lib/mk/spec/,$(SPECS))\
                           $(REP)/lib/mk))))
#
# Determine the repository base directory from the absolute pathname
# of the chosen libname.mk file. We need to specify the override
# command because REP_DIR was first set by prg.mk when building a
# program target. The repository of a library could be a different
# one.
#
# Finally, we strip the trailing slash from the path. The slash was
# used to match the whole directory in 'findstring'.
#
override REP_DIR := $(firstword $(foreach REP,$(REPOSITORIES),$(findstring $(REP)/,$(LIB_MK))))
override REP_DIR := $(REP_DIR:/=)

include $(LIB_MK)

include $(BASE_DIR)/mk/dep.inc

ifneq ($(UNSATISFIED_REQUIREMENTS),)
WARNING_SKIP := "Skip library $(LIB) because it requires '$(UNSATISFIED_REQUIREMENTS)'"
endif

SYMBOLS := $(call _symbol_file,$(LIB))

#
# Sanity check to detect missing library-description file
#
ifeq ($(sort $(LIB_MK) $(SYMBOLS)),)
WARNING_SKIP := "Library-description file '$(LIB).mk' is missing"
endif

ifneq ($(SYMBOLS),)
SHARED_LIB := yes
endif

ifdef SHARED_LIB
BUILD_ARTIFACTS ?= $(LIB).lib.so
endif

ifdef SHARED_LIB
LIBS += ldso_so_support
endif

#
# Hide archive dependencies of shared libraries from users of the shared
# library. Library users examine the 'DEP_A_<lib>' variable to determine
# transitive dependencies. For shared libraries, this variable remains
# undefined.
#
ifdef SHARED_LIB
DEP_A_VAR_NAME  := PRIVATE_DEP_A_$(LIB)
DEP_SO_VAR_NAME := PRIVATE_DEP_SO_$(LIB)
else
DEP_A_VAR_NAME  := DEP_A_$(LIB)
DEP_SO_VAR_NAME := DEP_SO_$(LIB)
endif

#
# Trigger the rule generation for the ABI of this library
#
ifdef SHARED_LIB
ifneq ($(SYMBOLS),)
generate: generate_abi_dep.$(LIB)
endif
endif

ifeq ($(filter $(LIBS_READY),$(LIB)),)
all: $(if $(WARNING_SKIP),generate_skip,generate)
endif

log_progress:
	@echo "LIBS_READY += $(LIB)" >> $(LIB_PROGRESS_LOG)

generate_skip:
	@echo $(WARNING_SKIP)
	@(echo "INVALID_DEPS += $(LIB)"; \
	  echo "$(LIB).lib.a $(LIB).lib.so:"; \
	  echo "") >> $(LIB_DEP_FILE)

generate: $(call deps_for_libs,$(LIBS))
ifdef SHARED_LIB
ifeq ($(SYMBOLS),)  # if no symbols exist, the shared object is the ABI
	@(echo "SO_NAME($(LIB)) := $(LIB).lib.so"; \
	  echo "$(LIB).abi.so: $(LIB).lib.so"; \
	  echo "$(LIB).lib.a:"; \
	  echo "	@true"; ) >> $(LIB_DEP_FILE)
endif
	@(echo "$(LIB).lib.so: check_ports $(if $(SYMBOLS),$(LIB).abi.so) $(addsuffix .lib.a,$(LIBS)) $(addsuffix .abi.so,$(LIBS))"; \
	  echo "	\$$(VERBOSE)\$$(call _prepare_lib_step,\$$@,$(LIB),$(BUILD_ARTIFACTS))"; \
	  echo "	\$$(VERBOSE_MK)\$$(MAKE) \$$(VERBOSE_DIR) -C \$$(LIB_CACHE_DIR)/$(LIB) -f \$$(BASE_DIR)/mk/so.mk \\"; \
	  echo "	     REP_DIR=$(REP_DIR) \\"; \
	  echo "	     LIB_MK=$(LIB_MK) \\"; \
	  echo "	     SYMBOLS=$(SYMBOLS) \\"; \
	  echo "	     LIB=$(LIB) \\"; \
	  echo "	     ARCHIVES=\"\$$(sort \$$($(DEP_A_VAR_NAME)))\" \\"; \
	  echo "	     SHARED_LIBS=\"\$$(sort \$$($(DEP_SO_VAR_NAME)))\" \\"; \
	  echo "	     BUILD_BASE_DIR=$(BUILD_BASE_DIR) \\"; \
	  echo "	     SHELL=$(SHELL) \\"; \
	  echo "	     INSTALL_DIR=\$$(INSTALL_DIR) \\"; \
	  echo "	     DEBUG_DIR=\$$(DEBUG_DIR)"; \
	  echo "") >> $(LIB_DEP_FILE)
else # not SHARED_LIB
	@(echo "$(LIB).lib.a: check_ports $(addsuffix .lib.a,$(LIBS)) $(addsuffix .abi.so,$(LIBS))"; \
	  echo "	\$$(VERBOSE)\$$(call _prepare_lib_step,\$$@,$(LIB),$(BUILD_ARTIFACTS))"; \
	  echo "	\$$(VERBOSE_MK)\$$(MAKE) \$$(VERBOSE_DIR) -C \$$(LIB_CACHE_DIR)/$(LIB) -f \$$(BASE_DIR)/mk/a.mk \\"; \
	  echo "	     REP_DIR=$(REP_DIR) \\"; \
	  echo "	     LIB_MK=$(LIB_MK) \\"; \
	  echo "	     LIB=$(LIB) \\"; \
	  echo "	     ARCHIVES=\"\$$(sort \$$($(DEP_A_VAR_NAME)))\" \\"; \
	  echo "	     SHARED_LIBS=\"\$$(sort \$$($(DEP_SO_VAR_NAME)))\" \\"; \
	  echo "	     BUILD_BASE_DIR=$(BUILD_BASE_DIR) \\"; \
	  echo "	     SHELL=$(SHELL) \\"; \
	  echo "	     INSTALL_DIR=\$$(INSTALL_DIR) \\"; \
	  echo "	     DEBUG_DIR=\$$(DEBUG_DIR)"; \
	  echo "$(LIB).lib.so $(LIB).abi.so:"; \
	  echo "	@true"; \
	  echo "ARCHIVE_NAME($(LIB)) := $(LIB).lib.a"; \
	  echo "") >> $(LIB_DEP_FILE)
endif

