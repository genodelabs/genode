TARGET = test-mesa
LIBS   = libc mesa egl
SRC_CC = main.cc

vpath main.cc $(PRG_DIR)/..

CC_CXX_WARN_STRICT =
