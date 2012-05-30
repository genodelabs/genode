#
# \brief   Implementation of the Genode thread-API
# \author  Martin Stein
# \date    2012-04-16
#

# add C++ sources
SRC_CC += thread.cc thread_bootstrap.cc thread_support.cc

# declare source paths
vpath thread_support.cc $(REP_DIR)/src/base/
vpath % $(BASE_DIR)/src/base/thread/
