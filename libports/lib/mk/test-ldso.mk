SRC_CC     = test-rtld.cc
SHARED_LIB = yes
INC_DIR   += $(REP_DIR)/src/test/ldso/include

LIBS = test-ldso2

vpath test-rtld.cc $(REP_DIR)/src/test/ldso/lib
