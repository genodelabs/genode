ifeq ($(called_from_lib_mk),yes)

LIBNL_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/lib/libnl
LIBNL_EMUL_H      := $(REP_DIR)/src/lib/libnl/include/libnl_emul.h

#
# Generate links for emulated header files in build directory
#
EMUL_INC      := $(shell pwd)/include
EMUL_INC_LIST := $(addprefix linux/, netdevice.h ethtool.h atm.h socket.h in_route.h) \
                 $(addprefix asm/, byteorder.h)
EMUL_INCLUDES := $(addprefix $(EMUL_INC)/,$(EMUL_INC_LIST))

all: $(EMUL_INCLUDES)

$(EMUL_INCLUDES):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -s $(LIBNL_EMUL_H) $@

endif

# vi: set ft=make :

CC_CXX_WARN_STRICT =
