TARGET   = test-python
LIBS     = cxx env python libc libc_log libm
REQUIRES = x86
SRC_CC   = main.cc

CC_OPT_main = -Wno-uninitialized
