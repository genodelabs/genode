SRC_CC = lock.cc cmpxchg.cc

INC_DIR += $(REP_DIR)/include/codezero/dummies
INC_DIR += $(REP_DIR)/src/base/lock

vpath lock.cc   $(BASE_DIR)/src/base/lock
vpath cmpxchg.cc $(REP_DIR)/src/base/lock
