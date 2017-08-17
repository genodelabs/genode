MESA_PORT_DIR := $(call select_from_ports,mesa)/src/lib/mesa
MESA_SRC_DIR  := $(MESA_PORT_DIR)/src/mesa
LIB_DIR       := $(REP_DIR)/src/lib/mesa

INC_DIR += $(MESA_SRC_DIR) \
           $(MESA_SRC_DIR)/main \
           $(MESA_SRC_DIR)/x86 \
           $(MESA_PORT_DIR) \
           $(MESA_PORT_DIR)/include \
           $(MESA_PORT_DIR)/src \
           $(MESA_PORT_DIR)/src/mapi \
           $(MESA_PORT_DIR)/src/gallium/auxiliary \
           $(MESA_PORT_DIR)/src/gallium/include \
           $(LIB_DIR)/include

CC_OPT += -DHAVE_PTHREAD -D_XOPEN_SOURCE=600 -DDEFAULT_DRIVER_DIR=\"\ \"

CC_WARN  += -Wno-return-type 
CC_C_OPT +=-Wno-implicit-function-declaration

# vi: set ft=make :

