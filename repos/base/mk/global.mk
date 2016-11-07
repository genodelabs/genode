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
CUSTOM_CC      ?= $(CROSS_DEV_PREFIX)gcc
CUSTOM_CXX     ?= $(CROSS_DEV_PREFIX)g++
CUSTOM_CXX_LIB ?= $(CUSTOM_CXX)
CUSTOM_LD      ?= $(CROSS_DEV_PREFIX)ld
CUSTOM_AS      ?= $(CROSS_DEV_PREFIX)as
CUSTOM_AR      ?= $(CROSS_DEV_PREFIX)ar
CUSTOM_NM      ?= $(CROSS_DEV_PREFIX)nm
CUSTOM_OBJCOPY ?= $(CROSS_DEV_PREFIX)objcopy
CUSTOM_RANLIB  ?= $(CROSS_DEV_PREFIX)ranlib
CUSTOM_STRIP   ?= $(CROSS_DEV_PREFIX)strip
CUSTOM_HOST_CC ?= gcc

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
CC      = $(CUSTOM_CC)
CXX     = $(CUSTOM_CXX)
LD      = $(CUSTOM_LD)
AS      = $(CUSTOM_AS)
AR      = $(CUSTOM_AR)
NM      = $(CUSTOM_NM)
OBJCOPY = $(CUSTOM_OBJCOPY)
RANLIB  = $(CUSTOM_RANLIB)
STRIP   = $(CUSTOM_STRIP)
HOST_CC = $(CUSTOM_HOST_CC)

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
# Default optimization and warning levels
#
CC_OLEVEL ?= -O2
CC_WARN   ?= -Wall

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
# Predefine C and C++ specific compiler options with their common values
#
CC_CXX_OPT += $(CC_OPT)
CC_C_OPT   += $(CC_OPT)
CC_ADA_OPT += $(CC_OLEVEL) $(CC_WARN)

#
# Use the correct linker
#
CC_RUSTC_OPT += -C linker=$(LD)

#
# Include dependencies
#
CC_RUSTC_OPT += $(foreach lib,$(LIBS),-L$(LIB_CACHE_DIR)/$(lib))

#
# Enable C++11 by default
#
CC_CXX_OPT_STD ?= -std=gnu++11
CC_CXX_OPT     += $(CC_CXX_OPT_STD)

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

INSTALL_DIR ?=

VERBOSE     ?= @
VERBOSE_DIR ?= --no-print-directory

MSG_LINK     = @$(ECHO) "    LINK     "
MSG_COMP     = @$(ECHO) "    COMPILE  "
MSG_BUILD    = @$(ECHO) "    BUILD    "
MSG_RENAME   = @$(ECHO) "    RENAME   "
MSG_MERGE    = @$(ECHO) "    MERGE    "
MSG_CONVERT  = @$(ECHO) "    CONVERT  "
MSG_CONFIG   = @$(ECHO) "    CONFIG   "
MSG_CLEAN    = @$(ECHO) "  CLEAN "
MSG_ASSEM    = @$(ECHO) "    ASSEMBLE "
MSG_INST     = @$(ECHO) "    INSTALL  "
MSG_PRG      = @$(ECHO) "$(BRIGHT_COL)  Program $(DEFAULT_COL)"
MSG_LIB      = @$(ECHO) "$(DARK_COL)  Library $(DEFAULT_COL)"

