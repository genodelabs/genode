#
# FAT File System Module
#

LIBS = dde_linux26_usbstorage

INC_DIR += $(REP_DIR)/src/lib/ffat/contrib

SRC_C = ff.c diskio.c

vpath % $(REP_DIR)/contrib/ff007e/src
vpath % $(REP_DIR)/src/lib/ffat/
