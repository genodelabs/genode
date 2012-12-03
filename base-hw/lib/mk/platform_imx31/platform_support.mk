#
# \brief   Platform implementations specific for base-hw and i.MX31
# \author  Martin Stein
# \date    2012-05-10
#

# add include paths
INC_DIR += $(REP_DIR)/src/core \
           $(REP_DIR)/src/core/include \
           $(REP_DIR)/src/core/include/imx31 \
           $(BASE_DIR)/src/core/include

# add C++ sources
SRC_CC += platform_support.cc platform_services.cc

# declare source paths
vpath % $(REP_DIR)/src/core/imx31
vpath platform_services.cc $(BASE_DIR)/src/core

