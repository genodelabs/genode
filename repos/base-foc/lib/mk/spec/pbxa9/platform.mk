#
# Configuration for L4 build system (for kernel-bindings, sigma0, bootstrap).
#
L4_CONFIG = $(call select_from_repositories,config/pbxa9.user)

include $(REP_DIR)/lib/mk/spec/arm/platform.inc
