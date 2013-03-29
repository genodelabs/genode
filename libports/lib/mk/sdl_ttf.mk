include $(REP_DIR)/ports/sdl_ttf.inc

SRC_C = SDL_ttf.c
LIBS += libc libm freetype sdl

vpath %.c $(REP_DIR)/contrib/$(SDL_TTF)

SHARED_LIB = yes
