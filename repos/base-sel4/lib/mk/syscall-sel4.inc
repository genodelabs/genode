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

SEL4_ARCH_INCLUDES += simple_types.h types.h constants.h objecttype.h \
                      functions.h syscalls.h invocation.h deprecated.h \
                      types_gen.h faults.h

ARCH_INCLUDES += objecttype.h types.h constants.h deprecated.h \
                 syscalls.h invocation.h simple_types.h

INCLUDES := objecttype.h types.h bootinfo.h bootinfo_types.h errors.h \
            constants.h sel4.h macros.h simple_types.h \
            syscall.h invocation.h shared_types_gen.h debug_assert.h \
            shared_types.h deprecated.h autoconf.h syscalls.h faults.h \
            benchmark_utilisation_types.h \
            syscalls_master.h virtual_client.h

PLAT_API_INCLUDES := constants.h

INCLUDE_SYMLINKS += $(addprefix include/sel4/,          $(INCLUDES))
INCLUDE_SYMLINKS += $(addprefix include/sel4/arch/,     $(ARCH_INCLUDES))
INCLUDE_SYMLINKS += $(addprefix include/sel4/sel4_arch/,$(SEL4_ARCH_INCLUDES))
INCLUDE_SYMLINKS += $(addprefix include/sel4/plat/api/, $(PLAT_API_INCLUDES))
INCLUDE_SYMLINKS += include/sel4/mode/types.h
INCLUDE_SYMLINKS += include/interfaces/sel4_client.h

all: $(INCLUDE_SYMLINKS)

#
# IPCBuffer pointer are not set up by kernel anymore, instead user land has
# to care about tracking the IPCBuffer, details are in the RFC
#
#   https://sel4.atlassian.net/browse/RFC-3
#
# The usage of the __thread variable in the original include/sel4/functions.h
# would require our emutls emulation to work in core/roottask at an very
# early stage, which we don't support currently.
#
# As workaround, we setup the tls pointer per architecture such, that it
# points to the IPCBuffer pointer per thread, which is implemented in
# repos/base-sel4/include/sel4/functions_$(SEL4_ARCH).h
#
include/sel4/sel4_arch/functions.h: $(LIBSEL4_DIR)/include/sel4/functions.h
	$(VERBOSE)mkdir -p $(dir $@) && \
	sed -e "s|extern __thread seL4_IPCBuffer|//extern __thread seL4_IPCBuffer|" \
	    -e "s|#include <sel4/syscalls.h>|#include <sel4/syscalls.h>\n#include <sel4/functions_$(SEL4_ARCH).h>\n|" \
	    -e "/LIBSEL4_INLINE_FUNC void seL4_SetIPCBuffer(seL4_IPCBuffer \*ipc_buffer)/,+4d" \
	    -e "/LIBSEL4_INLINE_FUNC seL4_IPCBuffer \*seL4_GetIPCBuffer/,+3d" \
	       $< >include/sel4/functions.h && \
	touch $@

#
# Plain symlinks to existing headers
#
include/sel4/sel4_arch/%.h: $(LIBSEL4_DIR)/sel4_arch_include/$(SEL4_ARCH)/sel4/sel4_arch/%.h
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $< $@

include/sel4/arch/%.h: $(LIBSEL4_DIR)/arch_include/$(ARCH)/sel4/arch/%.h
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $< $@

include/sel4/autoconf.h: $(LIB_CACHE_DIR)/kernel-sel4-$(BOARD)/autoconf/autoconf.h
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $< $@
	$(VERBOSE)mkdir include/sel4/kernel -p
	$(VERBOSE)ln -sf $(LIB_CACHE_DIR)/kernel-sel4-$(BOARD)/gen_config/kernel/gen_config.h include/sel4/kernel/gen_config.h

include/sel4/%.h: $(LIBSEL4_DIR)/include/sel4/%.h
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $< $@

include/sel4/plat/api/%.h: $(LIBSEL4_DIR)/sel4_plat_include/$(PLAT)/sel4/plat/api/%.h
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $< $@

include/sel4/mode/types.h: $(LIBSEL4_DIR)/mode_include/$(SEL4_WORDBITS)/sel4/mode/types.h
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $< $@

#
# Generated headers
#
include/sel4/mode_include/$(SEL4_WORDBITS)/sel4/shared_types.pbf: $(LIBSEL4_DIR)/mode_include/$(SEL4_WORDBITS)/sel4/shared_types.bf include/sel4/autoconf.h
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)$(CPP) -Iinclude/sel4 -P $< >$@

include/sel4/sel4_arch/types.pbf: $(LIBSEL4_DIR)/sel4_arch_include/$(SEL4_ARCH)/sel4/sel4_arch/types.bf include/sel4/autoconf.h
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)$(CPP) -Iinclude/sel4 -I$(LIBSEL4_DIR)/arch_include/$(ARCH) -P $< >$@

include/sel4/shared_types_gen.h: include/sel4/mode_include/$(SEL4_WORDBITS)/sel4/shared_types.pbf
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python -B $(LIBSEL4_DIR)/tools/bitfield_gen.py \
	                 --environment libsel4 "$<" $@

include/sel4/sel4_arch/types_gen.h: include/sel4/sel4_arch/types.pbf
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python -B $(LIBSEL4_DIR)/tools/bitfield_gen.py \
	                 --environment libsel4 "$<" $@

include/sel4/syscall.h: $(LIBSEL4_DIR)/include/api/syscall.xml $(LIBSEL4_DIR)/include/api/syscall.xsd
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python -B $(LIBSEL4_DIR)/tools/syscall_header_gen.py \
	                 --xml $< --libsel4_header $@

include/sel4/invocation.h: $(LIBSEL4_DIR)/include/interfaces/sel4.xml
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python -B $(LIBSEL4_DIR)/tools/invocation_header_gen.py \
	                 --xml $< --libsel4 --dest $@

include/sel4/sel4_arch/invocation.h: $(LIBSEL4_DIR)/sel4_arch_include/$(SEL4_ARCH)/interfaces/sel4arch.xml
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python -B $(LIBSEL4_DIR)/tools/invocation_header_gen.py \
	                 --xml $< --libsel4 --sel4_arch --dest $@

include/sel4/arch/invocation.h: $(LIBSEL4_DIR)/arch_include/$(ARCH)/interfaces/sel4arch.xml
	$(MSG_CONVERT)arch/$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python -B $(LIBSEL4_DIR)/tools/invocation_header_gen.py \
	                 --xml $< --libsel4 --arch --dest $@

SEL4_CLIENT_H_SRC := $(LIBSEL4_DIR)/sel4_arch_include/$(SEL4_ARCH)/interfaces/sel4arch.xml \
                     $(LIBSEL4_DIR)/arch_include/$(ARCH)/interfaces/sel4arch.xml \
                     $(LIBSEL4_DIR)/include/interfaces/sel4.xml

include/interfaces/sel4_client.h: $(SEL4_CLIENT_H_SRC)
	$(MSG_CONVERT)$(notdir $@)
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)python -B $(LIBSEL4_DIR)/tools/syscall_stub_gen.py \
	                 --buffer -a $(SEL4_ARCH) --word-size $(SEL4_WORDBITS) -o $@ $(SEL4_CLIENT_H_SRC)

endif
