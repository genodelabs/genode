SHARED_LIB = yes
LIBS       = libc egl swrast

include $(REP_DIR)/lib/mk/mesa-common.inc

SRC_C = platform_swrast.c

CC_OPT += -DHAVE_GENODE_PLATFORM

INC_DIR += $(MESA_PORT_DIR)/src/egl/main \
           $(MESA_PORT_DIR)/src/egl/drivers/dri2

vpath %.c $(LIB_DIR)/swrast
