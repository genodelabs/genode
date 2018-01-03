#
# Pseudo library to generate a symlink for each header file included by the
# contrib code. Each symlink points to the same 'bsd_emul.h' file, which
# provides our emulation of the OpenBSD kernel API.
#

ifeq ($(called_from_lib_mk),yes)

BSD_CONTRIB_DIR := $(call select_from_ports,dde_bsd)/src/lib/audio
BSD_EMUL_H      := $(REP_DIR)/src/lib/audio/include/bsd_emul.h

SCAN_DIRS    := $(addprefix $(BSD_CONTRIB_DIR)/, dev sys)
GEN_INCLUDES := $(shell grep -rIh "^\#include .*" $(SCAN_DIRS) |\
                        sed "s/^\#include [^<\"]*[<\"]\([^>\"]*\)[>\"].*/\1/" | sort | uniq)

GEN_INC      := $(shell pwd)/include
GEN_INCLUDES := $(addprefix $(GEN_INC)/,$(GEN_INCLUDES))

all: $(GEN_INCLUDES)

$(GEN_INCLUDES):
	$(VERBOSE)mkdir -p $(dir $@)
	$(VERBOSE)ln -sf $(BSD_EMUL_H) $@

endif

CC_CXX_WARN_STRICT =
