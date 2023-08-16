LIBS      += libc
COMPAT_DIR = $(REP_DIR)/src/lib/compat-libc

SRC_CC = compat.cc

vpath %.cc $(COMPAT_DIR)

CC_CXX_WARN_STRICT_CONVERSION =

# vi: set ft=make :
