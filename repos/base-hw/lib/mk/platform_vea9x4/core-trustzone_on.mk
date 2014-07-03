#
# \brief  Build config for parts of core that depend on Trustzone status
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/vea9x4/trustzone

# add C++ sources
SRC_CC += vm_session_component.cc

# declare source paths
vpath platform_support.cc     $(REP_DIR)/src/core/vea9x4/trustzone
vpath platform_services.cc    $(REP_DIR)/src/core/vea9x4/trustzone
vpath vm_session_component.cc $(REP_DIR)/src/core

# include less specific library parts
include $(REP_DIR)/lib/mk/platform_vea9x4/core-trustzone.inc
