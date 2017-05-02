#
# \brief  Build config for Genodes core process
# \author Norman Feske
# \date   2013-04-05
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/spec/rpi

# add C++ sources
SRC_CC += platform_services.cc
SRC_CC += spec/arm/cpu_context.cc
SRC_CC += spec/rpi/timer.cc
SRC_CC += spec/rpi/pic.cc

# include less specific configuration
include $(REP_DIR)/lib/mk/spec/arm_v6/core-hw.inc
