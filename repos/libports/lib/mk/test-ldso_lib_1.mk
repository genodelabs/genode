SRC_CC = lib_1.cc
SHARED_LIB = yes
INC_DIR += $(REP_DIR)/src/test/ldso/include
LIBS = test-ldso_lib_2
vpath % $(REP_DIR)/src/test/ldso

CC_CXX_WARN_STRICT =
