# dynamic variant is not supported in hybrid mode
ifeq ($(filter-out $(SPECS),always_hybrid),)
REQUIRES = plain_linux
endif

TARGET = test-lx_rmap_dynamic
SRC_CC = main.cc
LIBS   = base libc

vpath main.cc $(PRG_DIR)/..
