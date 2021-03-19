SHARED_LIB = yes
LIBS       = libc blit

include $(REP_DIR)/lib/mk/mesa-common.inc

INC_DIR += $(MESA_SRC_DIR)/src/egl/main \
           $(MESA_SRC_DIR)/src/egl/drivers/dri2 \
           $(MESA_SRC_DIR)/src/loader

SRC_C = main/eglapi.c \
        main/eglarray.c \
        main/eglconfig.c \
        main/eglcontext.c \
        main/eglcurrent.c \
        main/egldevice.c \
        main/egldisplay.c \
        main/eglglobals.c \
        main/eglimage.c \
        main/egllog.c \
        main/eglsurface.c \
        main/eglsync.c \
        drivers/dri2/egl_dri2.c \
        drivers/dri2/platform_device.c \
        platform.c

SRC_CC = genode_interface.cc

CC_OPT += -D_EGL_NATIVE_PLATFORM=_EGL_PLATFORM_GENODE \
          -DHAVE_GENODE_PLATFORM

vpath %.c $(MESA_SRC_DIR)/src/egl
vpath %.c $(LIB_DIR)/egl
vpath %.cc $(LIB_DIR)/egl

