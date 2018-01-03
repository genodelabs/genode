TARGET = test-fatfs_blkio
LIBS = fatfs_block libc
SRC_C = app4.c
SRC_CC = component.cc

CC_DEF += -D_MAX_SS=FF_MAX_SS
CC_WARN += -Wno-pointer-to-int-cast

FATFS_PORT_DIR = $(call select_from_ports,fatfs)

INC_DIR += $(FATFS_PORT_DIR)/include/fatfs

vpath %.c $(FATFS_PORT_DIR)/src/lib/fatfs/documents/res

CC_CXX_WARN_STRICT =
