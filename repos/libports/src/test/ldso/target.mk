SRC_CC   = main.cc
TARGET   = test-ldso
LIBS     = base test-ldso_lib_1 test-ldso_lib_2 libc libm format
INC_DIR += $(REP_DIR)/src/test/ldso/include

CC_CXX_WARN_STRICT =
