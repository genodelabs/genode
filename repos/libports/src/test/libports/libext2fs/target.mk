TARGET = test-libext2fs
LIBS   = libc libext2fs
SRC_CC = main.cc

vpath main.cc $(PRG_DIR)/..

CC_CXX_WARN_STRICT =
