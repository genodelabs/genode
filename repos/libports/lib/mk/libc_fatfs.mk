SRC_CC  = plugin.cc
LIBS   += libc fatfs_block

vpath plugin.cc $(REP_DIR)/src/lib/libc_fatfs

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
