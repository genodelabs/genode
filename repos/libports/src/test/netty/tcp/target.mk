TARGET = test-netty_tcp
SRC_CC = main.cc netty.cc
LIBS   = libc

INC_DIR += $(PRG_DIR)/..

vpath netty.cc $(PRG_DIR)/..

CC_CXX_WARN_STRICT =
