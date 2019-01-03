SRC_CC += timeout.cc
SRC_CC += timer_connection.cc
SRC_CC += timer_connection_time.cc
SRC_CC += timer_connection_timestamp.cc
SRC_CC += duration.cc

LIBS += alarm

INC_DIR += $(BASE_DIR)/src/include

vpath % $(call select_from_repositories,src/lib/timeout)
