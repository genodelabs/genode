TARGET   = untar
SRC_C    = untar.c
LIBS    += libarchive posix
CC_WARN += -Wno-unused-but-set-variable

LIBARCHIVE_DIR := $(call select_from_ports,libarchive)/src/lib/libarchive

vpath untar.c $(LIBARCHIVE_DIR)/examples

CC_CXX_WARN_STRICT =
