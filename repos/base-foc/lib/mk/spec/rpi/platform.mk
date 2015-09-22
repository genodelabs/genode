#
# Configuration for L4 build system (for kernel-bindings, sigma0, bootstrap).
#
L4_CONFIG = $(call select_from_repositories,config/rpi.user)

include $(REP_DIR)/lib/mk/spec/arm/platform.inc
