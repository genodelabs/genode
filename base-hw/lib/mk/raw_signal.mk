#
# \brief  Submit and receive asynchronous events
# \author Martin Stein
# \date   2012-05-07
#

# add C++ sources
SRC_CC += signal.cc

# add library dependencies
LIBS += slab

# declare source paths
vpath % $(REP_DIR)/src/base/signal

