include $(REP_DIR)/ports/sdl_mixer.inc

# exclude example programs
FILTER_OUT = playmus.c playwave.c

ALL_SDL_MIXER_SRC_C = $(notdir $(wildcard $(REP_DIR)/contrib/$(SDL_MIXER)/*.c))

SRC_C = $(filter-out $(FILTER_OUT), $(ALL_SDL_MIXER_SRC_C))

LIBS += libc libm sdl

# suppress warnings of 3rd-party code
CC_OPT_music      = -Wno-unused-label -Wno-unused-function
CC_OPT_load_aiff  = -Wno-unused-but-set-variable
CC_OPT_wavestream = -Wno-unused-but-set-variable

vpath %.c $(REP_DIR)/contrib/$(SDL_MIXER)

SHARED_LIB = yes
