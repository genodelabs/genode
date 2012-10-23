#
# \brief  Makefile for core
# \author Stefan Kalkowski
# \date   2012-10-04
#

# declare wich specs must be given to build this target
REQUIRES = platform_vea9x4

# adjust link address of a trustzone text segment
ifeq ($(filter-out $(SPECS),trustzone),)
LD_TEXT_ADDR = 0x48000000
endif

# include less specific target parts
include $(REP_DIR)/src/core/target.inc
