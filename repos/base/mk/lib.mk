##
## Rules for building a library target
##
## The following variables must be passed when calling this file:
##
##   BASE_DIR         - base directory of the build system
##   REPOSITORIES     - repositories providing libs and headers
##   VERBOSE          - build verboseness modifier
##   VERBOSE_DIR      - verboseness modifier for changing directories
##   VERBOSE_MK       - verboseness of make calls
##   BUILD_BASE_DIR   - base of build directory tree
##   LIB_CACHE_DIR    - library build cache location
##   INSTALL_DIR      - program target build directory
##   DEPS             - library dependencies
##   REP_DIR          - repository where the library resides
##   CONTRIB_DIR      - location of ported 3rd-party source codes
##

include $(BASE_DIR)/mk/base-libs.mk

#
# Prevent <libname>.mk rules to be executed as default rule
#
all:

#
# Make a rlib or dylib instead of object file
#
ifndef SHARED_LIB
  CC_RUSTC_OPT += --crate-type rlib
else
  CC_RUSTC_OPT += --crate-type dylib
endif

#
# Include common utility functions
#
include $(BASE_DIR)/mk/util.inc

#
# Include specifics, for example platform, kernel-api etc.
#
include $(SPEC_FILES)

#
# Include library build instructions
#
# We set the 'called_from_lib_mk' variable to allow the library description file
# to respond to the build pass.
#
BACKUP_INC_DIR := $(INC_DIR)
called_from_lib_mk = yes
include $(LIB_MK)

#
# Sanity check for INC_DIR overrides
#
ifneq ($(filter-out $(INC_DIR),$(BACKUP_INC_DIR)),)
all: error_inc_dir_override
endif

error_inc_dir_override:
	@$(ECHO) "Error: $(LIB_MK) overrides INC_DIR instead of appending" ; false

#
# Include lib-import descriptions of all used libraries and the target library
#
include $(foreach LIB,$(LIBS),$(call select_from_repositories,lib/import/import-$(LIB).mk))

#
# Include global definitions
#
include $(BASE_DIR)/mk/global.mk

#
# Name of <libname>.lib.a or <libname>.lib.so file to create
#
ifdef SHARED_LIB
LIB_SO       := $(addsuffix .lib.so,$(LIB))
INSTALL_SO   := $(INSTALL_DIR)/$(LIB_SO)
LIB_FILENAME := $(LIB_SO)
else ifdef SRC_RS
LIB_RLIB        := $(addsuffix .rlib,$(LIB))
LIB_FILENAME := $(LIB_RLIB)
else
LIB_A        := $(addsuffix .lib.a,$(LIB))
LIB_FILENAME := $(LIB_A)
endif
LIB_TAG := $(addsuffix .lib.tag,$(LIB))

#
# Link libgcc to shared libraries
#
# For static libraries, libgcc is not needed because it will be linked
# against the final target.
#
ifdef SHARED_LIB
LIBGCC = $(shell $(CC) $(CC_MARCH) -print-libgcc-file-name)
endif

#
# Build libraries position-independent
#
# This option is required for building shared objects but also for static
# libraries that are (potentially) linked against shared objects. Hence,
# we build all libraries with '-fPIC'.
#
CC_OPT_PIC ?= -fPIC
CC_OPT += $(CC_OPT_PIC)

#
# Print message for the currently built library
#
all: message

message:
	$(MSG_LIB)$(LIB)

#
# Trigger the creation of the <libname>.lib.a or <libname>.lib.so file
#
all: $(LIB_TAG)

#
# Trigger the build of host tools
#
# We make '$(LIB_TAG)' depend on the host tools to support building host tools
# from pseudo libraries with no actual source code. In this case '$(OBJECTS)'
# is empty.
#
$(LIB_TAG) $(OBJECTS): $(HOST_TOOLS)

$(LIB_TAG): $(LIB_A) $(LIB_SO) $(INSTALL_SO) $(LIB_RLIB)
	@touch $@

include $(BASE_DIR)/mk/generic.mk

#
# Rule to build the <libname>.lib.a file
#
# Use $(OBJECTS) instead of $^ for specifying the list of objects to include
# in the archive because $^ may also contain non-object phony targets, e.g.,
# used by the integration of Qt's meta-object compiler into the Genode
# build system.
#
$(LIB_A): $(OBJECTS)
	$(MSG_MERGE)$(LIB_A)
	$(VERBOSE)$(RM) -f $@
	$(VERBOSE)$(AR) -rcs $@ $(OBJECTS)
#
# Rename from object to rlib
#
$(LIB_RLIB):  $(OBJECTS)
	$(MSG_RENAME)$(LIB_RLIB)
	$(VERBOSE)cp $(OBJECTS) $(LIB_RLIB)


#
# Don't link base libraries against shared libraries except for ld.lib.so
#
ifdef SHARED_LIB
ifneq ($(LIB),$(DYNAMIC_LINKER))
override DEPS := $(filter-out $(BASE_LIBS:=.lib),$(DEPS))
endif
endif

#
# The 'sort' is needed to ensure the same link order regardless
# of the find order, which uses to vary among different systems.
#
STATIC_LIBS := $(foreach l,$(DEPS:.lib=),$(LIB_CACHE_DIR)/$l/$l.lib.a)
STATIC_LIBS := $(sort $(wildcard $(STATIC_LIBS)))
STATIC_LIBS_BRIEF := $(subst $(LIB_CACHE_DIR),$$libs,$(STATIC_LIBS))

#
# Rule to build the <libname>.lib.so file
#
# The 'LIBS' variable may contain static and shared sub libraries. When linking
# the shared library, we have to link all shared sub libraries to the library
# to store the library-dependency information in the library. Because we do not
# know which sub libraries are static or shared prior calling 'build_libs.mk',
# we use an explicit call to the 'lib_so_wildcard' macro to determine the subset
# of libraries that are shared.
#
# The 'ldso-startup/startup.o' object file, which contains the support code for
# constructing static objects must be specified as object file to prevent the
# linker from garbage-collecting it.
#

USED_SHARED_LIBS := $(filter $(DEPS:.lib=),$(SHARED_LIBS))
USED_SO_FILES    := $(foreach s,$(USED_SHARED_LIBS),$(LIB_CACHE_DIR)/$s/$s.lib.so)

#
# Don't link ld libary against shared objects
#
USED_SO_FILES    := $(filter-out %$(DYNAMIC_LINKER).lib.so,$(USED_SO_FILES))

#
# Default entry point of shared libraries
#
ENTRY_POINT      ?= 0x0

$(LIB_SO): $(STATIC_LIBS) $(OBJECTS) $(wildcard $(LD_SCRIPT_SO))
	$(MSG_MERGE)$(LIB_SO)
	$(VERBOSE)libs=$(LIB_CACHE_DIR); $(LD) -o $(LIB_SO) -shared --eh-frame-hdr \
	                $(LD_OPT) \
	                -T $(LD_SCRIPT_SO) \
	                --entry=$(ENTRY_POINT) \
	                --whole-archive \
	                --start-group \
	                $(USED_SO_FILES) $(STATIC_LIBS_BRIEF) $(OBJECTS) \
	                --end-group \
	                --no-whole-archive \
	                $(LIBGCC)

$(INSTALL_SO):
	$(VERBOSE)ln -sf $(CURDIR)/$(LIB_SO) $@

