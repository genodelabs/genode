ifeq ($(called_from_lib_mk),yes)

QEMU_CONTRIB_DIR := $(call select_from_ports,qemu-usb)/src/lib/qemu
QEMU_EMUL_H      := $(REP_DIR)/src/lib/qemu-usb/include/qemu_emul.h

#
# Determine the header files included by the contrib code. For each
# of these header files we create a symlink to 'qemu_emul.h'.
#
GEN_INCLUDES := $(shell grep -rIh "^\#include .*" $(QEMU_CONTRIB_DIR) |\
                        sed "s/^\#include [^<\"]*[<\"]\([^>\"]*\)[>\"].*/\1/" | sort | uniq)
#
# Put Qemu headers in 'GEN_INC' dir
#
GEN_INC         := $(shell pwd)/include
GEN_INCLUDES    := $(addprefix $(GEN_INC)/,$(GEN_INCLUDES))

all: $(GEN_INCLUDES)

$(GEN_INCLUDES):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -s $(QEMU_EMUL_H) $@

endif
