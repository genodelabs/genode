#
# Pseudo library to generate a symlink for each header file included by the
# contrib code. Each symlink points to the same 'lx_emul.h' file, which
# provides our emulation of the Linux kernel API.
#

ifeq ($(called_from_lib_mk),yes)

LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/framebuffer/intel
LX_EMUL_H      := $(REP_DIR)/src/drivers/framebuffer/intel/include/lx_emul.h

#
# Determine the header files included by the contrib code. For each
# of these header files we create a symlink to 'lx_emul.h'.
#
SCAN_DIRS := $(addprefix $(LX_CONTRIB_DIR)/include/, drm uapi asm-generic video linux) \
             $(addprefix $(LX_CONTRIB_DIR)/, drivers lib arch/x86)
GEN_INCLUDES := $(shell grep -rIh "^\#include .*" $(SCAN_DIRS) |\
                        sed "s/^\#include [^<\"]*[<\"]\([^>\"]*\)[>\"].*/\1/" |\
                        sort | uniq)

#
# Put Linux headers in 'GEN_INC' dir, since some include use "../../" paths use
# three level include hierarchy
#
GEN_INC         := $(shell pwd)/include/include/include
GEN_INCLUDES    := $(addprefix $(GEN_INC)/,$(GEN_INCLUDES))

all: $(GEN_INCLUDES)

$(GEN_INCLUDES):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $(LX_EMUL_H) $@

endif
