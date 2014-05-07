TARGET   = test-xev_track
REQUIRES = host x11 xtest xdamage
SRC_CC   = main.cc
LIBS     = xev_track

EXT_OBJECTS    += -lX11 -lXdamage /usr/lib/libXtst.so.6
