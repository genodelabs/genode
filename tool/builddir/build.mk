#
# \brief  Front end to the Genode build system
# \author Norman Feske
# \date   2011-08-04
#

#
# The operation of the build system can be tuned using the following variables,
# which can be specified at the command line or supplied via the
# 'etc/build.conf' file.
#
# BASE_DIR       - this directory points to Genode's 'base' repository
# REPOSITORIES   - list of directories incorporared into the build process
# VERBOSE        - variable that controls the verboseness of the build process
#
#                  By default, compiler messages are not displayed. If you are
#                  interested in these messages set this variable to nothing.
#                  ('VERBOSE=')
#
# VERBOSE_MK    - variable that controls the verboseness of make invocations
# VERBOSE_DIR   - variable that controls the verboseness of changing directories
#
#                 Using this variable, you can enable the make messages printed
#                 when changing directories. To enable the messages, set the
#                 variable to nothing.
#
# LIB_CACHE_DIR - location of the library build cache
#
#                 This variable defines the place for the library build cache.
#                 Normally, the libcache is located at 'var/libcache' and
#                 there is no need to change it.
#
# CONTRIB_DIR   - location of ported 3rd-party source codes
# CCACHE        - if set to 'yes', the build system uses the compiler cache
#
# REQUIRED_GCC_VERSION - GCC version required for building Genode
#

##
## Define global configuration variables
##

#
# We initially enforce .SHELLFLAGS flags in case build.mk called recursively
# (from tool/run) as SHELL will be reset to /bin/sh before setting up bash.
#
.SHELLFLAGS := -c

#
# Whenever using the 'run/%' rule and the run tool spawns this Makefile again
# when encountering a 'build' step, the build.conf is included a second time,
# with the content taken from the environment variable. We need to reset the
# 'REPOSITORIES' variable to prevent extending this variable twice.
# (see https://github.com/genodelabs/genode/issues/3731)
#
REPOSITORIES :=

-include etc/build.conf

BUILD_BASE_DIR := $(CURDIR)
DEBUG_DIR      := $(CURDIR)/debug
INSTALL_DIR    := $(CURDIR)/bin

export BASE_DIR         ?= ../base
export REPOSITORIES     ?= $(BASE_DIR:%base=%base-linux) $(BASE_DIR)
export VERBOSE          ?= @
export VERBOSE_DIR      ?= --no-print-directory
export VERBOSE_MK       ?= @
export LIB_CACHE_DIR    ?= $(BUILD_BASE_DIR)/var/libcache
export LIB_PROGRESS_LOG ?= $(BUILD_BASE_DIR)/progress.log
export LIB_DEP_FILE     ?= var/libdeps
export ECHO             ?= echo -e
export CONTRIB_DIR
export BOARD
export KERNEL

#
# Convert user-defined directories to absolute directories
#
# The 'echo' shell command expands '~' characters to the home directory,
# 'realpath' converts relative path names to absolute.
#
REPOSITORIES := $(realpath $(shell echo $(REPOSITORIES)))
BASE_DIR     := $(realpath $(shell echo $(BASE_DIR)))

define nl


endef

ifneq ($(words $(REPOSITORIES)),$(words $(sort $(REPOSITORIES))))
$(error detected duplicates in REPOSITORIES $(foreach p,$(REPOSITORIES),$(nl)    $(p)))
endif

#
# Configure shell program before executing any shell commands. On Ubuntu the
# standard shell is dash, which breaks colored output via its built-in echo
# command.
#
# SHELL is exported into the environment for tools used in rules
# (like tool/run). Unfortunately, sub-make instances will reset SHELL to
# /bin/sh if it is not explicitly provided on the command like (as we do
# below). See GNU Make manual "5.3.2 Choosing the Shell" for further details.
# .SHELLFLAGS is extended by option pipefail to make pipes fail if any pipe
# element fails.
#
export SHELL       := $(shell sh -c "command -v bash")
export .SHELLFLAGS := -o pipefail $(.SHELLFLAGS)

