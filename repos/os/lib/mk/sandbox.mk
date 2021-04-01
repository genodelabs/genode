SRC_CC   = library.cc child.cc server.cc config_model.cc
INC_DIR += $(REP_DIR)/src/lib/sandbox
LIBS    += base

SHARED_LIB = yes

vpath %.cc $(REP_DIR)/src/lib/sandbox
