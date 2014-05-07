SDL_PORT_DIR := $(call select_from_ports,sdl)

INC_DIR      += $(SDL_PORT_DIR)/include $(SDL_PORT_DIR)/include/SDL
REP_INC_DIR  += include/SDL
