#
# \brief   Provide cross-component synchronization
# \author  Martin Stein
# \date    2014-10-13
#

# Set program name
TARGET = test-sync

# Add C++ sources
SRC_CC = main.cc

# Add include paths
INC_DIR += $(PRG_DIR)/../include

# Add libraries
LIBS = base server
