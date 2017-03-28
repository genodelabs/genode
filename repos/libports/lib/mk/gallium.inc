MESA_PORT_DIR   := $(call select_from_ports,mesa)
GALLIUM_SRC_DIR := $(MESA_PORT_DIR)/src/lib/mesa/src/gallium

LIBS += libc libm mesa_host_tools

INC_DIR += $(GALLIUM_SRC_DIR)/include \
           $(GALLIUM_SRC_DIR)/auxiliary \
           $(GALLIUM_SRC_DIR)/auxiliary/util \
           $(GALLIUM_SRC_DIR)/drivers \
           $(MESA_PORT_DIR)/include

CC_OPT += -U__linux__

#
# Prevent double definition of 'ushort' and 'uint' in 'pipe/p_compiler.h' and
# 'libc/sys/types.h'. By defining '__USE_MISC', we suppress the first one.
# However, because the libc headers are not included by all gallium sources
# that include 'p_compiler.h', we unconditionally include 'sys/types.h'.
#
CC_OPT += -D__USE_MISC -include sys/types.h
