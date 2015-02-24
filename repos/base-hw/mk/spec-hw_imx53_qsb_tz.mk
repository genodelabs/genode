#
# \brief  Build configurations for 'base-hw' on Freescale i.MX53
# \author Stefan Kalkowski
# \date   2012-10-24
#

# denote wich specs are also fullfilled by this spec
SPECS += hw_imx53_qsb trustzone

# include implied specs
include $(call select_from_repositories,mk/spec-hw_imx53_qsb.mk)
