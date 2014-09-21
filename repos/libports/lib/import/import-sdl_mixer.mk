SDL_MIXER_PORT_DIR := $(call select_from_ports,sdl_mixer)
INC_DIR += $(SDL_MIXER_PORT_DIR)/include $(SDL_MIXER_PORT_DIR)/include/SDL
