##
## Rules for building a program target
##
## The following variables must be passed when calling this file:
##
##   BASE_DIR         - base directory of the build system
##   REP_DIR          - source repository of the program
##   PRG_REL_DIR      - directory of the program relative to 'src/'
##   REPOSITORIES     - repositories providing libs and headers
##   INSTALL_DIR      - final install location
##   VERBOSE          - build verboseness modifier
##   VERBOSE_DIR      - verboseness modifier for changing directories
##   VERBOSE_MK       - verboseness of make calls
##   LIB_CACHE_DIR    - library build cache location
##

#
# Prevent target.mk rules to be executed as default rule
#
all:

#
# Tell rust to make an object file instead of anything else
#
CC_RUSTC_OPT += --emit obj

#
# Include common utility functions
#
include $(BASE_DIR)/mk/util.inc

#
# Include target build instructions
#
PRG_DIR := $(REP_DIR)/src/$(PRG_REL_DIR)
include $(PRG_DIR)/target.mk

#
# Include lib-import description files
#
include $(foreach LIB,$(LIBS),$(call select_from_repositories,lib/import/import-$(LIB).mk))

#
# Include specifics for platforms, kernel APIs, etc.
#
include $(SPEC_FILES)

vpath % $(PRG_DIR)

include $(BASE_DIR)/mk/global.mk

#
# Assemble linker options for static and dynamic linkage
#
LD_TEXT_ADDR ?= 0x01000000
ifneq ($(LD_TEXT_ADDR),)
CXX_LINK_OPT += -Wl,-Ttext=$(LD_TEXT_ADDR)
endif

#
# Supply machine-dependent arguments to the linker
#
CXX_LINK_OPT += $(CC_MARCH)


#
# Generic linker script for statically linked binaries
#
LD_SCRIPT_STATIC ?= $(call select_from_repositories,src/ld/genode.ld)

include $(BASE_DIR)/mk/generic.mk
include $(BASE_DIR)/mk/base-libs.mk

ifeq ($(INSTALL_DIR),)
all: message $(TARGET)
else
all: message $(INSTALL_DIR)/$(TARGET)
endif
	@true # prevent nothing-to-be-done message

.PHONY: message
message:
	$(MSG_PRG)$(PRG_REL_DIR)/$(TARGET)

#
# Enforce unconditional call of gnatmake rule when compiling Ada sources
#
# Citation from texinfo manual for make:
#
# If a rule has no prerequisites or commands, and the target of the rule
# is a nonexistent file, then `make' imagines this target to have been
# updated whenever its rule is run.  This implies that all targets
# depending on this one will always have their commands run.
#
FORCE:
$(SRC_ADA:.adb=.o): FORCE

#
# The 'sort' is needed to ensure the same link order regardless
# of the find order, which uses to vary among different systems.
#
SHARED_LIBS := $(foreach l,$(DEPS:.lib=),$(LIB_CACHE_DIR)/$l/$l.lib.so)
SHARED_LIBS := $(sort $(wildcard $(SHARED_LIBS)))

#
# Use CXX for linking
#
LD_CMD ?= $(CXX)

LD_CMD += $(CXX_LINK_OPT)

ifeq ($(SHARED_LIBS),)
LD_SCRIPTS  := $(LD_SCRIPT_STATIC)
FILTER_DEPS := $(DEPS:.lib=)
else

#
# Add a list of symbols that shall always be added to the dynsym section
#
LD_OPT += --dynamic-list=$(call select_from_repositories,src/ld/genode_dyn.dl)

LD_SCRIPTS  := $(LD_SCRIPT_DYN)
LD_CMD      += -Wl,--dynamic-linker=$(DYNAMIC_LINKER).lib.so \
               -Wl,--eh-frame-hdr

#
# Filter out the base libraries since they will be provided by the LDSO library
#
FILTER_DEPS := $(filter-out $(BASE_LIBS),$(DEPS:.lib=))
SHARED_LIBS += $(LIB_CACHE_DIR)/$(DYNAMIC_LINKER)/$(DYNAMIC_LINKER).lib.so


#
# Link all dynamic executables to the component entry-point library (a
# trampoline for component startup from ldso)
#
FILTER_DEPS += component_entry_point

#
# Build program position independent as well
#
CC_OPT_PIC ?= -fPIC
CC_OPT     += $(CC_OPT_PIC)

endif

