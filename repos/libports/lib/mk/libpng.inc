LIBPNG_DIR := $(call select_from_ports,libpng)/src/lib/libpng
LIBS       += libc libm zlib

# find 'config.h'
INC_DIR += $(REP_DIR)/src/lib/libpng

INC_DIR += $(call select_from_ports,libpng)/include/libpng

CC_DEF += -DHAVE_CONFIG_H

SRC_C = png.c pngset.c pngget.c pngrutil.c pngtrans.c pngwutil.c \
        pngread.c pngrio.c pngwio.c pngwrite.c pngrtran.c pngwtran.c \
        pngmem.c pngerror.c pngpread.c 

vpath %.c $(LIBPNG_DIR)

SHARED_LIB = yes
