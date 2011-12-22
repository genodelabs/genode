SRC_S   = crt0.s
SRC_CC  = _main.cc
LIBS   += cxx lock

INC_DIR += $(REP_DIR)/src/platform $(BASE_DIR)/src/platform

vpath crt0.s   $(BASE_DIR)/src/platform/x86_32
vpath _main.cc $(BASE_DIR)/src/platform