#
# LD_SCRIPT_PREFIX is needed as 'addprefix' chokes on prefixes containing
# commas othwerwise. For compatibilty with older tool chains, we use two -Wl
# parameters for both components of the linker command line.
#
LD_SCRIPT_PREFIX  = -Wl,-T -Wl,

#
# LD_SCRIPTS may be a list of linker scripts (e.g., in base-linux). Further,
# we use the default linker script if none was specified - 'addprefix' expands
# to empty string on empty input.
#
LD_CMD += $(addprefix $(LD_SCRIPT_PREFIX), $(LD_SCRIPTS))

STATIC_LIBS := $(foreach l,$(FILTER_DEPS),$(LIB_CACHE_DIR)/$l/$l.lib.a)
STATIC_LIBS := $(sort $(wildcard $(STATIC_LIBS)))

#
# --whole-archive does not work with rlibs
#
RUST_LIBS := $(foreach l,$(FILTER_DEPS),$(LIB_CACHE_DIR)/$l/$l.rlib)
RUST_LIBS :=  $(sort $(wildcard $(RUST_LIBS))) 
SHORT_RUST_LIBS := $(subst $(LIB_CACHE_DIR),$$libs,$(RUST_LIBS))

#
# For hybrid Linux/Genode programs, prevent the linkage Genode's cxx and base
# library because these functionalities are covered by the glibc or by
# 'src/platform/lx_hybrid.cc'.
#
ifeq ($(USE_HOST_LD_SCRIPT),yes)
STATIC_LIBS := $(filter-out $(LIB_CACHE_DIR)/startup/startup.lib.a, $(STATIC_LIBS))
STATIC_LIBS := $(filter-out $(LIB_CACHE_DIR)/base/base.lib.a,       $(STATIC_LIBS))
STATIC_LIBS := $(filter-out $(LIB_CACHE_DIR)/cxx/cxx.lib.a,         $(STATIC_LIBS))
endif

#
# We need the linker option '--whole-archive' to make sure that all library
# constructors marked with '__attribute__((constructor))' end up int the
# binary. When not using this option, the linker goes through all libraries
# to resolve a symbol and, if it finds the symbol, stops searching. This way,
# object files that are never explicitly referenced (such as library
# constructors) would not be visited at all.
#
# Furthermore, the '--whole-archive' option reveals symbol ambiguities, which
# would go undetected if the search stops after the first match.
#
LINK_ITEMS       := $(OBJECTS) $(STATIC_LIBS) $(SHARED_LIBS)
SHORT_LINK_ITEMS := $(subst $(LIB_CACHE_DIR),$$libs,$(LINK_ITEMS))

#
# Trigger the build of host tools
#
$(LINK_ITEMS) $(TARGET): $(HOST_TOOLS)

LD_CMD += -Wl,--whole-archive -Wl,--start-group
LD_CMD += $(SHORT_LINK_ITEMS)
LD_CMD += $(EXT_OBJECTS)
LD_CMD += -Wl,--no-whole-archive
LD_CMD += $(SHORT_RUST_LIBS)
LD_CMD += -Wl,--end-group

#
# Link libgcc to each program
#
LD_LIBGCC ?= $(shell $(CC) $(CC_MARCH) -print-libgcc-file-name)
LD_CMD += $(LD_LIBGCC)

#
# Skip final linking if no objects are involved, i.e. no 'SRC' files are
# specified in the 'target.mk' file. This applies for pseudo 'target.mk'
# files that invoke a 3rd-party build system by providing local rule for
# $(TARGET).
#
ifneq ($(OBJECTS),)
$(TARGET): $(LINK_ITEMS) $(wildcard $(LD_SCRIPTS))
	$(MSG_LINK)$(TARGET)
	$(VERBOSE)libs=$(LIB_CACHE_DIR); $(LD_CMD) -o $@

$(INSTALL_DIR)/$(TARGET): $(TARGET)
	$(VERBOSE)ln -sf $(CURDIR)/$(TARGET) $@
else
$(TARGET):
$(INSTALL_DIR)/$(TARGET): $(TARGET)
endif

clean_prg_objects:
	$(MSG_CLEAN)$(PRG_REL_DIR)
	$(VERBOSE)$(RM) -f $(OBJECTS) $(OBJECTS:.o=.d) $(TARGET)
	$(VERBOSE)$(RM) -f *.d *.i *.ii *.s *.ali

clean: clean_prg_objects
