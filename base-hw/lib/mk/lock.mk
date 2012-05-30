#
# \brief  Synchronisation through locks
# \author Martin Stein
# \date   2012-04-16
#

# add C++ sources
SRC_CC += lock.cc

# add include paths
INC_DIR += $(REP_DIR)/src/base/lock

# declare source paths
vpath % $(BASE_DIR)/src/base/lock
