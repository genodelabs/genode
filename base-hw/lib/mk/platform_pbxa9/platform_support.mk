#
# \brief   Platform implementations specific for base-hw and PBXA9
# \author  Stefan Kalkowski
# \date    2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core \
           $(REP_DIR)/src/core/include \
           $(BASE_DIR)/src/core/include

# add C++ sources
SRC_CC += platform_services.cc platform_support.cc

# declare source paths
vpath platform_services.cc $(BASE_DIR)/src/core
vpath platform_support.cc  $(REP_DIR)/src/core/pbxa9

