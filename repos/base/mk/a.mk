##
## Rules for building a static library archive (.lib.a)
##
## The following variables must be passed when calling this file:
##
##   LIB              - library name
##   BASE_DIR         - base directory of the build system
##   VERBOSE          - build verboseness modifier
##   VERBOSE_DIR      - verboseness modifier for changing directories
##   VERBOSE_MK       - verboseness of make calls
##   BUILD_BASE_DIR   - base of build directory tree
##   LIB_CACHE_DIR    - library build cache location
##   INSTALL_DIR      - installation directory for stripped shared objects
##   DEBUG_DIR        - installation directory for unstripped shared objects
##   SHARED_LIBS      - shared-library dependencies of the library
##   ARCHIVES         - archive dependencies of the library
##   REP_DIR          - repository where the library resides
##   CONTRIB_DIR      - location of ported 3rd-party source codes
##

LIB_A := $(addsuffix .lib.a,$(LIB))

#
# Prevent <libname>.mk rules to be executed as default rule
#
all:
	@true # prevent nothing-to-be-done message

#
# Include common utility functions
#
include $(BASE_DIR)/mk/util.inc

#
# Include specifics, for example platform, kernel-api etc.
#
include $(SPEC_FILES)

ORIG_INC_DIR := $(INC_DIR)

#
# Include library build instructions
#
# We set the 'called_from_lib_mk' variable to allow the library description file
# to respond to the build pass.
#
called_from_lib_mk = yes
include $(LIB_MK)

#
# Include lib-import descriptions of all used libraries and the target library
#
include $(foreach LIB,$(LIBS),$(call select_from_repositories,lib/import/import-$(LIB).mk))

#
# Sanity check for INC_DIR overrides
#
ifneq ($(filter-out $(INC_DIR),$(ORIG_INC_DIR)),)
all: error_inc_dir_override
endif

error_inc_dir_override:
	@$(ECHO) "Error: INC_DIR overridden instead of appended" ; false

#
# Include global definitions
#
include $(BASE_DIR)/mk/global.mk
include $(BASE_DIR)/mk/generic.mk

#
# Trigger the creation of the <libname>.lib.a
#
LIB_TAG := $(addsuffix .lib.tag,$(LIB))
all: $(LIB_TAG)

#
# Trigger the build of host tools
#
# We make '$(LIB_TAG)' depend on the host tools to support building host tools
# from pseudo libraries with no actual source code. In this case '$(OBJECTS)'
# is empty.
#
$(LIB_TAG) $(OBJECTS): $(HOST_TOOLS)

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

$(LIB_TAG): $(CUSTOM_TARGET_DEPS) $(LIB_A)
	@touch $@
