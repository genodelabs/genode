TARGET = test-netty_udp
SRC_CC = main.cc netty.cc
LIBS   = libc

INC_DIR += $(PRG_DIR)/..

vpath netty.cc $(PRG_DIR)/..
