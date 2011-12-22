TARGET   = liquid_fb
LIBS     = scout_widgets
SRC_CC   = main.cc services.cc
INC_DIR += $(REP_DIR)/src/app/scout/include \
           $(REP_DIR)/src/app/scout/include/genode \
           $(REP_DIR)/src/server/framebuffer/sdl

# suppress non-critical but weird compiler warning, probably a bug in gcc-4.6.1
CC_OPT_main += -Wno-uninitialized
