#
# \brief  Build config for a core that prints hardware information
# \author Martin Stein
# \date   2011-12-16
#

# add C++ sources
SRC_CC += spec/arm_v7/info.cc

# decrlare source directories
vpath % $(REP_DIR)/src/test/hw_info
