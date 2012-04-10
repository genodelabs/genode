#
# SDL library
#

SDL = SDL-1.2.13
SDL_DIR = $(REP_DIR)/contrib/$(SDL)

#
# XXX libSDL does not build on ARM because a compile-time assertion fails:
#
#
# SDL_stdinc.h:133:1: error: size of array ‘SDL_dummy_enum’ is negative
#
REQUIRES = x86

# build shared object
SHARED_LIB = yes

# backends
SRC_CC   = SDL_genode_fb_video.cc \
           SDL_genode_fb_events.cc \
           SDL_genodeaudio.cc \
           SDL_systimer.cc

INC_DIR += $(REP_DIR)/include/SDL \
           $(REP_DIR)/src/lib/sdl \
           $(REP_DIR)/src/lib/sdl/thread \
           $(REP_DIR)/src/lib/sdl/video

# main files
SRC_C    = SDL.c \
           SDL_error.c \
           SDL_fatal.c

INC_DIR  += $(REP_DIR)/src/lib/sdl
#INC_DIR += $(REP_DIR)/include/SDL_genode \
#           $(REP_DIR)/include/SDL_genode/SDL \
#           $(REP_DIR)/include/SDL_contrib \
#           $(REP_DIR)/include/SDL_contrib/SDL \
#           $(REP_DIR)/src/lib/sdl/src \

# stdlib files
SRC_C   += SDL_getenv.c \
           SDL_string.h

# thread subsystem
SRC_C   += SDL_thread.c \
           SDL_systhread.c \
           SDL_syscond.c \
           SDL_sysmutex.c \
           SDL_syssem.c
INC_DIR += $(SDL_DIR)/src/thread

# cpuinfo subsystem
SRC_C   += SDL_cpuinfo.c

# timer subsystem
SRC_C   += SDL_timer.c
INC_DIR += $(SDL_DIR)/src/timer

# video subsystem
SRC_C  +=  SDL_blit_0.c \
           SDL_blit.c \
           SDL_cursor.c \
           SDL_RLEaccel.c \
           SDL_video.c \
           SDL_yuv_sw.c \
           SDL_blit_1.c \
           SDL_blit_N.c \
           SDL_gamma.c \
           SDL_stretch.c \
           SDL_yuv.c \
           SDL_blit_A.c \
           SDL_bmp.c \
           SDL_pixels.c \
           SDL_surface.c \
           SDL_yuv_mmx.c
INC_DIR += $(SDL_DIR)/src/video

# event subsystem
SRC_C   += SDL_events.c \
           SDL_keyboard.c \
           SDL_mouse.c \
           SDL_resize.c \
           SDL_active.c \
           SDL_quit.c
INC_DIR += $(SDL_DIR)/src/events

# audio subsystem
SRC_C   += SDL_audio.c \
           SDL_audiocvt.c \
           SDL_audiodev.c \
           SDL_mixer.c \
           SDL_mixer_m68k.c \
           SDL_mixer_MMX.c \
           SDL_mixer_MMX_VC.c \
           SDL_wave.c

INC_DIR += $(SDL_DIR)/src/audio

# file I/O subsystem
SRC_C   += SDL_rwops.c

# joystick subsystem
SRC_C   += SDL_joystick.c \
           SDL_sysjoystick.c
INC_DIR += $(SDL_DIR)/src/joystick

# we need libc
LIBS = libc pthread

# dim build noise for contrib code
CC_OPT_SDL_RLEaccel += -Wno-unused-but-set-variable
CC_OPT_SDL_bmp      += -Wno-unused-but-set-variable
CC_OPT_SDL_stretch  += -Wno-unused-but-set-variable
CC_OPT_SDL_wave     += -Wno-unused-but-set-variable

# backend pathes
vpath %    $(REP_DIR)/src/lib/sdl
vpath %    $(REP_DIR)/src/lib/sdl/audio
vpath %    $(REP_DIR)/src/lib/sdl/timer
vpath %    $(REP_DIR)/src/lib/sdl/video

# contribution pathes
vpath SDL_syscond.c   $(SDL_DIR)/src/thread/generic
vpath SDL_sysmutex.c  $(SDL_DIR)/src/thread/generic

vpath %.c  $(SDL_DIR)/src
vpath %.c  $(SDL_DIR)/src/stdlib
vpath %.c  $(SDL_DIR)/src/video
vpath %.c  $(SDL_DIR)/src/video/dummy
vpath %.c  $(SDL_DIR)/src/audio
vpath %.c  $(SDL_DIR)/src/thread
vpath %.c  $(SDL_DIR)/src/thread/pthread
vpath %.c  $(SDL_DIR)/src/timer
vpath %.c  $(SDL_DIR)/src/events
vpath %.c  $(SDL_DIR)/src/cpuinfo
vpath %.c  $(SDL_DIR)/src/file
vpath %.c  $(SDL_DIR)/src/joystick
vpath %.c  $(SDL_DIR)/src/joystick/dummy
