ifeq ($(called_from_lib_mk),yes)

USB_NET_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/drivers/usb_net
LX_EMUL_H           := $(REP_DIR)/src/drivers/legacy_usb_net/lx_emul.h

#
# Determine the header files included by the contrib code. For each
# of these header files we create a symlink to 'lx_emul.h'.
#
SCAN_DIRS := $(addprefix $(USB_NET_CONTRIB_DIR)/include/, asm-generic linux uapi net) \
             $(addprefix $(USB_NET_CONTRIB_DIR)/,         drivers net lib)
GEN_INCLUDES := $(shell grep -rIh "^\#include .*\/" $(SCAN_DIRS) |\
                        sed "s/^\#include [^<\"]*[<\"]\([^>\"]*\)[>\"].*/\1/" |\
                        sort | uniq)
#
# Filter out original Linux headers that exist in the contrib directory
#
NO_GEN_INCLUDES := $(shell cd $(USB_NET_CONTRIB_DIR)/; find include  -name "*.h" |\
                           sed "s/.\///" | sed "s/.*include\///")
GEN_INCLUDES    := $(filter-out $(NO_GEN_INCLUDES),$(GEN_INCLUDES))

#
# Put Linux headers in 'GEN_INC' dir, since some include use "../../" paths use
# three level include hierarchy
#
GEN_INC         := $(shell pwd)/include/include/include
GEN_INCLUDES    := $(addprefix $(GEN_INC)/,$(GEN_INCLUDES))

all: $(GEN_INCLUDES)

$(GEN_INCLUDES):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -s $(LX_EMUL_H) $@

endif

CC_CXX_WARN_STRICT =
