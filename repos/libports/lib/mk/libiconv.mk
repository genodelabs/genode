include $(REP_DIR)/ports/libiconv.inc

LIBICONV_DIR = $(REP_DIR)/contrib/$(LIBICONV)
LIBS      += libc

# find 'config.h'
INC_DIR += $(REP_DIR)/src/lib/libiconv
INC_DIR += $(REP_DIR)/src/lib/libiconv/private

CC_DEF    += -DLIBDIR=\"\"

SRC_C = iconv.c \
        relocatable.c \
        localcharset.c

SHARED_LIB = yes

vpath %.c $(LIBICONV_DIR)/lib
vpath %.c $(LIBICONV_DIR)/libcharset/lib
