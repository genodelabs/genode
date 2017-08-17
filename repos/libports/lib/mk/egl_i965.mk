SHARED_LIB = yes
LIBS       = libc egl i965 pthread

include $(REP_DIR)/lib/mk/mesa-common.inc

SRC_C   = platform_i965.c
SRC_CC  = drm_init.cc

CC_OPT += -DHAVE_GENODE_PLATFORM

INC_DIR += $(MESA_PORT_DIR)/src/egl/main \
           $(MESA_PORT_DIR)/src/egl/drivers/dri2

vpath %.c $(LIB_DIR)/i965
vpath %.cc $(LIB_DIR)/i965
