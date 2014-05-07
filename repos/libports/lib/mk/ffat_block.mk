#
# FAT File System Module using a Block session as disk I/O backend
#

FFAT_PORT_DIR := $(call select_from_ports,ffat)

INC_DIR += $(FFAT_PORT_DIR)/include

FFAT_DIR := $(FFAT_PORT_DIR)/src/lib/ffat

SRC_C  = ff.c ccsbcs.c
SRC_CC = diskio_block.cc

vpath % $(REP_DIR)/src/lib/ffat/
vpath % $(FFAT_DIR)/src
vpath % $(FFAT_DIR)/src/option
