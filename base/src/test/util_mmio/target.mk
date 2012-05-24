#
# \brief   Diversified test of the Register and MMIO framework
# \author  Martin Stein
# \date    2012-04-25
#

# Set program name
TARGET = test-util_mmio

# Add C++ sources
SRC_CC += main.cc

# Add libraries
LIBS += cxx env thread
