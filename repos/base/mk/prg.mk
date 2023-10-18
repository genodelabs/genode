##
## Rules for building a program target
##
## The following variables must be passed when calling this file:
##
##   BASE_DIR         - base directory of the build system
##   REP_DIR          - source repository of the program
##   PRG_REL_DIR      - directory of the program relative to 'src/'
##   REPOSITORIES     - repositories providing libs and headers
##   INSTALL_DIR      - installation directory for stripped executables
##   DEBUG_DIR        - installation directory for unstripped executables
##   VERBOSE          - build verboseness modifier
##   VERBOSE_DIR      - verboseness modifier for changing directories
##   VERBOSE_MK       - verboseness of make calls
##   SHARED_LIBS      - shared-library dependencies of the target
##   ARCHIVES         - archive dependencies of the target
##   LIB_CACHE_DIR    - library build cache location
##

#
# Prevent target.mk rules to be executed as default rule
#
all:

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
ifneq ($(filter linux, $(SPECS)),)
LD_SCRIPT_STATIC ?= $(BASE_DIR)/src/ld/genode.ld \
                    $(call select_from_repositories,src/ld/stack_area.ld)
else
LD_SCRIPT_STATIC ?= $(BASE_DIR)/src/ld/genode.ld
endif

include $(BASE_DIR)/mk/generic.mk
include $(BASE_DIR)/mk/base-libs.mk

all: message $(TARGET)

ifneq ($(INSTALL_DIR),)
ifneq ($(DEBUG_DIR),)
all: message $(INSTALL_DIR)/$(TARGET) $(DEBUG_DIR)/$(TARGET) $(DEBUG_DIR)/$(TARGET).debug
endif
endif

all:
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
# Run binder if Ada sources are included in the build
#
ifneq ($(SRC_ADS)$(SRC_ADB),)

CUSTOM_BINDER_FLAGS ?= -n -we -D768k

OBJECTS += b~$(TARGET).o

ALIS := $(addsuffix .ali, $(basename $(SRC_ADS) $(SRC_ADB)))
ALI_DIRS := $(foreach LIB,$(LIBS),$(call select_from_repositories,lib/ali/$(LIB)))
BINDER_SEARCH_DIRS = $(addprefix -I$(BUILD_BASE_DIR)/var/libcache/, $(LIBS)) $(addprefix -aO, $(ALI_DIRS))

BINDER_SRC := b~$(TARGET).ads b~$(TARGET).adb

$(BINDER_SRC): $(ALIS)
	$(VERBOSE)$(GNATBIND) $(CUSTOM_BINDER_FLAGS) $(BINDER_SEARCH_DIRS) $(INCLUDES) --RTS=$(ADA_RTS) -o $@ $^
endif

#
# Use CXX for linking
#
LD_CMD ?= $(CXX)
LD_CMD += $(CXX_LINK_OPT)

ifeq ($(SHARED_LIBS),)
LD_SCRIPTS := $(LD_SCRIPT_STATIC)
else

#
# Add a list of symbols that shall always be added to the dynsym section
#
LD_OPT += --dynamic-list=$(BASE_DIR)/src/ld/genode_dyn.dl

LD_SCRIPTS := $(LD_SCRIPT_DYN)
LD_CMD     += -Wl,--dynamic-linker=$(DYNAMIC_LINKER).lib.so \
              -Wl,--eh-frame-hdr -Wl,-rpath-link=.

#
# Filter out the base libraries since they will be provided by the LDSO library
#
override ARCHIVES := $(filter-out $(BASE_LIBS:=.lib.a),$(ARCHIVES))
endif

#
# LD_SCRIPT_PREFIX is needed as 'addprefix' chokes on prefixes containing
# commas othwerwise. For compatibilty with older tool chains, we use two -Wl
# parameters for both components of the linker command line.
#
LD_SCRIPT_PREFIX := -Wl,-T -Wl,

