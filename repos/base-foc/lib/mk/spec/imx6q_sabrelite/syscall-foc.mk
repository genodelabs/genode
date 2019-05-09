L4_CONFIG := $(call select_from_repositories,config/imx6q_sabrelite.user)

L4_BIN_DIR := $(LIB_CACHE_DIR)/syscall-foc/imx6q_sabrelite-build/bin/arm_armv7a

include $(REP_DIR)/lib/mk/spec/arm/syscall-foc.inc
