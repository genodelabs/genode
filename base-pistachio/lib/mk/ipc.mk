SRC_CC  = ipc.cc pager.cc
LIBS    = cap_copy

# disable warning about array boundaries, caused by L4 headers
CC_WARN = -Wall -Wno-array-bounds

vpath %.cc $(REP_DIR)/src/base/ipc
