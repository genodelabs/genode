#
# \brief   Test the distribution and application of CPU quota
# \author  Martin Stein
# \date    2014-10-13
#

# Set program name
TARGET = test-cpu_quota

# Add C++ sources
SRC_CC += main.cc

# Add include paths
INC_DIR += $(PRG_DIR)/include

# Add libraries
LIBS += base
