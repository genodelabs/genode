TARGET = test-libc_fs
LIBS   = libc libc_log libc_fs
SRC_CC = main.cc

# we re-use the libc_ffat test
vpath main.cc $(REP_DIR)/src/test/libc_ffat
