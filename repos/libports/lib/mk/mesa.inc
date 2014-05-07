MESA         = Mesa-7.8.1
MESA_DIR     = $(REP_DIR)/contrib/$(MESA)
MESA_SRC_DIR = $(MESA_DIR)/src/mesa

ifeq ($(wildcard $(MESA_DIR)),)
REQUIRES += prepare_mesa
endif

LIBS += cxx libc libm

#
# Prevent warning about non-existing 'fpu_control.h' included
# by 'mesa/main/compiler.h' if '__linux__' is defined.
#
CC_OPT += -U__linux__

INC_DIR += $(MESA_DIR)/src/mesa \
           $(MESA_DIR)/src/gallium/include \
           $(MESA_DIR)/src/gallium/auxiliary

#SHARED_LIB = yes
