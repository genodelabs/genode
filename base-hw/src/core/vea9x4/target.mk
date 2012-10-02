#
# \brief  VEA9x4-specific makefile for core
# \author Stefan Kalkowski
# \date   2012-10-04
#

REQUIRES = platform_vea9x4

ifeq ($(filter-out $(SPECS),trustzone),)
LD_TEXT_ADDR = 0x48000000
endif

include $(REP_DIR)/src/core/target.inc
