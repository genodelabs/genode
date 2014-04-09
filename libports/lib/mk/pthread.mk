SRC_CC = semaphore.cc \
         thread.cc thread_create.cc 

LIBS  += libc

vpath % $(REP_DIR)/src/lib/pthread

SHARED_LIB = yes
