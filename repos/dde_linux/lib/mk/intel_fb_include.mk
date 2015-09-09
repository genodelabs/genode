#
# Pseudo library to generate a symlink for each header file included by the
# contrib code. Each symlink points to the same 'lx_emul.h' file, which
# provides our emulation of the Linux kernel API.
#

LX_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/framebuffer/intel
LX_EMUL_H      := $(REP_DIR)/src/drivers/framebuffer/intel/include/lx_emul.h

GEN_INCLUDES := $(shell grep -rh "^\#include .*\/" $(LX_CONTRIB_DIR) |\
                        sed "s/^\#include [^<\"]*[<\"]\([^>\"]*\)[>\"].*/\1/" | sort | uniq)

all: $(GEN_INCLUDES)

$(GEN_INCLUDES):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $(LX_EMUL_H) $@
