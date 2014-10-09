#
# \brief  Build config for a core that tests its double-list implementation
# \author Martin Stein
# \date   2011-12-16
#

# set target name that this configuration applies to
TARGET = test-double_list

# library that provides the whole configuration
LIBS += core

# add C++ sources
SRC_CC += kernel/test.cc
