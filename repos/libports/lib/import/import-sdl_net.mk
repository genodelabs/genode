SDL_NET_PORT_DIR := $(call select_from_ports,sdl_net)
INC_DIR += $(SDL_NET_PORT_DIR)/include $(SDL_NET_PORT_DIR)/include/SDL
