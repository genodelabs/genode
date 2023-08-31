LXIP_CONTRIB_DIR := $(call select_from_ports,dde_linux)/src/lib/lxip
SRC_DIR          := $(REP_DIR)/src/lib/legacy_lxip

# architecture-dependent includes
ifeq ($(filter-out $(SPECS),x86),)
  ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/x86
  ifeq ($(filter-out $(SPECS),32bit),)
    ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/x86_32
  endif # 32bit
  ifeq ($(filter-out $(SPECS),64bit),)
    ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/x86_64
  endif # 64bit
endif # x86

ifeq ($(filter-out $(SPECS),arm),)
  ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/arm
  ifeq ($(filter-out $(SPECS),arm_v6),)
    ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/arm_v6
  endif # arm_v6
  ifeq ($(filter-out $(SPECS),arm_v7),)
    ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/arm_v7
  endif # arm_v7
endif # arm
ifeq ($(filter-out $(SPECS),arm_v8),)
  ARCH_SRC_INC_DIR += $(REP_DIR)/src/include/spec/arm_v8
endif # arm_v8

#
# The order of include-search directories is important, we need to look into
# 'contrib' before falling back to our custom 'lx_emul.h' header.
#
INC_DIR += $(SRC_DIR) \
           $(SRC_DIR)/include
INC_DIR += $(REP_DIR)/src/include
INC_DIR += $(ARCH_SRC_INC_DIR)
INC_DIR += $(LXIP_CONTRIB_DIR)/include \
           $(LXIP_CONTRIB_DIR)/include/uapi
INC_DIR += $(LIB_CACHE_DIR)/lxip_include/include/include/include
