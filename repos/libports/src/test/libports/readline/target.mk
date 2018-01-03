TARGET = test-readline
LIBS   = libc readline history
SRC_CC = main.cc

vpath main.cc $(PRG_DIR)/..

CC_CXX_WARN_STRICT =
