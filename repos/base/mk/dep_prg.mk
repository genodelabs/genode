#
# Determine library dependencies of a program
#

ACCUMULATE_MISSING_PORTS = 1

include $(BASE_DIR)/mk/util.inc

all:
	@true

#
# Include target build instructions to aquire library dependecies
#
PRG_DIR := $(dir $(TARGET_MK))

include $(TARGET_MK)

#
# Check if the requirements of the target are satisfied
#
UNSATISFIED_REQUIREMENTS = $(filter-out $(SPECS),$(REQUIRES))
ifneq ($(UNSATISFIED_REQUIREMENTS),)
WARNING_SKIP := "Skip library $(LIB) because it requires '$(UNSATISFIED_REQUIREMENTS)'"
endif

#
# Add libgcov if coverage is requested
#
ifeq ($(COVERAGE),yes)
LIBS += libgcov
endif

#
# Add libraries for undefined behavior sanitizer if requested
#
ifeq ($(SANITIZE_UNDEFINED),yes)
LIBS += libubsan libsanitizer_common
endif

#
# Include lib-import description files
#
include $(foreach LIB,$(LIBS),$(call select_from_repositories,lib/import/import-$(LIB).mk))

#
# Add globally defined library supplements
#
include $(SPEC_FILES)

#
# Evaluate library dependencies for this target
#
include $(BASE_DIR)/mk/dep.inc

DEP_A_VAR_NAME  := DEP_A_$(TARGET).prg
DEP_SO_VAR_NAME := DEP_SO_$(TARGET).prg

#
# Names of build artifacts to appear in the 'progress.log'
#
BUILD_ARTIFACTS ?= $(TARGET)

#
# Determine location of $(TARGET_MK) within 'src/', remove trailing slash
#
PRG_REL_DIR := $(subst $(REP_DIR)/src/,,$(PRG_DIR))
PRG_REL_DIR := $(PRG_REL_DIR:/=)

ifneq ($(UNSATISFIED_REQUIREMENTS),)
WARNING_SKIP := "Skip target $(PRG_REL_DIR) because it requires '$(UNSATISFIED_REQUIREMENTS)'"
endif

all: $(if $(WARNING_SKIP),generate_skip,generate)

generate_skip:
	@echo $(WARNING_SKIP)

generate: $(call deps_for_libs,$(LIBS))
	@(echo "$(TARGET).prg: check_ports $(addsuffix .lib.a,$(LIBS)) $(addsuffix .abi.so,$(LIBS))"; \
	  echo "	\$$(VERBOSE)\$$(call _prepare_prg_step,$(PRG_REL_DIR)/$(TARGET),$(BUILD_ARTIFACTS))"; \
	  echo "	\$$(VERBOSE_MK)\$$(MAKE) $(VERBOSE_DIR) -C $(PRG_REL_DIR) -f \$$(BASE_DIR)/mk/prg.mk \\"; \
	  echo "	     REP_DIR=$(REP_DIR) \\"; \
	  echo "	     PRG_REL_DIR=$(PRG_REL_DIR) \\"; \
	  echo "	     BUILD_BASE_DIR=$(BUILD_BASE_DIR) \\"; \
	  echo "	     ARCHIVES=\"\$$(sort \$$(DEP_A_$(TARGET).prg))\" \\"; \
	  echo "	     SHARED_LIBS=\"\$$(sort \$$(DEP_SO_$(TARGET).prg))\" \\"; \
	  echo "	     SHELL=$(SHELL) \\"; \
	  echo "	     INSTALL_DIR=\"\$$(INSTALL_DIR)\" \\"; \
	  echo "	     DEBUG_DIR=\"\$$(DEBUG_DIR)\""; \
	  echo "") >> $(LIB_DEP_FILE)
#
# Make 'all' depend on the target, which triggers the building of the target
# and the traversal of the target's library dependencies. But we only do so
# if the target does not depend on any library with unsatisfied build
# requirements. In such a case, the target cannot be linked anyway.
#
	@(echo "ifeq (\$$(filter \$$(DEP_A_$(TARGET).prg:.lib.a=) \$$(DEP_SO_$(TARGET).prg:.lib.so=) $(LIBS),\$$(INVALID_DEPS)),)"; \
	  echo "all: $(TARGET).prg"; \
	  echo "endif") >> $(LIB_DEP_FILE)
#
# Normally, if the target depends on a library, which cannot be built (such
# libraries get recorded in the 'INVALID_DEPS' variable), we skip the target
# altogether. In some cases, however, we want to build all non-invalid
# libraries of a target regardless of whether the final target can be created
# or not. (i.e., for implementing the mechanism for building all libraries,
# see 'base/src/lib/target.mk'). This use case is supported via the
# 'FORCE_BUILD_LIBS' variable. If the 'target.mk' file assigns the value
# 'yes' to this variable, we build all non-invalid libraries regardless of
# the validity of the final target.
#
ifeq ($(FORCE_BUILD_LIBS),yes)
	@(echo ""; \
	  echo "all: \$$(addsuffix .lib,\$$(filter-out \$$(INVALID_DEPS), $(LIBS)))") >> $(LIB_DEP_FILE)
endif
