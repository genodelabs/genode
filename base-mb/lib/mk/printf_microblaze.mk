SRC_CC   = microblaze_console.cc
LIBS     = cxx console
INC_DIR += $(REP_DIR)/src/platform

vpath %.cc $(REP_DIR)/src/base/console
