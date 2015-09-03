#
# \brief  Build configurations for 'base-hw' on Freescale i.MX53
# \author Stefan Kalkowski
# \date   2012-10-24
#

# denote wich specs are also fullfilled by this spec
SPECS += hw imx53_qsb imx53

# configure multiprocessor mode
NR_OF_CPUS = 1

# set address where to link the text segment at
LD_TEXT_ADDR ?= 0x70010000

# add repository relative include paths
REP_INC_DIR += include/spec/imx53_qsb

# include implied specs
include $(call select_from_repositories,mk/spec/hw.mk)
include $(call select_from_repositories,mk/spec/imx53.mk)
