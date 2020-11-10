REQUIRES := x86_64

TARGET  := cbe_tester
SRC_CC  += main.cc
SRC_CC  += crypto.cc
SRC_CC  += trust_anchor.cc
SRC_CC  += vfs_utilities.cc

INC_DIR := $(PRG_DIR)
LIBS    += base cbe_cxx cbe_init_cxx cbe_check_cxx cbe_dump_cxx vfs
