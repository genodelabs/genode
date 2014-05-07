#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

REQUIRES += trustzone

# add include paths
INC_DIR  += $(REP_DIR)/src/core/imx53/trustzone

# add C++ sources
SRC_CC += vm_session_component.cc

# declare source paths
vpath platform_services.cc    $(BASE_DIR)/src/core/imx53/trustzone
vpath platform_support.cc     $(REP_DIR)/src/core/imx53/trustzone
vpath trustzone.cc            $(REP_DIR)/src/core/imx53/trustzone
vpath vm_session_component.cc $(REP_DIR)/src/core

# include less specific target parts
include $(REP_DIR)/src/core/imx53/target.inc

