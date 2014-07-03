#
# \brief  Build config for Genodes core process
# \author Martin Stein
# \date   2011-12-16
#

# add include paths
INC_DIR += $(REP_DIR)/src/core/arm

# declare source paths
vpath cpu_support.cc $(REP_DIR)/src/core/arm
vpath crt0.s         $(REP_DIR)/src/core/arm

# use dummy boot-modules per default
BOOT_MODULES_VPATH = $(REP_DIR)/src/core/arm

# include less specific library parts
include $(REP_DIR)/lib/mk/core.inc
