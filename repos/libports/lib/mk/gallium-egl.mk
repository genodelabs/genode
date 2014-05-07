include $(REP_DIR)/lib/mk/gallium.inc

EGL_ST_SRC_DIR := $(MESA_PORT_DIR)/src/lib/mesa/src/gallium/state_trackers/egl
INC_DIR        += $(EGL_ST_SRC_DIR)
INC_DIR        += $(MESA_PORT_DIR)/src/lib/mesa/src/egl/main
INC_DIR        += $(MESA_PORT_DIR)/src/lib/mesa/src/gallium
CC_OPT         += -DRTLD_NODELETE=0

# generic driver code
SRC_C := $(notdir $(wildcard $(EGL_ST_SRC_DIR)/common/*.c))
vpath %.c $(EGL_ST_SRC_DIR)/common

# state tracker declarations for OpenGL ES1 and ES2
SRC_C += st_es1.c st_es2.c
vpath %.c $(MESA_PORT_DIR)/src/lib/mesa/src/gallium/state_trackers/es

# state tracker declarations for OpenGL
SRC_C += st_opengl.c
vpath st_opengl.c $(REP_DIR)/src/lib/egl

# Genode-specific driver code
SRC_CC += driver.cc select_driver.cc
vpath driver.cc $(REP_DIR)/src/lib/egl
vpath select_driver.cc $(REP_DIR)/src/lib/egl
LIBS += blit

# MESA state tracker code
MESA_ST_SRC_DIR := $(MESA_PORT_DIR)/src/lib/mesa/src/mesa/state_tracker
INC_DIR += $(MESA_ST_SRC_DIR)
INC_DIR += $(MESA_PORT_DIR)/src/lib/mesa/src/mesa/main
INC_DIR += $(MESA_PORT_DIR)/src/lib/mesa/src/mesa

SRC_C += $(notdir $(wildcard $(MESA_ST_SRC_DIR)/*.c))
vpath %.c $(MESA_ST_SRC_DIR)

# dim warning noise
CC_OPT_st_atom_pixeltransfer += -Wno-unused-but-set-variable
CC_OPT_st_cb_drawpixels      += -Wno-unused-but-set-variable
CC_OPT_st_cb_texture         += -Wno-strict-aliasing
CC_OPT_st_cb_texture         += -Wno-unused-but-set-variable
CC_OPT_st_framebuffer        += -Wno-strict-aliasing
CC_OPT_st_program            += -Wno-unused-but-set-variable
CC_OPT_st_texture            += -Wno-unused-but-set-variable

