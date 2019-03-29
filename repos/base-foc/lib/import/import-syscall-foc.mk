L4_INCLUDE_DIR := $(LIB_CACHE_DIR)/syscall-foc/include

ifeq ($(filter-out $(SPECS),x86_32),)
  INC_DIR += $(L4_INCLUDE_DIR)/x86/l4f $(L4_INCLUDE_DIR)/x86
endif # 32bit

ifeq ($(filter-out $(SPECS),x86_64),)
  INC_DIR += $(L4_INCLUDE_DIR)/amd64/l4f $(L4_INCLUDE_DIR)/amd64
endif # 64bit

ifeq ($(filter-out $(SPECS),arm),)
  INC_DIR += $(L4_INCLUDE_DIR)/arm/l4f $(L4_INCLUDE_DIR)/arm
  CC_OPT  += -DARCH_arm
endif # ARM

ifeq ($(filter-out $(SPECS),arm_64),)
  INC_DIR += $(L4_INCLUDE_DIR)/arm64/l4f $(L4_INCLUDE_DIR)/arm64
  CC_OPT  += -DARCH_arm64
endif # ARM


INC_DIR += $(L4_INCLUDE_DIR)/l4f $(L4_INCLUDE_DIR)
CC_OPT += -DCONFIG_L4_CALL_SYSCALLS

#
# Use 'regparm=0' call instead of an inline function, when accessing
# the utcb. This is needed to stay compatible with L4linux
#
CC_OPT += -DL4SYS_USE_UTCB_WRAP=1
