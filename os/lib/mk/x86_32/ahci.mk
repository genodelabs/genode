#
# \brief  Toolchain configurations for AHCI on X86 32 bit
# \author Martin Stein <martin.stein@genode-labs.com>
# \date   2013-05-17
#

# add include directories
INC_DIR += $(REP_DIR)/src/drivers/ahci/x86_32

# include less specific config
include $(REP_DIR)/lib/mk/ahci.inc
