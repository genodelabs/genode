SRC_C   = adler32.c compress.c crc32.c gzio.c uncompr.c deflate.c trees.c \
          zutil.c inflate.c infback.c inftrees.c inffast.c

CC_OPT += -nostdinc
LIBS    = mini_c

vpath % $(REP_DIR)/src/lib/libz/contrib

include $(REP_DIR)/lib/import/import-libz_static.mk
