#
# \brief  Toolchain configurations for AHCI on Exynos
# \author Martin Stein <martin.stein@genode-labs.com>
# \date   2013-05-17
#

# add C++ sources
SRC_CC += ahci_driver.cc

# add include directories
INC_DIR += $(REP_DIR)/src/drivers/ahci/exynos5

# declare source paths
vpath ahci_driver.cc $(REP_DIR)/src/drivers/ahci/exynos5

# insert configurations that are less specific
include $(REP_DIR)/lib/mk/ahci.inc
