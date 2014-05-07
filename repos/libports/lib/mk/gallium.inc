MESA            = Mesa-7.8.1
MESA_DIR        = $(REP_DIR)/contrib/$(MESA)
GALLIUM_SRC_DIR = $(MESA_DIR)/src/gallium

LIBS += cxx libc libm

INC_DIR += $(GALLIUM_SRC_DIR)/include \
           $(GALLIUM_SRC_DIR)/auxiliary \
           $(GALLIUM_SRC_DIR)/auxiliary/util \
           $(GALLIUM_SRC_DIR)/drivers

CC_OPT += -U__linux__

#
# Prevent double definition of 'ushort' and 'uint' in 'pipe/p_compiler.h' and
# 'libc/sys/types.h'. By defining '__USE_MISC', we suppress the first one.
# However, because the libc headers are not included by all gallium sources
# that include 'p_compiler.h', we unconditionally include 'sys/types.h'.
#
CC_OPT += -D__USE_MISC -include sys/types.h

#
# Detect missing preparation of Mesa library, skip unprepared library
#
ifeq ($(wildcard $(REP_DIR)/include/GL),)
REQUIRES = prepare_mesa
endif

