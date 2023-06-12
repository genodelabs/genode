SHARED_LIB = yes
LIBS      += libc
COMPAT_DIR = $(REP_DIR)/src/lib/compat-libc

SRC_CC = compat.cc
SRC_C  = libc.c

vpath %.cc $(COMPAT_DIR)
vpath %.c  $(COMPAT_DIR)

LD_OPT  += --version-script=$(COMPAT_DIR)/symbol.map

CC_CXX_WARN_STRICT_CONVERSION =

# vi: set ft=make :