MAKE := $(MAKE) SHELL=$(SHELL)

#
# Discharge variables evaluated by ccache mechanism that may be inherited when
# using the build system in a nested fashion.
#
undefine CUSTOM_CXX
undefine CUSTOM_CC

#
# Fetch SPECS configuration from all source repositories and the build directory
#
SPECS :=
-include $(foreach REP,$(REPOSITORIES),$(wildcard $(REP)/etc/specs.conf))
-include $(BUILD_BASE_DIR)/etc/specs.conf

#
# \deprecated  We include the repository-specific 'specs.conf' once again as the
#              build-dir-local etc/specs.conf (as created by create_builddir)
#              reassigns the 'SPECS' variable instead of appending it.
#              We sort the 'SPECS' to remove duplicates. We should remove this
#              once the old 'create_builddir' arguments are gone.
#
-include $(foreach REP,$(REPOSITORIES),$(wildcard $(REP)/etc/specs.conf))
SPECS := $(sort $(SPECS))

select_from_repositories = $(firstword $(foreach REP,$(REPOSITORIES),$(wildcard $(REP)/$(1))))

#
# Determine the spec files to incorporate into the build configuration from the
# repositories. Always consider the spec files present in BASE_DIR. This is
# needed when the build system is invoked from the package-build tool where the
# repos/base is not present in the list of REPOSITORIES.
#
export SPEC_FILES := \
       $(sort $(foreach SPEC,$(SPECS),$(call select_from_repositories,mk/spec/$(SPEC).mk)) \
              $(wildcard $(foreach SPEC,$(SPECS),$(BASE_DIR)/mk/spec/$(SPEC).mk)))
include $(SPEC_FILES)
export SPECS

include $(BASE_DIR)/mk/global.mk

export LIBGCC_INC_DIR := $(shell dirname `$(CUSTOM_CXX_LIB) -print-libgcc-file-name`)/include

#
# Find out about the target directories to build
#
# Arguments starting with 'run/' and 'lib/' are special. The former triggers
# the execution of a run script. The latter issues the build of a library.
#
DST_DIRS := $(filter-out clean cleanall again run/% lib/%,$(MAKECMDGOALS))

ifeq ($(MAKECMDGOALS),)
DST_DIRS := *
endif

#
# Detect use of obsoleted LIB=<libname> option
#
ifneq ($(LIB),)
$(error the 'LIB=$(LIB)' option is no longer supported, use 'make lib/$(LIB)')
endif

#
# Determine library targets specified as lib/<libname> at the command line
#
LIBS := $(notdir $(filter lib/%,$(MAKECMDGOALS)))

ifeq ($(MAKECMDGOALS),)
ALL_LIB_MK_DIRS  := $(wildcard \
                       $(foreach R,$(REPOSITORIES),\
                          $R/lib/mk $(foreach S,$(SPECS),$R/lib/mk/spec/$S)))
