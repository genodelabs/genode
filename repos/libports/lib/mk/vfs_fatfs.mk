SRC_CC = vfs_fatfs.cc

LIBS  += fatfs_block

vpath %.cc $(REP_DIR)/src/lib/vfs/fatfs

SHARED_LIB = yes
