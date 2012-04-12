SRC_CC = semaphore.cc \
         thread.cc

LIBS  += libc

vpath % $(REP_DIR)/src/lib/pthread

SHARED_LIB = yes
