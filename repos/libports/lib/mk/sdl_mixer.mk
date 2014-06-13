SDL_MIXER_PORT_DIR := $(call select_from_ports,sdl_mixer)

# exclude example programs
FILTER_OUT = playmus.c playwave.c

ALL_SDL_MIXER_SRC_C = $(notdir $(wildcard $(SDL_MIXER_PORT_DIR)/src/lib/sdl_mixer/*.c))

SRC_C = $(filter-out $(FILTER_OUT), $(ALL_SDL_MIXER_SRC_C))

LIBS += libc libm sdl libogg libvorbis

CC_OPT = -DOGG_MUSIC -Wno-unused-variable
# suppress warnings of 3rd-party code
CC_OPT_music      = -Wno-unused-label -Wno-unused-function
CC_OPT_load_aiff  = -Wno-unused-but-set-variable
CC_OPT_wavestream = -Wno-unused-but-set-variable

vpath %.c $(SDL_MIXER_PORT_DIR)/src/lib/sdl_mixer

SHARED_LIB = yes
