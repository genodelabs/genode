TARGET = test-vmm_utils
SRC_CC = main.cc
LIBS  += base

REQUIRES = nova

vpath %.cc $(PRG_DIR)/..
