#
# \brief  Build config for Genodes core process
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

# add include paths
INC_DIR  += $(REP_DIR)/src/core/imx53/no_trustzone
ifeq ($(filter-out $(SPECS),trustzone),)
  REQUIRES += no_trustzone
endif

# declare source paths
vpath platform_services.cc $(BASE_DIR)/src/core
vpath platform_support.cc  $(REP_DIR)/src/core/imx53/no_trustzone

# include less specific target parts
include $(REP_DIR)/src/core/imx53/target.inc

