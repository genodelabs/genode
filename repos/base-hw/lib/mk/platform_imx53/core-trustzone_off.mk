#
# \brief  Build config for parts of core that depend on Trustzone status
# \author Stefan Kalkowski
# \author Martin Stein
# \date   2012-10-24
#

# declare source paths
vpath platform_support.cc   $(REP_DIR)/src/core/imx53
vpath pic.cc                $(REP_DIR)/src/core/imx53
vpath platform_services.cc $(BASE_DIR)/src/core

# include less specific library parts
include $(REP_DIR)/lib/mk/platform_imx53/core-trustzone.inc
