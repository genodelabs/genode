MESA_PORT_DIR := $(call select_from_ports,mesa)
MESA_SRC_DIR  := $(MESA_PORT_DIR)/src/lib/mesa/src/mesa

LIBS += libc libm mesa_host_tools

#
# Prevent warning about non-existing 'fpu_control.h' included
# by 'mesa/main/compiler.h' if '__linux__' is defined.
#
CC_OPT += -U__linux__

INC_DIR += $(MESA_PORT_DIR)/src/lib/mesa/src/mesa \
           $(MESA_PORT_DIR)/src/lib/mesa/src/gallium/include \
           $(MESA_PORT_DIR)/src/lib/mesa/src/gallium/auxiliary \
           $(MESA_PORT_DIR)/include

#SHARED_LIB = yes