#
# LD_SCRIPTS may be a list of linker scripts (e.g., in base-linux). Further,
# we use the default linker script if none was specified - 'addprefix' expands
# to empty string on empty input.
#
LD_CMD += $(addprefix $(LD_SCRIPT_PREFIX), $(LD_SCRIPTS))

STATIC_LIBS := $(foreach l,$(ARCHIVES:.lib.a=),$(LIB_CACHE_DIR)/$l/$l.lib.a)

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
LINK_ITEMS_BRIEF := $(subst $(LIB_CACHE_DIR),$$libs,$(LINK_ITEMS))

#
# Trigger the build of host tools
#
$(LINK_ITEMS) $(TARGET): $(HOST_TOOLS)

#
# Trigger build of additional program specific targets
#
all: $(CUSTOM_TARGET_DEPS)

LD_CMD += -Wl,--whole-archive -Wl,--start-group
LD_CMD += $(LINK_ITEMS_BRIEF)
LD_CMD += $(EXT_OBJECTS)
LD_CMD += -Wl,--no-whole-archive
LD_CMD += -Wl,--end-group

#
# Link libgcc to each program
#
LD_LIBGCC ?= $(shell $(CC) $(CC_MARCH) -print-libgcc-file-name)
LD_CMD += $(LD_LIBGCC)

#
# If available, link XML schema file for checking configurations
#
ifneq ($(CONFIG_XSD),)
all: $(INSTALL_DIR)/$(TARGET).xsd
$(INSTALL_DIR)/$(TARGET).xsd: $(PRG_DIR)/$(CONFIG_XSD)
	$(VERBOSE)ln -sf $< $@
endif

#
# Skip final linking if no objects are involved, i.e. no 'SRC' files are
# specified in the 'target.mk' file. This applies for pseudo 'target.mk'
# files that invoke a 3rd-party build system by providing local rule for
# $(TARGET).
#
ifneq ($(OBJECTS),)
$(TARGET): $(LINK_ITEMS) $(wildcard $(LD_SCRIPTS)) $(LIB_SO_DEPS)
	$(MSG_LINK)$(TARGET)
	$(VERBOSE)libs=$(LIB_CACHE_DIR); $(LD_CMD) -o $@

STRIP_TARGET_CMD ?= $(STRIP) -o $@ $<

$(TARGET).debug: $(TARGET)
	$(VERBOSE)$(OBJCOPY) --only-keep-debug $< $@

$(TARGET).stripped: $(TARGET) $(TARGET).debug
	$(VERBOSE)$(STRIP_TARGET_CMD)
	$(VERBOSE)$(OBJCOPY) --add-gnu-debuglink=$(TARGET).debug $@

$(INSTALL_DIR)/$(TARGET): $(TARGET).stripped
	$(VERBOSE)ln -sf $(CURDIR)/$< $@
ifeq ($(COVERAGE),yes)
	$(VERBOSE)mkdir -p $(INSTALL_DIR)/gcov_data/$(TARGET)
	$(VERBOSE)ln -sf $(CURDIR)/*.gcno $(INSTALL_DIR)/gcov_data/$(TARGET)/
endif

ifneq ($(DEBUG_DIR),)
$(DEBUG_DIR)/$(TARGET): $(TARGET).stripped
	$(VERBOSE)ln -sf $(CURDIR)/$< $@
$(DEBUG_DIR)/$(TARGET).debug: $(TARGET).debug
	$(VERBOSE)ln -sf $(CURDIR)/$< $@
endif

else
$(TARGET):
$(INSTALL_DIR)/$(TARGET): $(TARGET)
$(DEBUG_DIR)/$(TARGET): $(TARGET)
$(DEBUG_DIR)/$(TARGET).debug: $(TARGET)
endif


clean_prg_objects:
	$(MSG_CLEAN)$(PRG_REL_DIR)
	$(VERBOSE)$(RM) -f $(OBJECTS) $(OBJECTS:.o=.d) $(TARGET) $(TARGET).stripped $(TARGET).debug $(BINDER_SRC)
	$(VERBOSE)$(RM) -f *.d *.i *.ii *.s *.ali *.lib.so

clean: clean_prg_objects
