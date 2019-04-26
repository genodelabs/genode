LIBICONV_PORT_DIR := $(call select_from_ports,libiconv)

LIBS += libc

INC_DIR += $(REP_DIR)/include/iconv
INC_DIR += $(LIBICONV_PORT_DIR)/include/iconv

# find 'config.h'
INC_DIR += $(REP_DIR)/src/lib/libiconv/private

CC_DEF += -DLIBDIR=\"\"

SRC_C := iconv.c \
         relocatable.c \
         localcharset.c

SHARED_LIB = yes

vpath %.c $(LIBICONV_PORT_DIR)/src/lib/libiconv/lib
vpath %.c $(LIBICONV_PORT_DIR)/src/lib/libiconv/libcharset/lib

CC_CXX_WARN_STRICT =
