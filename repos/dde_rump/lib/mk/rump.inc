#
# \brief  Basic definitions for all rump libraries
# \author Sebastian Sumpf
# \date   2013-12-12
#

RUMP_CONTRIB_DIR := $(call select_from_ports,dde_rump)/src/lib/dde_rump
RUMP_BASE         = $(BUILD_BASE_DIR)/var/libcache/rump
RUMP_LIB          = $(RUMP_BASE)/lib

SHARED_LIB  = yes

#
# ARCHIVE contains the absolute paths to rump-archive libraries, rump.ld is an
# additional linker script which is required during initialization
#
LD_OPT      += --whole-archive --start-group $(ARCHIVE) --end-group \
               --no-whole-archiv

LD_SCRIPT_SO = $(call select_from_repositories,src/platform/genode_rel.ld) \
               -T$(REP_DIR)/src/platform/rump.ld

# vi:set ft=make :
