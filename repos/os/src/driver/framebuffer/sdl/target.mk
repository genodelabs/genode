TARGET   = fb_sdl
LIBS     = lx_hybrid blit
REQUIRES = linux
SRC_CC   = main.cc
LX_LIBS  = sdl2
INC_DIR += $(PRG_DIR) /usr/include/SDL2

# accept conversion warnings caused by libgcc's include/arm_neon.h
CC_OPT  += -Wno-error=narrowing \
           -Wno-error=float-conversion \
           -Wno-error=unused-parameter \
           -Wno-error=cpp
