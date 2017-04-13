SRC_C   = png.c pngset.c pngget.c pngrutil.c pngtrans.c pngwutil.c pngread.c \
          pngrio.c pngwio.c pngwrite.c pngrtran.c pngwtran.c pngmem.c \
          pngerror.c pngpread.c

CC_OPT += -nostdinc -funroll-loops -DPNG_USER_CONFIG
LIBS    = mini_c libz_static

CC_WARN = -Wall -Wno-address -Wno-misleading-indentation

vpath % $(REP_DIR)/src/lib/libpng/contrib

include $(REP_DIR)/lib/import/import-libpng_static.mk
