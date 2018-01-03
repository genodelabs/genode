LIBBZ2_DIR := $(call select_from_ports,bzip2)/src/lib/libbz2
LIBS       += libc

BIGFILES = -D_FILE_OFFSET_BITS=64
CC_DEF 	+= -Winline $(BIGFILES)

SRC_C = blocksort.c huffman.c crctable.c randtable.c \
        compress.c decompress.c bzlib.c

vpath %.c $(LIBBZ2_DIR)

# Bzip2 upstream recommends using a static library.

CC_CXX_WARN_STRICT =
