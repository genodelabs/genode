TARGET = test-libc_vfs
LIBS   = posix
SRC_CC = main.cc

# we re-use the libc_ffat test
vpath main.cc $(REP_DIR)/src/test/libc_ffat
