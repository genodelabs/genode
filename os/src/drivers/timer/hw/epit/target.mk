#
# \brief   Timer session server
# \author  Stefan Kalkowski
# \date    2012-10-25
#

# set program name
TARGET = timer

# add C++ sources
SRC_CC += main.cc

# skip build if required specs not fullfilled
REQUIRES += hw epit

# add libraries
LIBS += cxx server env alarm

# add include paths
INC_DIR += $(PRG_DIR) $(PRG_DIR)/../ $(PRG_DIR)/../../nova/

# declare source paths
vpath main.cc $(PRG_DIR)/../..

