#
# \brief  Build config for a core that prints hardware information
# \author Martin Stein
# \date   2011-12-16
#

# set target name that this configuration applies to
TARGET = test-hw_info

# add library dependencies
LIBS += core test-hw_info

# add C++ sources
SRC_CC += kernel/test.cc
