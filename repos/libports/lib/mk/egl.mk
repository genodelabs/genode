SHARED_LIB = yes
LIBS       = libc blit

include $(REP_DIR)/lib/mk/mesa-common.inc

SRC_C = \
	main/eglsurface.c \
	main/eglconfig.c \
	main/eglglobals.c \
	main/egldisplay.c \
	main/eglfallbacks.c \
	main/eglsync.c \
	main/egllog.c \
	main/eglarray.c \
	main/eglimage.c \
	main/eglcontext.c \
	main/eglcurrent.c \
	main/eglapi.c \
	main/egldriver.c \
	drivers/dri2/egl_dri2.c \
	platform.c

SRC_CC = genode_interface.cc

CC_OPT  += -D_EGL_NATIVE_PLATFORM=_EGL_PLATFORM_GENODE -D_EGL_BUILT_IN_DRIVER_DRI2 \
           -DHAVE_GENODE_PLATFORM

INC_DIR += $(MESA_PORT_DIR)/src/egl/main \
           $(MESA_PORT_DIR)/src/egl/drivers/dri2

vpath %.c  $(MESA_PORT_DIR)/src/egl
vpath %.c  $(LIB_DIR)/egl
vpath %.cc $(LIB_DIR)/egl
