#
# \brief  Build config for a core that tests its double-list implementation
# \author Martin Stein
# \date   2011-12-16
#

TARGET  = test-double_list
INC_DIR = $(REP_DIR)/src/core/include
SRC_CC  = test.cc double_list.cc
LIBS    = base

vpath double_list.cc $(REP_DIR)/src/core/kernel
