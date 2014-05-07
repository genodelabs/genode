include $(REP_DIR)/lib/mk/mesa.inc

SRC_C := $(notdir $(wildcard $(MESA_PORT_DIR)/src/lib/mesa/src/egl/main/*.c))

CC_OPT += -D_EGL_DRIVER_SEARCH_DIR=\"\"
CC_OPT += -D_EGL_DEFAULT_DISPLAY=\"\"
CC_OPT += -U__unix__ -U__unix

# dim warning noise
CC_OPT_eglconfig += -Wno-uninitialized

vpath %.c $(MESA_PORT_DIR)/src/lib/mesa/src/egl/main

