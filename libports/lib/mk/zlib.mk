ZLIB     = zlib-1.2.7
ZLIB_DIR = $(REP_DIR)/contrib/$(ZLIB)
LIBS    += libc
INC_DIR += $(ZLIB_DIR)
SRC_C    = adler32.c compress.c crc32.c deflate.c gzclose.c \
           gzlib.c gzread.c gzwrite.c infback.c inffast.c inflate.c \
           inftrees.c trees.c uncompr.c zutil.c
CC_WARN  =

vpath %.c $(ZLIB_DIR)

SHARED_LIB = yes