ALL_LIB_MK_FILES := $(wildcard $(addsuffix /*.mk,$(ALL_LIB_MK_DIRS)))
LIBS             := $(sort $(notdir $(ALL_LIB_MK_FILES:.mk=)))
endif

#
# Helper function to check if a needed tool is installed
#
check_tool = $(if $(shell command -v $(1)),,$(error Need to have '$(1)' installed.))

#
# Tool chain version check
#
# Empty DST_DIRS is interpreted as a tool-chain agnostic target, e.g., clean.
#
ifneq ($(DST_DIRS),)
REQUIRED_GCC_VERSION ?= 12.3.0
GCC_VERSION := $(filter $(REQUIRED_GCC_VERSION) ,$(shell $(CUSTOM_CXX) --version))
ifneq ($(GCC_VERSION), $(REQUIRED_GCC_VERSION))
$(error "$(CUSTOM_CXX) version $(REQUIRED_GCC_VERSION) is required")
endif
endif

ifneq ($(STATIC_ANALYZE),)
$(call check_tool,scan-build)

MAKE := scan-build --use-c++=$(CUSTOM_CXX) --use-cc=$(CUSTOM_CC) $(MAKE)
endif

#
# Default rule: build all directories specified as make arguments
#
_all $(DST_DIRS) $(addprefix lib/,$(LIBS)) : gen_deps_and_build_targets
	@true

##
## First stage: generate library dependencies
##

#
# Reset library-build log and library-dependency file
#
# The 'progress' file contains the names of the already processed libraries
# of the current build process. Before considering to process any library,
# the build system checks if the library is already present in the 'progress'
# file and, if yes, skips it.
#
.PHONY: init_progress_log
init_progress_log:
	@echo "#" > $(LIB_PROGRESS_LOG)
	@echo "# Library build progress log - generated by dep_prg.mk and dep_lib.mk" >> $(LIB_PROGRESS_LOG)
	@echo "#" >> $(LIB_PROGRESS_LOG)

.PHONY: init_libdep_file
init_libdep_file: $(dir $(LIB_DEP_FILE))
	@echo "checking library dependencies..."
	@(echo "#"; \
	  echo "# Library dependencies for build '$(DST_DIRS)'"; \
	  echo "#"; \
	  echo ""; \
	  echo "export SPEC_FILES := \\"; \
	  for i in $(SPEC_FILES); do \
	    echo "  $$i \\"; done; \
	  echo ""; \
	  echo "LIB_CACHE_DIR = $(LIB_CACHE_DIR)"; \
	  echo "BASE_DIR      = $(realpath $(BASE_DIR))"; \
	  echo "VERBOSE      ?= $(VERBOSE)"; \
	  echo "VERBOSE_MK   ?= $(VERBOSE_MK)"; \
	  echo "VERBOSE_DIR  ?= $(VERBOSE_DIR)"; \
	  echo "INSTALL_DIR  ?= $(INSTALL_DIR)"; \
	  echo "DEBUG_DIR    ?= $(DEBUG_DIR)"; \
	  echo "SHELL        ?= $(SHELL)"; \
	  echo "MKDIR        ?= mkdir"; \
	  echo ""; \
	  echo "all:"; \
	  echo "	@true # prevent nothing-to-be-done message"; \
	  echo "") > $(LIB_DEP_FILE)

#
# We check if any target.mk files exist in the specified directories within
# any of the repositories listed in the 'REPOSITORIES' variable.
#

$(dir $(LIB_DEP_FILE)):
	@mkdir -p $@

#
# Find all 'target.mk' files located within any of the specified subdirectories
# ('DST_DIRS') and any repository. The 'sort' is used to remove duplicates.
#
TARGETS_TO_VISIT := $(shell find $(wildcard $(REPOSITORIES:=/src)) -false \
                            $(foreach DST,$(DST_DIRS), \
                                      -or -path "*/src/$(DST)/**target.mk" \
                                          -printf " %P "))
TARGETS_TO_VISIT := $(sort $(TARGETS_TO_VISIT))

#
# Perform sanity check for non-existing targets being specified at the command
# line.
#
MISSING_TARGETS := $(strip \
                     $(foreach DST,$(DST_DIRS),\
                       $(if $(filter $(DST)/%,$(TARGETS_TO_VISIT)),,$(DST))))

ifneq ($(MAKECMDGOALS),)
ifneq ($(MISSING_TARGETS),)
init_libdep_file: error_missing_targets
error_missing_targets:
	@for target in $(MISSING_TARGETS); do \
	  echo "Error: target '$$target' does not exist"; done
	@false;
endif
endif

#
# The procedure of collecting library dependencies is realized as a single
# rule to gain maximum performance. If we invoked a rule for each target,
# we would need to spawn one additional shell per target, which would take
# 10-20 percent more time.
#
traverse_dependencies: $(dir $(LIB_DEP_FILE)) init_libdep_file init_progress_log
	$(VERBOSE_MK) \
	for lib in $(LIBS); do \
	    $(MAKE) $(VERBOSE_DIR) -f $(BASE_DIR)/mk/dep_lib.mk \
	            REP_DIR=$$rep LIB=$$lib \
	            BUILD_BASE_DIR=$(BUILD_BASE_DIR) \
	            DARK_COL="$(DARK_COL)" DEFAULT_COL="$(DEFAULT_COL)"; \
	    echo "all: $$lib.lib" >> $(LIB_DEP_FILE); \
	done; \
	for target in $(TARGETS_TO_VISIT); do \
	  for rep in $(REPOSITORIES); do \
	    test -f $$rep/src/$$target || continue; \
	    $(MAKE) $(VERBOSE_DIR) -f $(BASE_DIR)/mk/dep_prg.mk \
	            REP_DIR=$$rep TARGET_MK=$$rep/src/$$target \
	            BUILD_BASE_DIR=$(BUILD_BASE_DIR) \
	            DARK_COL="$(DARK_COL)" DEFAULT_COL="$(DEFAULT_COL)" || result=false; \
	    break; \
	  done; \
	done; $$result;

.PHONY: $(LIB_DEP_FILE)

$(LIB_DEP_FILE): traverse_dependencies


##
## Second stage: build targets based on the result of the first stage
##

$(INSTALL_DIR) $(DEBUG_DIR):
	$(VERBOSE)mkdir -p $@

.PHONY: gen_deps_and_build_targets
gen_deps_and_build_targets: $(INSTALL_DIR) $(DEBUG_DIR) $(LIB_DEP_FILE)
	@(echo ""; \
	  echo "ifneq (\$$(MISSING_PORTS),)"; \
	  echo "check_ports:"; \
	  echo "	@echo \"\""; \
	  echo "	@echo \"Error: Ports not prepared or outdated:\""; \
	  echo "	@echo \"  \$$(sort \$$(MISSING_PORTS))\""; \
	  echo "	@echo \"\""; \
	  echo "	@echo \"You can prepare respectively update them as follows:\""; \
	  echo "	@echo \"  $(GENODE_DIR)/tool/ports/prepare_port \$$(sort \$$(MISSING_PORTS))\""; \
	  echo "	@echo \"\""; \
	  echo "	@false"; \
	  echo "else"; \
	  echo "check_ports:"; \
	  echo "endif"; \
	  echo "") >> $(LIB_DEP_FILE)
	$(VERBOSE_MK)$(MAKE) $(VERBOSE_DIR) -f $(LIB_DEP_FILE) all


.PHONY: again
again: $(INSTALL_DIR) $(DEBUG_DIR)
	$(VERBOSE_MK)$(MAKE) $(VERBOSE_DIR) -f $(LIB_DEP_FILE) all

#
# Read tools configuration to obtain the cross-compiler prefix passed
# to the run script.
#
-include $(call select_from_repositories,etc/tools.conf)


##
## Compiler-cache support
##

#
# To hook the ccache into the build process, the compiler executables are
# shadowed by symlinks named after the compiler but pointing to the ccache
# program. When invoked, the ccache program uses argv0 to query the real
# compiler executable.
#
# If the configured tool-chain path is absolute, we assume that it is not
# already part of the PATH variable. In this (default) case, we supplement the
# tool-chain path to the CCACHE_PATH as evaluated by the ccache program.
#
# Should the tool-chain path not be absolute, the tool-chain executables must
# already be reachable via the regular PATH variable. Otherwise, the build
# would not work without ccache either.
#

ifeq ($(CCACHE),yes)

$(call check_tool,ccache)

ifneq ($(dir $(CUSTOM_CXX)),$(dir $(CUSTOM_CC)))
$(error ccache enabled but the compilers $(CUSTOM_CXX) and $(CUSTOM_CC)\
        reside in different directories)
endif

CCACHE_TOOL_DIR    := $(addsuffix /var/tool/ccache,$(BUILD_BASE_DIR))
CCACHED_CUSTOM_CC  := $(CCACHE_TOOL_DIR)/$(notdir $(CUSTOM_CC))
CCACHED_CUSTOM_CXX := $(CCACHE_TOOL_DIR)/$(notdir $(CUSTOM_CXX))

gen_deps_and_build_targets: $(CCACHED_CUSTOM_CC) $(CCACHED_CUSTOM_CXX)

# create ccache symlinks at var/tool/ccache/
$(CCACHED_CUSTOM_CC) $(CCACHED_CUSTOM_CXX):
	$(VERBOSE_MK)mkdir -p $(dir $@)
	$(VERBOSE_MK)ln -sf `command -v ccache` $@

# supplement tool-chain directory to the search-path variable used by ccache
ifneq ($(filter /%,$(CUSTOM_CXX)),)
export CCACHE_PATH := $(dir $(CUSTOM_CXX)):$(PATH)
endif

# force preprocessor mode
export CCACHE_NODIRECT=1

# override CUSTOM_CC and CUSTOM_CXX to point to the ccache symlinks
export CUSTOM_CC  := $(CCACHED_CUSTOM_CC)
export CUSTOM_CXX := $(CCACHED_CUSTOM_CXX)

RUN_OPT_CCACHE := --ccache

endif


##
## Rules for running automated test cases
##

RUN_OPT ?=

# helper for run/% rule
RUN_SCRIPT = $(call select_from_repositories,run/$*.run)

run/%: $(call select_from_repositories,run/%.run) $(RUN_ENV)
	$(VERBOSE)test -f "$(RUN_SCRIPT)" || (echo "Error: No run script for $*"; exit -1)
	$(VERBOSE)$(GENODE_DIR)/tool/run/run --genode-dir $(GENODE_DIR) \
	                                     --name $* \
	                                     --specs "$(SPECS)" \
	                                     --board "$(BOARD)" \
	                                     --repositories "$(REPOSITORIES)" \
	                                     --cross-dev-prefix "$(CROSS_DEV_PREFIX)" \
	                                     --qemu-args "$(QEMU_OPT)" \
	                                     --make "$(MAKE)" \
	                                     $(RUN_OPT_CCACHE) \
	                                     $(RUN_OPT) \
	                                     --include $(RUN_SCRIPT)

##
## Clean rules
##

#
# For cleaning, visit each directory for that a corresponding target.mk
# file exists in the source tree. For each such directory, we call
# the single_target rule.
#
clean_targets:
	$(VERBOSE_MK)for d in `$(GNU_FIND) -mindepth 1 -type d | $(TAC) | sed "s/^..//"`; do \
		for r in $(REPOSITORIES); do \
			test -f $$r/src/$$d/target.mk && \
				$(MAKE) $(VERBOSE_DIR) clean \
					-C $$d \
					-f $(BASE_DIR)/mk/prg.mk \
					BUILD_BASE_DIR=$(BUILD_BASE_DIR) \
					PRG_REL_DIR=$$d \
					REP_DIR=$$r || \
				true; \
		done; \
	done

clean_libcache:
	$(VERBOSE)rm -rf var/libcache

clean_run:
	$(VERBOSE)rm -rf var/run
	$(VERBOSE)rm -rf config-00-00-00-00-00-00

clean_gen_files:
	$(VERBOSE)rm -f $(LIB_PROGRESS_LOG)
	$(VERBOSE)rm -f $(LIB_DEP_FILE)

clean_install_dir: clean_targets clean_libcache
	$(VERBOSE)(test -d $(INSTALL_DIR) && find $(INSTALL_DIR) -type l -not -readable -delete) || true

clean_debug_dir: clean_targets clean_libcache
	$(VERBOSE)(test -d $(DEBUG_DIR) && find $(DEBUG_DIR) -type l -not -readable -delete) || true

clean_empty_dirs: clean_targets clean_libcache clean_run clean_gen_files clean_install_dir clean_debug_dir
	$(VERBOSE)$(GNU_FIND) . -depth -type d -empty -delete

clean cleanall: clean_empty_dirs

