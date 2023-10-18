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
##   INSTALL_DIR      - installation directory for stripped shared objects
##   DEBUG_DIR        - installation directory for unstripped shared objects
##   SHARED_LIBS      - shared-library dependencies of the library
##   ARCHIVES         - archive dependencies of the library
##   REP_DIR          - repository where the library resides
##   CONTRIB_DIR      - location of ported 3rd-party source codes
##

include $(BASE_DIR)/mk/base-libs.mk

#
# Prevent <libname>.mk rules to be executed as default rule
#
all:

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

ifneq ($(SYMBOLS),)
SHARED_LIB := yes
endif

#
# If a symbol list is provided, we create an ABI stub named '<lib>.abi.so'
#
# The ABI-stub library does not contain any code or data but only the symbol
# information of the binary interface (ABI) of the shared library.
#
# The ABI stub is linked by the users of the library (executables or shared
# objects) instead of the real library. This effectively decouples the library
# users from the concrete library instance but binds them merely to the
# library's binary interface. Note that the ABI stub is not used at runtime at
# all. At runtime, the real library that implements the ABI is loaded by the
# dynamic linker.
#
# The symbol information are incorporated into the ABI stub via an assembly
# file named '<lib>.symbols.s' that is generated from the library's symbol
# list. We create a symbolic link from the symbol file to the local directory.
# By using '.symbols' as file extension, the pattern rule '%.symbols.s:
# %.symbols' defined in 'generic.mk' is automatically applied for creating the
# assembly file from the symbols file.
#
# The '.PRECIOUS' special target prevents make to remove the intermediate
# assembler file. Otherwise make would spill the build log with messages
# like "rm libc.symbols.s".
#
ifneq ($(SYMBOLS),)
ABI_SO := $(addsuffix .abi.so,$(LIB))
ABI_SONAME := $(addsuffix .lib.so,$(LIB))

$(LIB).symbols:
	$(VERBOSE)ln -sf $(SYMBOLS) $@

.PRECIOUS: $(LIB).symbols.s
endif

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
# Print message for the currently built library
#
all: message

message:
	$(MSG_LIB)$(LIB)

include $(BASE_DIR)/mk/generic.mk

#
# Name of <libname>.lib.a or <libname>.lib.so file to create
#
# Skip the creation and installation of an .so file if there are no
# ingredients. This is the case if the library is present as ABI only.
#
ifdef SHARED_LIB
 ifneq ($(sort $(OBJECTS) $(LIBS)),)
  LIB_SO         := $(addsuffix .lib.so,$(LIB))
  INSTALL_SO     := $(INSTALL_DIR)/$(LIB_SO)
  DEBUG_SO       := $(DEBUG_DIR)/$(LIB_SO)
  LIB_SO_DEBUG   := $(LIB_SO).debug
  DEBUG_SO_DEBUG := $(DEBUG_SO).debug
 endif
else
LIB_A := $(addsuffix .lib.a,$(LIB))
endif

#
# Whenever an ABI is defined for a shared library, we check the consistency
# between both by invoking the 'check_abi' tool via the 'LIB_CHECKED'
# dependency.
#
ifneq ($(LIB_SO),)
 ifneq ($(ABI_SO),)
  LIB_CHECKED := $(addsuffix .lib.checked,$(LIB))
 endif
endif

#
# Trigger the creation of the <libname>.lib.a or <libname>.lib.so file
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
# Trigger build of additional library specific targets
#
$(LIB_TAG): $(CUSTOM_TARGET_DEPS)

$(LIB_TAG): $(LIB_A) $(LIB_SO) $(LIB_CHECKED) $(ABI_SO) $(INSTALL_SO) $(DEBUG_SO) $(DEBUG_SO_DEBUG)
	@touch $@

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
# Link ldso-support library to each shared library to provide local hook
# functions for constructors and ARM
#
ifdef SHARED_LIB
override ARCHIVES += ldso_so_support.lib.a
endif

#
# Don't link base libraries against shared libraries except for ld.lib.so
#
ifneq ($(LIB_IS_DYNAMIC_LINKER),yes)
override ARCHIVES := $(filter-out $(BASE_LIBS:=.lib.a),$(ARCHIVES))
endif

#
# The 'sort' is needed to ensure the same link order regardless
# of the find order, which uses to vary among different systems.
#
STATIC_LIBS       := $(sort $(foreach l,$(ARCHIVES:.lib.a=),$(LIB_CACHE_DIR)/$l/$l.lib.a))
STATIC_LIBS_BRIEF := $(subst $(LIB_CACHE_DIR),$$libs,$(STATIC_LIBS))

#
# Rule to build the <libname>.lib.so file
#
# When linking the shared library, we have to link all shared sub libraries
# (LIB_SO_DEPS) to the library to store the library-dependency information in
# the generated shared object.
#

#
# Default entry point of shared libraries
#
ENTRY_POINT ?= 0x0

$(LIB_SO): $(SHARED_LIBS)

$(LIB_SO): $(STATIC_LIBS) $(OBJECTS) $(wildcard $(LD_SCRIPT_SO)) $(LIB_SO_DEPS)
	$(MSG_MERGE)$(LIB_SO)
	$(VERBOSE)libs=$(LIB_CACHE_DIR); $(LD) -o $(LIB_SO) -soname=$(LIB_SO) -shared --eh-frame-hdr \
	                $(LD_OPT) -T $(LD_SCRIPT_SO) --entry=$(ENTRY_POINT) \
	                --whole-archive --start-group \
	                $(SHARED_LIBS) $(STATIC_LIBS_BRIEF) $(OBJECTS) \
	                --end-group --no-whole-archive \
	                $(LIBGCC)

$(ABI_SO): $(LIB).symbols.o
	$(MSG_MERGE)$(ABI_SO)
	$(VERBOSE)$(LD) -o $(ABI_SO) -soname=$(ABI_SONAME) -shared --eh-frame-hdr $(LD_OPT) \
	                -T $(LD_SCRIPT_SO) \
	                --whole-archive --start-group \
	                $(LIB_SO_DEPS) $< \
	                --end-group --no-whole-archive

$(LIB_CHECKED): $(LIB_SO) $(SYMBOLS)
	$(VERBOSE)$(BASE_DIR)/../../tool/check_abi $(LIB_SO) $(SYMBOLS) && touch $@

$(LIB_SO_DEBUG): $(LIB_SO)
	$(VERBOSE)$(OBJCOPY) --only-keep-debug $< $@

$(LIB_SO).stripped: $(LIB_SO) $(LIB_SO_DEBUG)
	$(VERBOSE)$(STRIP) -o $@ $<
	$(VERBOSE)$(OBJCOPY) --add-gnu-debuglink=$(LIB_SO_DEBUG) $@

$(INSTALL_SO): $(LIB_SO).stripped
	$(VERBOSE)ln -sf $(CURDIR)/$< $@

$(DEBUG_SO): $(LIB_SO).stripped
	$(VERBOSE)ln -sf $(CURDIR)/$< $@

$(DEBUG_SO_DEBUG): $(LIB_SO_DEBUG)
	$(VERBOSE)ln -sf $(CURDIR)/$< $@
