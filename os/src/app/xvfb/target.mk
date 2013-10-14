TARGET   = xvfb
REQUIRES = linux x11 xtest xdamage
SRC_CC   = main.cc inject_input.cc
LIBS     = lx_hybrid syscall blit xev_track config

EXT_OBJECTS += -lX11 -lXdamage /usr/lib/libXtst.so.6
