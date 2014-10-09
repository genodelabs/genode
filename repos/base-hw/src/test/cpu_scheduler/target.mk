#
# \brief  Build config for a core that tests its CPU-scheduler implementation
# \author Martin Stein
# \date   2011-12-16
#

# set target name that this configuration applies to
TARGET = test-cpu_scheduler

# library that provides the whole configuration
LIBS += core

# add C++ sources
SRC_CC += kernel/test.cc
