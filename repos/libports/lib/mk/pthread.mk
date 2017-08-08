SRC_CC = semaphore.cc rwlock.cc \
         thread.cc thread_create.cc 

LIBS  += libc

INC_DIR += $(REP_DIR)/src/lib

vpath % $(REP_DIR)/src/lib/pthread

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
