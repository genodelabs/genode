#
# FAT File System Module using a Block session as disk I/O backend
#

FATFS_PORT_DIR      := $(call select_from_ports,fatfs)
FATFS_PORT_SRC_DIR  := $(FATFS_PORT_DIR)/src/lib/fatfs/source
FATFS_LOCAL_SRC_DIR := $(REP_DIR)/src/lib/fatfs

INC_DIR += $(REP_DIR)/include/fatfs $(FATFS_PORT_DIR)/include

SRC_C  = ff.c ffunicode.c
SRC_CC = diskio_block.cc

CC_OPT += -Wno-unused-variable

vpath % $(FATFS_LOCAL_SRC_DIR)
vpath % $(FATFS_PORT_SRC_DIR)

CC_CXX_WARN_STRICT =
