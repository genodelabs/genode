TARGET   = liquid_fb
LIBS     = scout_widgets config
SRC_CC   = main.cc services.cc
INC_DIR += $(REP_DIR)/src/app/scout \
           $(REP_DIR)/src/server/framebuffer/sdl
