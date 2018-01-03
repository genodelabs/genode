SDL_IMAGE_PORT_DIR := $(call select_from_ports,sdl_image)

SRC_C = $(notdir $(wildcard $(SDL_IMAGE_PORT_DIR)/src/lib/sdl_image/IMG*.c))

LIBS += libc libm sdl jpeg libpng zlib

SUPPORTED_FORMATS = PNG JPG TGA PNM XPM

CC_OPT += $(addprefix -DLOAD_,$(SUPPORTED_FORMATS))

vpath %.c $(SDL_IMAGE_PORT_DIR)/src/lib/sdl_image

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
