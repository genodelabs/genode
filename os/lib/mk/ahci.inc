#
# \brief  Generic toolchain configurations for AHCI
# \author Martin Stein <martin.stein@genode-labs.com>
# \date   2013-05-17
#

# add C++ sources
SRC_CC  += main.cc

# add library dependencies
LIBS += base

# add include directories
INC_DIR += $(REP_DIR)/src/drivers/ahci/include

# declare source paths
vpath main.cc $(REP_DIR)/src/drivers/ahci
