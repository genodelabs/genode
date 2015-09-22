#
# Configuration for L4 build system (for kernel-bindings, sigma0, bootstrap).
#
L4_CONFIG = $(call select_from_repositories,config/x86_64.user)

#
# Create mirror for architecture-specific L4sys header files
#
L4_INC_TARGETS = amd64/l4/sys \
                 amd64/l4f/l4/sys \
                 amd64/l4/vcpu

include $(REP_DIR)/lib/mk/platform.inc
