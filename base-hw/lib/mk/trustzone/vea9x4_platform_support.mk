#
# \brief   Platform implementations specific for base-hw and VEA9X4 (TrustZone)
# \author  Stefan Kalkowski
# \date    2012-10-04
#

# add include paths
INC_DIR += $(REP_DIR)/src/core \
           $(REP_DIR)/src/core/include \
           $(BASE_DIR)/src/core/include

SRC_CC   = platform_services.cc \
           platform_support.cc \
           vm_session_component.cc

vpath platform_support.cc     $(REP_DIR)/src/core/vea9x4/trustzone
vpath platform_services.cc    $(REP_DIR)/src/core/vea9x4/trustzone
vpath vm_session_component.cc $(REP_DIR)/src/core
