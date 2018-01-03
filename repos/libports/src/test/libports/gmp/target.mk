TARGET = test-gmp
LIBS   = libc gmp
SRC_CC = main.cc

# The gmp.lib by now only compiles for 32-bit so we have to mind that
REQUIRES = 32bit

vpath main.cc $(PRG_DIR)/..

CC_CXX_WARN_STRICT =
