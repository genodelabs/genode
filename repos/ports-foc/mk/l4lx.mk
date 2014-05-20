TARGET              = vmlinux
VERBOSE_LX_MK      ?= 0
REQUIRES           += foc
INC_DIR            += $(REP_DIR)/include
LIBS                = l4lx l4sys
GENODE_LIBS        := base base-common startup syscall cxx l4lx l4sys config

GENODE_LIBS        := $(foreach l,$(GENODE_LIBS),$(BUILD_BASE_DIR)/var/libcache/$l/$l.lib.a)
GENODE_LIBS_SORTED  = $(sort $(wildcard $(GENODE_LIBS)))
GENODE_LIBS_SORTED += $(shell $(CC) $(CC_MARCH) -print-libgcc-file-name)

L4LX_CONTRIB_DIR := $(call select_from_ports,$(LX_TARGET))/src
L4LX_BUILD        = $(BUILD_BASE_DIR)/$(LX_TARGET)
L4LX_BINARY       = $(L4LX_BUILD)/$(TARGET)
L4LX_SYMLINK      = $(BUILD_BASE_DIR)/bin/$(LX_TARGET)
L4LX_CONFIG       = $(L4LX_BUILD)/.config

$(TARGET): $(L4LX_BINARY)

$(L4LX_BINARY): $(L4LX_CONFIG)
	$(VERBOSE_MK)$(MAKE) $(VERBOSE_DIR) \
	               -C $(L4LX_CONTRIB_DIR)/$(LX_TARGET) \
	               O=$(L4LX_BUILD) \
	               CROSS_COMPILE="$(CROSS_DEV_PREFIX)" \
	               CC="$(CC)" \
	               KBUILD_VERBOSE=$(VERBOSE_LX_MK) \
	               V=$(VERBOSE_LX_MK) \
	               GENODE_INCLUDES="$(addprefix -I,$(INC_DIR))" \
	               GENODE_LIBS="$(GENODE_LIBS_SORTED)" \
	               L4ARCH="$(L4LX_L4ARCH)" || false
	$(VERBOSE)ln -sf $@ $(L4LX_SYMLINK)

$(L4LX_CONFIG): $(SRC_L4LX_CONFIG)
	$(VERBOSE)sed -e "s/CONFIG_L4_OBJ_TREE.*/CONFIG_L4_OBJ_TREE=\"$(subst /,\/,$(L4_BUILD_DIR))\"/" $< > $@

clean:
	$(VERBOSE)rm -rf $(L4LX_BUILD)

.PHONY: $(L4LX_BINARY)
