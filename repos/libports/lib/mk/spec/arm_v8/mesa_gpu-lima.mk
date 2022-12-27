SHARED_LIB := yes
LIBS       := libc egl

include $(REP_DIR)/lib/mk/mesa-common.inc

SRC_CC := drm_init.cc
SRC_C  := platform_lima.c

CC_OPT += -DHAVE_GENODE_PLATFORM

INC_DIR += $(MESA_SRC_DIR)/src/egl/drivers/dri2 \
           $(MESA_SRC_DIR)/src/egl/main \
           $(MESA_SRC_DIR)/src/mapi \
           $(MESA_SRC_DIR)/src/mesa

vpath %.c  $(LIB_DIR)/lima
vpath %.cc $(LIB_DIR)/lima

CC_CXX_WARN_STRICT :=
