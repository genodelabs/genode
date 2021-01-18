#
# \brief  Build config for Genodes core process
# \author Norman Feske
# \date   2013-04-05
#

# add include paths
REP_INC_DIR += src/core/board/rpi

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += spec/arm/bcm2835_pic.cc
SRC_CC += spec/arm/bcm2835_system_timer.cc

# include less specific configuration
include $(call select_from_repositories,lib/mk/spec/arm_v6/core-hw.inc)
