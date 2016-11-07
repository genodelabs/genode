#
# Create prerequisites for building Genode for seL4
#
# Prior building Genode programs for seL4, the kernel bindings needed are
# symlinked to the build directory.
#

#
# We do this also in the first build stage to ensure that the kernel
# port, if missing, is added to the missing-ports list of this stage.
#
LIBSEL4_DIR := $(call select_from_ports,sel4)/src/kernel/sel4/libsel4
LIBSEL4_AUTO:= $(call select_from_ports,sel4)/src/kernel/sel4/include/plat/pc99

#
# Execute the rules in this file only at the second build stage when we know
# about the complete build settings, e.g., the 'CROSS_DEV_PREFIX'.
#
ifeq ($(called_from_lib_mk),yes)

#
# Make seL4 kernel API headers available to the Genode build system
#
# We have to create symbolic links of the content of seL4's 'include/sel4' and
# 'include_arch/<arch>/sel4' directories into our local build directory.
#

SEL4_ARCH_INCLUDES := simple_types.h types.h constants.h objecttype.h \
                      functions.h syscalls.h invocation.h deprecated.h

ARCH_INCLUDES := objecttype.h types.h constants.h functions.h deprecated.h \
                 pfIPC.h syscalls.h exIPC.h invocation.h simple_types.h

INCLUDES := objecttype.h types.h bootinfo.h bootinfo_types.h errors.h constants.h \
            messages.h sel4.h macros.h simple_types.h types_gen.h syscall.h \
            invocation.h shared_types_gen.h debug_assert.h shared_types.h \
            sel4.h deprecated.h autoconf.h

INCLUDE_SYMLINKS += $(addprefix $(BUILD_BASE_DIR)/include/sel4/,          $(INCLUDES))
INCLUDE_SYMLINKS += $(addprefix $(BUILD_BASE_DIR)/include/sel4/arch/,     $(ARCH_INCLUDES))
INCLUDE_SYMLINKS += $(addprefix $(BUILD_BASE_DIR)/include/sel4/sel4_arch/,$(SEL4_ARCH_INCLUDES))
INCLUDE_SYMLINKS += $(BUILD_BASE_DIR)/include/interfaces/sel4_client.h

all: $(INCLUDE_SYMLINKS)

#
# Plain symlinks to existing headers
#
$(BUILD_BASE_DIR)/include/sel4/sel4_arch/%.h: $(LIBSEL4_DIR)/sel4_arch_include/ia32/sel4/sel4_arch/%.h
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $< $@

$(BUILD_BASE_DIR)/include/sel4/arch/%.h: $(LIBSEL4_DIR)/arch_include/x86/sel4/arch/%.h
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $< $@

$(BUILD_BASE_DIR)/include/sel4/autoconf.h: $(LIBSEL4_AUTO)/autoconf.h
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $< $@

$(BUILD_BASE_DIR)/include/sel4/%.h: $(LIBSEL4_DIR)/include/sel4/%.h
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $< $@

#
# Generated headers
#
$(BUILD_BASE_DIR)/include/sel4/types_gen.h: $(LIBSEL4_DIR)/include/sel4/types_32.bf
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python $(LIBSEL4_DIR)/tools/bitfield_gen.py \
	                 --environment libsel4 "$<" $@

$(BUILD_BASE_DIR)/include/sel4/shared_types_gen.h: $(LIBSEL4_DIR)/include/sel4/shared_types_32.bf
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python $(LIBSEL4_DIR)/tools/bitfield_gen.py \
	                 --environment libsel4 "$<" $@

$(BUILD_BASE_DIR)/include/sel4/syscall.h: $(LIBSEL4_DIR)/include/api/syscall.xml
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python $(LIBSEL4_DIR)/tools/syscall_header_gen.py \
	                 --xml $< --libsel4_header $@

$(BUILD_BASE_DIR)/include/sel4/invocation.h: $(LIBSEL4_DIR)/include/interfaces/sel4.xml
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python $(LIBSEL4_DIR)/tools/invocation_header_gen.py \
	                 --xml $< --libsel4 --dest $@

$(BUILD_BASE_DIR)/include/sel4/sel4_arch/invocation.h: $(LIBSEL4_DIR)/sel4_arch_include/ia32/interfaces/sel4arch.xml
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python $(LIBSEL4_DIR)/tools/invocation_header_gen.py \
	                 --xml $< --libsel4 --sel4_arch --dest $@

$(BUILD_BASE_DIR)/include/sel4/arch/invocation.h: $(LIBSEL4_DIR)/arch_include/x86/interfaces/sel4arch.xml
	$(MSG_CONVERT)arch/$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python $(LIBSEL4_DIR)/tools/invocation_header_gen.py \
	                 --xml $< --libsel4 --arch --dest $@

SEL4_CLIENT_H_SRC := $(LIBSEL4_DIR)/sel4_arch_include/ia32/interfaces/sel4arch.xml \
                     $(LIBSEL4_DIR)/arch_include/x86/interfaces/sel4arch.xml \
                     $(LIBSEL4_DIR)/include/interfaces/sel4.xml


$(BUILD_BASE_DIR)/include/interfaces/sel4_client.h: $(SEL4_CLIENT_H_SRC)
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python $(LIBSEL4_DIR)/tools/syscall_stub_gen.py \
	                 --buffer -a ia32 --word-size 32 -o $@ $(SEL4_CLIENT_H_SRC)

endif
