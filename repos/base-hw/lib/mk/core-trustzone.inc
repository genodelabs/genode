#
# \brief  Build config for parts of core that depend on Trustzone status
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

# add include paths
INC_DIR +=  $(REP_DIR)/src/core
INC_DIR +=  $(REP_DIR)/src/core/include
INC_DIR += $(BASE_DIR)/src/core/include

# add C++ sources
SRC_CC += platform_support.cc
SRC_CC += platform_services.cc
