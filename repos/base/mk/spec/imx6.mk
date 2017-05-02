#
# \brief  Build-system configurations for Freescale i.MX6
# \author Nikolay Golikov <nik@ksyslabs.org>
# \author Josef Soentgen
# \author Martin Stein
# \date   2014-02-25
#

# denote wich specs are also fullfilled by this spec
SPECS += cortex_a9

# add repository relative include paths
REP_INC_DIR += include/spec/imx6

# include implied specs
include $(BASE_DIR)/mk/spec/cortex_a9.mk

