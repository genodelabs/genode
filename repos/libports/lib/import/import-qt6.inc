QT_TOOLS_DIR = /usr/local/genode/tool/23.05/qt6

ifeq ($(filter-out $(SPECS),arm),)
QT_PLATFORM = genode-arm-g++
else ifeq ($(filter-out $(SPECS),arm_64),)
QT_PLATFORM = genode-aarch64-g++
else ifeq ($(filter-out $(SPECS),x86_32),)
QT_PLATFORM = genode-x86_32-g++
else ifeq ($(filter-out $(SPECS),x86_64),)
QT_PLATFORM = genode-x86_64-g++
else
$(error Error: unsupported platform)
endif

ifeq ($(CONTRIB_DIR),)
QT_DIR          = $(call select_from_repositories,src/lib/qt6)
QT_API_DIR      = $(call select_from_repositories,mkspecs)/..
abi_symbol_path = $(wildcard $(call select_from_repositories,lib/symbols/$1))
else
QT_PORT_DIR     := $(call select_from_ports,qt6)
QT_DIR           = $(QT_PORT_DIR)/src/lib/qt6
QT_API_DIR       = $(QT_DIR)/genode/api
abi_symbol_path  = $(wildcard $(addprefix $(QT_API_DIR)/lib/symbols/,$1))
endif

ifeq ($(VERBOSE),)
MAKE_VERBOSE="1"
else
QT6_OUTPUT_FILTER = > /dev/null
endif

#
# Genode libraries to be linked to Qt applications and libraries
#

QT6_COMPONENT_LIB_SO   ?= qt6_component.lib.so
QT6_GENODE_LIBS_COMMON += libc.lib.so libm.lib.so stdcxx.lib.so
QT6_GENODE_LIBS_APP    += $(QT6_GENODE_LIBS_COMMON) $(QT6_COMPONENT_LIB_SO)
QT6_GENODE_LIBS_SHLIB  += $(QT6_GENODE_LIBS_COMMON)

#
# common 'build_dependencies' rules
#

build_dependencies:
	$(VERBOSE)mkdir -p $@

build_dependencies/include: build_dependencies
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)ln -snf $(QT_API_DIR)/include/* $@/

build_dependencies/lib: build_dependencies
	$(VERBOSE)mkdir -p $@

build_dependencies/lib/%.lib.so: build_dependencies/lib
	$(VERBOSE)ln -sf $(BUILD_BASE_DIR)/var/libcache/$*/$*.abi.so $@

build_dependencies/lib/%.lib.a: build_dependencies/lib
	$(VERBOSE)ln -sf $(BUILD_BASE_DIR)/var/libcache/$*/$*.lib.a $@

#
# rules for generating .lib.so files from port symbol files
#

lib_name = $(notdir $(1:.lib.so=))
abi_name = $(notdir $(1:.abi.so=))
abi_path = build_dependencies/abi/$(call abi_name,$1)/$(call abi_name,$1).abi.so
so_path  = build_dependencies/lib/$(call lib_name,$1).lib.so

BUILD_DEPENDENCIES_QT6_SO_FILES  := $(foreach L,$(QT6_PORT_LIBS),$(call so_path,$L))
BUILD_DEPENDENCIES_QT6_ABI_FILES := $(foreach L,$(QT6_PORT_LIBS),$(call abi_path,$L))

$(BUILD_DEPENDENCIES_QT6_ABI_FILES):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)$(MAKE) -C $(dir $@) -f $(BASE_DIR)/mk/abi.mk \
	        SYMBOLS=$(call abi_symbol_path,$(call abi_name,$@)) \
	        LIB=$(call abi_name,$@) \
	        BUILD_BASE_DIR=$(BUILD_BASE_DIR) \
	        SHELL=$(SHELL)

$(BUILD_DEPENDENCIES_QT6_SO_FILES): $(BUILD_DEPENDENCIES_QT6_ABI_FILES)

.PHONY: qt6_so_files

qt6_so_files: $(BUILD_DEPENDENCIES_QT6_SO_FILES)
	$(VERBOSE)for l in $(QT6_PORT_LIBS); do \
	   ln -sf ../abi/$$l/$$l.abi.so build_dependencies/lib/$$l.lib.so; done
