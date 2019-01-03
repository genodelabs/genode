SRC_CC += timeout.cc
SRC_CC += timer_connection.cc
SRC_CC += timer_connection_time.cc
SRC_CC += hw/timer_connection_timestamp.cc
SRC_CC += duration.cc

LIBS += alarm

INC_DIR += $(BASE_DIR)/src/include

vpath % $(BASE_DIR)/src/lib/timeout
