#
# \brief  Offer build configurations that are specific to base-hw and Zynq/Zedboard
# \author Mark Albers
# \author Johannes Schlatow
# \date   2015-03-12
#

# denote which specs are also fulfilled by this spec
SPECS += hw platform_zedboard

# configure multiprocessor mode
NR_OF_CPUS = 1

# set address where to link the text segment at
LD_TEXT_ADDR ?= 0x00100000

# include implied specs
include $(call select_from_repositories,mk/spec-hw.mk)
include $(call select_from_repositories,mk/spec-platform_zedboard.mk)
