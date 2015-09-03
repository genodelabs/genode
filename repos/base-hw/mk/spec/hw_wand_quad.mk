#
# \brief  Build configurations for 'base-hw' on WandBoard Quad
# \author Nikolay Golikov <nik@ksyslabs.org>
# \author Josef Soentgen
# \author Martin Stein
# \date   2014-02-25
#

# denote wich specs are also fullfilled by this spec
SPECS += hw imx6

# configure multiprocessor mode
NR_OF_CPUS = 1

# set address where to link the text segment at
LD_TEXT_ADDR ?= 0x10001000

# include implied specs
include $(call select_from_repositories,mk/spec/hw.mk)
include $(call select_from_repositories,mk/spec/imx6.mk)
