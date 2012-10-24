#
# \brief   Timer session server
# \author  Martin Stein
# \date    2012-05-03
#

# Set program name
TARGET = timer

# Add C++ sources
SRC_CC += main.cc

# Skip build if required specs not fullfilled
REQUIRES += hw omap4

# Add libraries
LIBS += cxx server env alarm

# Add include paths
INC_DIR += $(PRG_DIR) $(PRG_DIR)/../ $(PRG_DIR)/../../nova/

# Declare source paths
vpath main.cc $(PRG_DIR)/../..
