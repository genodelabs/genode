include $(REP_DIR)/ports/sdl_net.inc
SDL_NET_DIR = $(REP_DIR)/contrib/$(SDL_NET)

SRC_C = $(notdir $(wildcard $(SDL_NET_DIR)/SDLnet*.c))

vpath %.c $(SDL_NET_DIR)

INC_DIR += $(SDL_NET_DIR)

LIBS += libc sdl

SHARED_LIB = yes
