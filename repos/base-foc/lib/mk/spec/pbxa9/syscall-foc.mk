L4_CONFIG := $(call select_from_repositories,config/pbxa9.user)

L4_BIN_DIR := $(LIB_CACHE_DIR)/syscall-foc/pbxa9-build/bin/arm_armv7a

include $(REP_DIR)/lib/mk/spec/arm/syscall-foc.inc
