SRC_CC = plugin.cc

LIBS += libc

REP_INC_DIR += src/lib/libc

INC_DIR += $(BASE_DIR)/src/base/env/

vpath %.cc $(REP_DIR)/src/lib/libc_noux

SHARED_LIB = yes
