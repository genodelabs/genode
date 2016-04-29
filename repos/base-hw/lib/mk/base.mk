include $(BASE_DIR)/lib/mk/base.inc

SRC_CC += thread_start.cc
SRC_CC += env.cc
SRC_CC += capability.cc
SRC_CC += cache.cc

LIBS += startup
