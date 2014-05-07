#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-04
#

# declare wich specs must be given to build this target
REQUIRES += hw_vea9x4
ifeq ($(filter-out $(SPECS),trustzone),)
  REQUIRES += no_trustone
endif

# add include paths
INC_DIR += $(REP_DIR)/src/core/vea9x4/no_trustzone

# declare source paths
vpath platform_support.cc  $(REP_DIR)/src/core/vea9x4/no_trustzone
vpath platform_services.cc $(BASE_DIR)/src/core

# include less specific target parts
include $(REP_DIR)/src/core/vea9x4/target.inc

