#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# declare wich specs must be given to build this target
REQUIRES += hw_vea9x4 trustzone

# add include paths
INC_DIR += $(REP_DIR)/src/core/vea9x4/trustzone

# adjust link address of a trustzone text segment
LD_TEXT_ADDR = 0x48000000

# add C++ sources
SRC_CC += vm_session_component.cc

# declare source paths
vpath platform_support.cc     $(REP_DIR)/src/core/vea9x4/trustzone
vpath platform_services.cc    $(REP_DIR)/src/core/vea9x4/trustzone
vpath trustzone.cc            $(REP_DIR)/src/core/vea9x4/trustzone
vpath vm_session_component.cc $(REP_DIR)/src/core

# include less specific target parts
include $(REP_DIR)/src/core/vea9x4/target.inc

