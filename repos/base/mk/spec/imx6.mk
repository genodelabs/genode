#
# \brief  Build-system configurations for Freescale i.MX6
# \author Nikolay Golikov <nik@ksyslabs.org>
# \author Josef Soentgen
# \author Martin Stein
# \date   2014-02-25
#

# denote wich specs are also fullfilled by this spec
SPECS += cortex_a9 imx

# add repository relative include paths
REP_INC_DIR += include/spec/imx6
REP_INC_DIR += include/spec/imx

# include implied specs
include $(call select_from_repositories,mk/spec/cortex_a9.mk)

