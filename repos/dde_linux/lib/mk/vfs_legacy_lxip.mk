SHARED_LIB = yes

VFS_DIR  = $(REP_DIR)/src/lib/vfs/legacy_lxip
LXIP_DIR = $(REP_DIR)/src/lib/legacy_lxip

LIBS    += legacy_lxip lxip_include
INC_DIR += $(VFS_DIR)
LD_OPT  += --version-script=$(VFS_DIR)/symbol.map
SRC_CC   = vfs.cc

vpath %.cc $(REP_DIR)/src/lib/vfs/legacy_lxip

SETUP_SUFFIX =
CC_OPT += -DSETUP_SUFFIX=$(SETUP_SUFFIX)

CC_OPT += -U__linux__ -D__KERNEL__
CC_OPT += -DCONFIG_INET -DCONFIG_BASE_SMALL=0 -DCONFIG_DEBUG_LOCK_ALLOC \
          -DCONFIG_IP_PNP_DHCP

CC_C_OPT  += -include $(LXIP_DIR)/include/lx_emul.h
CC_CXX_OPT += -fpermissive

CC_CXX_WARN_STRICT =
