TARGET = test-libc_fatfs
LIBS   = libc libc_fatfs
SRC_CC = main.cc

# we re-use the libc_vfs test
vpath main.cc $(REP_DIR)/src/test/libc_vfs

CC_CXX_WARN_STRICT =
