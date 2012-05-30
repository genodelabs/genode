#
# \brief   Generic parts of the core-support lib
# \author  Martin Stein
# \date    2012-04-27
#

# add include paths
INC_DIR += $(BOARD_DIR) $(REP_DIR)/src/core/include $(REP_DIR)/include \
           $(BASE_DIR)/src/core/include $(BASE_DIR)/include

# set entry point of core's first thread
CC_OPT += -DCORE_MAIN=_main

# add C++ sources
SRC_CC += platform_support.cc kernel.cc rm_session_support.cc

# declare source paths
vpath platform_support.cc $(BOARD_DIR)
vpath %                   $(REP_DIR)/src/core

