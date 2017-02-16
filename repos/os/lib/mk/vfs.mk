SRC_CC  += file_system_factory.cc
INC_DIR += $(REP_DIR)/src/lib/vfs

LIBS = ld

vpath %.cc $(REP_DIR)/src/lib/vfs
