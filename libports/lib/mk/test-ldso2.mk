SRC_CC     = test_lib.cc
SHARED_LIB = yes
INC_DIR   += $(REP_DIR)/src/test/ldso/include

vpath test_lib.cc $(REP_DIR)/src/test/ldso/lib
