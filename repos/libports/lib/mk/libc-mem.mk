SRC_CC = libc_mem_alloc.cc

include $(REP_DIR)/lib/mk/libc-common.inc

INC_DIR += $(REP_DIR)/src/lib/libc

vpath libc_mem_alloc.cc $(REP_DIR)/src/lib/libc

CC_CXX_WARN_STRICT =
