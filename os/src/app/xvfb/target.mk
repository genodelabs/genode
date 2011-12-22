TARGET   = xvfb
REQUIRES = linux x11 xtest xdamage
SRC_CC   = main.cc inject_input.cc
LIBS     = env syscall blit xev_track

# we need X11 headers, so let us use standard includes and standard libs
STDINC       = yes
STDLIB       = yes
EXT_OBJECTS += -lX11 -lXdamage /usr/lib/libXtst.so.6
