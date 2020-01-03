ZLIB_DIR = $(call select_from_ports,zlib)/src/lib/zlib
LIBS    += libc
INC_DIR += $(ZLIB_DIR)
SRC_C    = adler32.c compress.c crc32.c deflate.c gzclose.c \
           gzlib.c gzread.c gzwrite.c infback.c inffast.c inflate.c \
           inftrees.c trees.c uncompr.c zutil.c
CC_WARN  =
CC_OPT  += -DZ_HAVE_UNISTD_H

vpath %.c $(ZLIB_DIR)

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
