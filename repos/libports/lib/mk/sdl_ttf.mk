SRC_C = SDL_ttf.c
LIBS += libc libm freetype sdl

vpath %.c $(call select_from_ports,sdl_ttf)/src/lib/sdl_ttf

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
