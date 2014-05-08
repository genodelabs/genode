#
# This file determines dependencies of a library from other libraries
#
# The following variables must be defined by the caller:
#
#   VERBOSE          - controls the make verboseness
#   VERBOSE_DIR      - verboseness of directory change messages
#   VERBOSE_MK       - verboseness of make calls
#   REPOSITORIES     - source code repositories to use
#   BASE_DIR         - base directory of build system repository
#   TARGET_DIR       - target build directory
#   BUILD_BASE_DIR   - build directory with build config
#   LIB_CACHE_DIR    - destination directory for object files
#   LIB_PROGRESS_LOG - library build log file
#   BUILD_LIBS       - list of libraries to build (without .lib.a or .lib.so suffix)
#   INSTALL_DIR      - destination directory for installing shared libraries
#

#
# Include common utility functions
#
include $(BASE_DIR)/mk/util.inc

#
# Generate dependencies only for those libs that are
# not already contained in the library build log
#
include $(LIB_PROGRESS_LOG)
ifneq ($(filter $(LIB),$(LIBS_READY)),)
already_visited:
	@true
else
all: append_lib_to_progress_log
endif

append_lib_to_progress_log:
	@echo "LIBS_READY  += $(LIB)" >> $(LIB_PROGRESS_LOG)

LIB_MK_DIRS = $(foreach REP,$(REPOSITORIES),$(addprefix $(REP)/lib/mk/,$(SPECS)) $(REP)/lib/mk)

#
# Of all possible file locations, use the (first) one that actually exist.
#
LIB_MK = $(firstword $(wildcard $(addsuffix /$(LIB).mk,$(LIB_MK_DIRS))))

#
# Sanity check to detect missing library description file
#
ifeq ($(LIB_MK),)
all: warn_missing_lib_mk
else
all: check_unsatisfied_requirements
endif

warn_missing_lib_mk: generate_lib_rule_for_defect_library
	@$(ECHO) "Library-description file $(DARK_COL)$(LIB).mk$(DEFAULT_COL) is missing"

#
# Determine the repository base directory from the absolute pathname
# of the choosen libname.mk file. We need to specify the override
# command because REP_DIR was first set by prg.mk when building a
# program target. The repository of a library could be a different
# one.
#
# Finally, we strip the trailing slash from the path. The slash was
# used to match the whole directory in 'findstring'.
#
override REP_DIR := $(firstword $(foreach REP,$(REPOSITORIES),$(findstring $(REP)/,$(LIB_MK))))
override REP_DIR := $(REP_DIR:/=)

include $(BASE_DIR)/mk/base-libs.mk
include $(LIB_MK)

ifdef SHARED_LIB
#
# For shared libraries, we have to make sure to build ldso support before
# building a shared library.
#
LIBS += ldso-startup

ifneq ($(LIB),$(DYNAMIC_LINKER))
LIBS += $(DYNAMIC_LINKER)
endif

#
# Ensure that startup_dyn is build for the dynamic programs that depend on a
# shared library. They add it to their dependencies as replacement for the
# static-case startup as soon as they recognize that they are dynamic.
# The current library in contrast filters-out startup_dyn from its
# dependencies before they get merged.
#
DEP_VAR_NAME := DEP_$(LIB).lib.so
else
DEP_VAR_NAME := DEP_$(LIB).lib
endif

#
# Add platform preparation dependency
#
# We make each leaf library depend on a library called 'platform'. This way,
# the 'platform' library becomes a prerequisite of all other libraries. The
# 'platform' library is supposed to take precautions for setting up
# platform-specific build environments, e.g., preparing kernel API headers.
#
ifeq ($(LIBS),)
ifneq ($(LIB),platform)
LIBS += platform
endif
endif

#
# Check if the requirements of the target are satisfied
#
UNSATISFIED_REQUIREMENTS = $(filter-out $(SPECS),$(REQUIRES))
ifneq ($(UNSATISFIED_REQUIREMENTS),)
check_unsatisfied_requirements: warn_unsatisfied_requirements
else
check_unsatisfied_requirements: generate_lib_rule
endif

warn_unsatisfied_requirements: generate_lib_rule_for_defect_library
	@$(ECHO) "Skip library $(LIB) because it requires $(DARK_COL)$(UNSATISFIED_REQUIREMENTS)$(DEFAULT_COL)"

generate_lib_rule_for_defect_library:
	@echo "INVALID_DEPS += $(LIB)" >> $(LIB_DEP_FILE)
	@echo "" >> $(LIB_DEP_FILE)

LIBS_TO_VISIT = $(filter-out $(LIBS_READY),$(LIBS))

generate_lib_rule:
ifneq ($(LIBS),)
	@(echo "$(DEP_VAR_NAME) = $(foreach l,$(LIBS),$l.lib \$$(DEP_$l.lib))"; \
	  echo "") >> $(LIB_DEP_FILE)
endif
	@(echo "$(LIB).lib: $(addsuffix .lib,$(LIBS))"; \
	  echo "	@\$$(MKDIR) -p \$$(LIB_CACHE_DIR)/$(LIB)"; \
	  echo "	\$$(VERBOSE_MK)\$$(MAKE) $(VERBOSE_DIR) -C \$$(LIB_CACHE_DIR)/$(LIB) -f \$$(BASE_DIR)/mk/lib.mk \\"; \
	  echo "	     REP_DIR=$(REP_DIR) \\"; \
	  echo "	     LIB_MK=$(LIB_MK) \\"; \
	  echo "	     LIB=$(LIB) \\"; \
	  echo "	     DEPS=\"\$$($(DEP_VAR_NAME))\" \\"; \
	  echo "	     BUILD_BASE_DIR=$(BUILD_BASE_DIR) \\"; \
	  echo "	     SHELL=$(SHELL) \\"; \
	  echo "	     SHARED_LIBS=\"\$$(SHARED_LIBS)\"\\"; \
	  echo "	     INSTALL_DIR=\$$(INSTALL_DIR)"; \
	  echo "") >> $(LIB_DEP_FILE)
	@for i in $(LIBS_TO_VISIT); do \
	  $(MAKE) $(VERBOSE_DIR) -f $(BASE_DIR)/mk/dep_lib.mk REP_DIR=$(REP_DIR) LIB=$$i; done
ifdef SHARED_LIB
	@(echo "SHARED_LIBS += $(LIB)"; \
	  echo "") >> $(LIB_DEP_FILE)
endif

