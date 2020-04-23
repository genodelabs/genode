#
# Global build configuration variables
#

#
# Read user-provided tools configuration
#
-include $(call select_from_repositories,etc/tools.conf)
-include $(BUILD_BASE_DIR)/etc/tools.conf

#
# Set undefined CUSTOM_ tools to their default values
#
CUSTOM_CC       ?= $(CROSS_DEV_PREFIX)gcc
CUSTOM_CXX      ?= $(CROSS_DEV_PREFIX)g++
CUSTOM_CPP      ?= $(CROSS_DEV_PREFIX)cpp
CUSTOM_CXX_LIB  ?= $(CUSTOM_CXX)
CUSTOM_LD       ?= $(CROSS_DEV_PREFIX)ld
CUSTOM_AS       ?= $(CROSS_DEV_PREFIX)as
CUSTOM_AR       ?= $(CROSS_DEV_PREFIX)ar
CUSTOM_NM       ?= $(CROSS_DEV_PREFIX)nm
CUSTOM_OBJCOPY  ?= $(CROSS_DEV_PREFIX)objcopy
CUSTOM_RANLIB   ?= $(CROSS_DEV_PREFIX)ranlib
CUSTOM_STRIP    ?= $(CROSS_DEV_PREFIX)strip
CUSTOM_GNATBIND ?= $(CROSS_DEV_PREFIX)gnatbind
CUSTOM_HOST_CC  ?= $(HOST_DEV_PREFIX)gcc
CUSTOM_HOST_CXX ?= $(HOST_DEV_PREFIX)g++
CUSTOM_ADA_CC   ?= $(CUSTOM_CC)
CUSTOM_ALI2DEP  ?= $(CROSS_DEV_PREFIX)ali2dep

#
# GNU utilities
#
# Non-Linux operating systems may have to install 'findutils'
# to get the GNU versions of xargs and find.
#
TAC       ?= tac
GNU_FIND  ?= find
GNU_XARGS ?= xargs
ECHO      ?= echo -e

#
# Build tools
#
CC       = $(CUSTOM_CC)
CXX      = $(CUSTOM_CXX)
CPP      = $(CUSTOM_CPP)
LD       = $(CUSTOM_LD)
AS       = $(CUSTOM_AS)
AR       = $(CUSTOM_AR)
NM       = $(CUSTOM_NM)
OBJCOPY  = $(CUSTOM_OBJCOPY)
RANLIB   = $(CUSTOM_RANLIB)
STRIP    = $(CUSTOM_STRIP)
GNATBIND = $(CUSTOM_GNATBIND)
HOST_CC  = $(CUSTOM_HOST_CC)
ADA_CC   = $(CUSTOM_ADA_CC)
ALI2DEP  = $(CUSTOM_ALI2DEP)

#
# Compiler and Linker options
#

#
# Options for automatically generating dependency files
#
# We specify the target for the generated dependency file explicitly via
# the -MT option. Unfortunately, this option is handled differently by
# different gcc versions. Older versions used to always append the object
# file to the target. However, gcc-4.3.2 takes the -MT argument literally.
# So we have to specify both the .o file and the .d file. On older gcc
# versions, this results in the .o file to appear twice in the target
# but that is no problem.
#
CC_OPT_DEP = -MMD -MP -MT '$@ $(@:.o=.d)'

#
# Always compile with '-ffunction-sections' to enable the use of the
# linker option '-gc-sections'
#
CC_OPT += -ffunction-sections

#
# Prevent the compiler from optimizations related to strict aliasing
#
CC_OPT += -fno-strict-aliasing

#
# Do not compile/link with standard includes and standard libraries per
# default.
#
ifneq ($(STDINC),yes)
CC_OPT_NOSTDINC := -nostdinc
endif
ifneq ($(STDLIB),yes)
LD_OPT_NOSTDLIB := -nostdlib -Wl,-nostdlib
endif

#
# Add coverage options
#
# The directory for the coverage data (generated at runtime) is derived from
# the current build directory and is going to look like
#
# '/genodelabs/bin/test-log_gcov/2018-11-13/gcov_data/test-log_gcov'
#
# to match the path where the gcov note files (generated at build time) are
# stored in the depot package of the test program.
#
# If depot packages are not used, the path is going to look like
#
# '/gcov_data/test-log_gcov'
#
ifeq ($(COVERAGE),yes)
ifneq ($(findstring /depot/,$(CURDIR)),)
PROFILE_DIR = $(shell echo $(CURDIR) | \
                      sed -e 's/^.*\/depot\//\//' \
                          -e 's/\.build\/.*//')/gcov_data/$(TARGET)
else
PROFILE_DIR = /gcov_data/$(TARGET)
endif
CC_OPT += -fprofile-arcs -ftest-coverage -fprofile-dir=$(PROFILE_DIR)
endif

#
# Enable the undefined behavior sanitizer if requested
#
ifeq ($(SANITIZE_UNDEFINED),yes)
CC_OPT += -fsanitize=undefined
endif

#
# Default optimization and warning levels
#
CC_OLEVEL ?= -O2
CC_WARN   ?= -Wall

