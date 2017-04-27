TARGET = test-netty_tcp
SRC_CC = main.cc netty.cc
LIBS   = libc libc_resolv

INC_DIR += $(PRG_DIR)/..

vpath netty.cc $(PRG_DIR)/..
