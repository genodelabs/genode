REQUIRES += x86_64

TARGET:= cbe_init_trust_anchor

SRC_CC := component.cc
INC_DIR := $(PRG_DIR)

LIBS := base vfs
