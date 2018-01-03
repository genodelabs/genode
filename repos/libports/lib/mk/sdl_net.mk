SDL_NET_PORT_DIR := $(call select_from_ports,sdl_net)
SDL_NET_DIR      := $(SDL_NET_PORT_DIR)/src/lib/sdl_net

SRC_C = $(notdir $(wildcard $(SDL_NET_PORT_DIR)/src/lib/sdl_net/SDLnet*.c))

vpath %.c $(SDL_NET_PORT_DIR)/src/lib/sdl_net

INC_DIR += $(SDL_NET_PORT_DIR)/src/lib/sdl_net

LIBS += libc sdl

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
