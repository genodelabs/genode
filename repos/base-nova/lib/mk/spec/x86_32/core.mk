SRC_CC  += pager.cc

INC_DIR  = $(REP_DIR)/src/core/include \
           $(BASE_DIR)/src/core/include \
           $(BASE_DIR)/src/include

vpath %.cc $(REP_DIR)/src/core/spec/x86_32
