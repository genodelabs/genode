##
## Rules for building a shared object (.lib.so)
##
## The expected variables correspond to those documented in a.mk, with
## the following addition:
##
##   SYMBOLS - path of symbols file
##

LIB_SO := $(addsuffix .lib.so,$(LIB))

include $(BASE_DIR)/mk/base-libs.mk

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
# Link libgcc to shared libraries
#
# For static libraries, libgcc is not needed because it will be linked
# against the final target.
#
LIBGCC = $(shell $(CC) $(CC_MARCH) -print-libgcc-file-name)

#
# Name of <libname>.lib.a or <libname>.lib.so file to create
#
# Skip the creation and installation of an .so file if there are no
# ingredients. This is the case if the library is present as ABI only.
#
ifneq ($(sort $(OBJECTS) $(LIBS)),)
ABI_SO         := $(wildcard $(addsuffix .abi.so,$(LIB)))
INSTALL_SO     := $(INSTALL_DIR)/$(LIB_SO)
DEBUG_SO       := $(DEBUG_DIR)/$(LIB_SO)
LIB_SO_DEBUG   := $(LIB_SO).debug
DEBUG_SO_DEBUG := $(DEBUG_SO).debug
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
# Trigger the creation of the <libname>.lib.so file
#
LIB_TAG := $(addsuffix .lib.tag,$(LIB))
all: $(LIB_TAG)

$(LIB_TAG):
	@touch $@

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

#
# Don't link shared object it BUILD_ARTIFACTS are declared as empty (ld.lib.so)
#
BUILD_ARTIFACTS ?= $(LIB).lib.so

ifneq ($(BUILD_ARTIFACTS),)
$(LIB_TAG): $(LIB_SO) $(LIB_CHECKED) $(INSTALL_SO) $(DEBUG_SO) $(DEBUG_SO_DEBUG)
endif

#
# Link ldso-support library to each shared library to provide local hook
# functions for constructors and ARM
#
override ARCHIVES += ldso_so_support.lib.a

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

ENTRY_POINT ?= 0x0

$(LIB_SO): $(SHARED_LIBS)

$(LIB_SO): $(STATIC_LIBS) $(OBJECTS) $(wildcard $(LD_SCRIPT_SO))
	$(MSG_MERGE)$(LIB_SO)
	$(VERBOSE)libs=$(LIB_CACHE_DIR); $(LD) -o $(LIB_SO) -soname=$(LIB_SO) -shared --eh-frame-hdr \
	                $(LD_OPT) -T $(LD_SCRIPT_SO) --entry=$(ENTRY_POINT) \
	                --whole-archive --start-group \
	                $(SHARED_LIBS) $(STATIC_LIBS_BRIEF) $(OBJECTS) \
	                --end-group --no-whole-archive \
	                $(LIBGCC)

$(ABI_SO): $(LD_SCRIPT_SO)

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
