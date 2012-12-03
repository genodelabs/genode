#
# \brief   Platform implementations specific for base-hw and VEA9X4
# \author  Stefan Kalkowski
# \date    2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core \
           $(REP_DIR)/src/core/include \
           $(BASE_DIR)/src/core/include

# avoid building of this lib with other platforms
REQUIRES += platform_vea9x4

SRC_CC = platform_support.cc platform_services.cc

vpath platform_support.cc  $(REP_DIR)/src/core/vea9x4
vpath platform_services.cc $(BASE_DIR)/src/core
