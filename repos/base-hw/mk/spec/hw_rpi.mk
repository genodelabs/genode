#
# \brief  Build configurations specific to base-hw and Raspberry Pi
# \author Norman Feske
# \date   2013-04-05
#

# denote wich specs are also fullfilled by this spec
SPECS += hw rpi

# configure multiprocessor mode
NR_OF_CPUS = 1

# include implied specs
include $(call select_from_repositories,mk/spec/hw.mk)
include $(call select_from_repositories,mk/spec/rpi.mk)