#
# XXX fix the warnings and remove this option
#
CC_WARN += -Wno-error=implicit-fallthrough

#
# Additional warnings for C++
#
CC_CXX_WARN_STRICT ?= -Wextra -Weffc++ -Werror -Wsuggest-override
CC_CXX_WARN        ?= $(CC_WARN) $(CC_CXX_WARN_STRICT)

#
# Aggregate compiler options that are common for C and C++
#
CC_OPT += $(CC_OPT_NOSTDINC) -g $(CC_MARCH) $(CC_OLEVEL) $(CC_OPT_DEP) $(CC_WARN)

#
# Incorporate source-file-specific compiler options
#
# The make variable $* refers to the currently processed compilation
# unit when 'CC_OPT' gets implicitly expanded by the rules '%.o: %.c'
# and '%.o: %.cc' of 'generic.mk'.
#
# We substitute '.' characters by '_' to allow source-file-specific
# compiler options for files with more than one dot in their name.
#
CC_OPT += $(CC_OPT_$(subst .,_,$*))

#
# Build program position independent as well
#
CC_OPT_PIC ?= -fPIC
CC_OPT     += $(CC_OPT_PIC)

#
# Predefine C and C++ specific compiler options with their common values
#
CC_CXX_OPT += $(CC_OPT) $(CC_CXX_WARN)
CC_C_OPT   += $(CC_OPT)
CC_ADA_OPT += $(filter-out -fno-builtin-cos -fno-builtin-sin -fno-builtin-cosf -fno-builtin-sinf ,$(CC_OPT)) -fexceptions

#
# Enable C++11 by default
#
# We substitute '.' characters by '_' to allow a source-file-specific
# C++ standard option for files with more than one dot in their name.
#
CC_CXX_OPT_STD ?= -std=gnu++17
CC_CXX_OPT += $(lastword $(CC_CXX_OPT_STD) $(CC_CXX_OPT_STD_$(subst .,_,$*)))

#
# Linker options
#
# Use '-gc-sections' by default but allow a platform to disable this feature by
# defining 'LD_GC_SECTIONS' empty. This is needed for older tool chains (gcc
# version 4.11 and binutils version 2.16), which happen to produce broken
# code when '-gc-sections' is enabled. Also, set max-page-size to 4KiB to
# prevent the linker from aligning the text segment to any built-in default
# (e.g., 4MiB on x86_64 or 64KiB on ARM). Otherwise, the padding bytes are
# wasted at the beginning of the final binary.
#
LD_OPT_GC_SECTIONS ?= -gc-sections
LD_OPT_ALIGN_SANE   = -z max-page-size=0x1000
LD_OPT_PREFIX      := -Wl,
LD_OPT             += $(LD_MARCH) $(LD_OPT_GC_SECTIONS) $(LD_OPT_ALIGN_SANE)
CXX_LINK_OPT       += $(addprefix $(LD_OPT_PREFIX),$(LD_OPT))
CXX_LINK_OPT       += $(LD_OPT_NOSTDLIB)

#
# Linker script for dynamically linked programs
#
LD_SCRIPT_DYN = $(BASE_DIR)/src/ld/genode_dyn.ld

#
# Linker script for shared libraries
#
LD_SCRIPT_SO ?= $(BASE_DIR)/src/ld/genode_rel.ld

#
# Assembler options
#
AS_OPT += $(AS_MARCH)

#
# Control sequences for color terminals
#
# To disable colored output, define these variable empty in your
# build-local 'etc/tools.conf' file.
#
BRIGHT_COL  ?= \033[01;33m
DARK_COL    ?= \033[00;33m
DEFAULT_COL ?= \033[0m

ALL_INC_DIR := .
ALL_INC_DIR += $(INC_DIR)
ALL_INC_DIR += $(foreach DIR,$(REP_INC_DIR), $(foreach REP,$(REPOSITORIES),$(REP)/$(DIR)))
ALL_INC_DIR += $(foreach REP,$(REPOSITORIES),$(REP)/include)
ALL_INC_DIR += $(LIBGCC_INC_DIR)
ALL_INC_DIR += $(HOST_INC_DIR)

VERBOSE     ?= @
VERBOSE_DIR ?= --no-print-directory

MSG_LINK     ?= @$(ECHO) "    LINK     "
MSG_COMP     ?= @$(ECHO) "    COMPILE  "
MSG_BUILD    ?= @$(ECHO) "    BUILD    "
MSG_RENAME   ?= @$(ECHO) "    RENAME   "
MSG_MERGE    ?= @$(ECHO) "    MERGE    "
MSG_CONVERT  ?= @$(ECHO) "    CONVERT  "
MSG_CONFIG   ?= @$(ECHO) "    CONFIG   "
MSG_CLEAN    ?= @$(ECHO) "  CLEAN "
MSG_ASSEM    ?= @$(ECHO) "    ASSEMBLE "
MSG_INST     ?= @$(ECHO) "    INSTALL  "
MSG_PRG      ?= @$(ECHO) "$(BRIGHT_COL)  Program $(DEFAULT_COL)"
MSG_LIB      ?= @$(ECHO) "$(DARK_COL)  Library $(DEFAULT_COL)"

