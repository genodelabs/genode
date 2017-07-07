TARGET = test-libc_ffat
LIBS   = libc libc_ffat
SRC_CC = main.cc

# we re-use the libc_vfs test
vpath main.cc $(REP_DIR)/src/test/libc_vfs
