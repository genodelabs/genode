SRC_CC = semaphore.cc rwlock.cc \
         thread.cc thread_create.cc 

LIBS  += libc

vpath % $(REP_DIR)/src/lib/pthread

SHARED_LIB = yes

CC_CXX_WARN_STRICT =
