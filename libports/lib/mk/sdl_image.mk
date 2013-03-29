include $(REP_DIR)/ports/sdl_image.inc

SRC_C = $(notdir $(wildcard $(REP_DIR)/contrib/$(SDL_IMAGE)/IMG*.c))
LIBS += libc libm sdl jpeg libpng zlib

SUPPORTED_FORMATS = PNG JPG TGA PNM

CC_OPT += $(addprefix -DLOAD_,$(SUPPORTED_FORMATS))

vpath %.c $(REP_DIR)/contrib/$(SDL_IMAGE)

SHARED_LIB = yes
