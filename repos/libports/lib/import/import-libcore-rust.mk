TARGET_DIR = src/lib/rust-targets/spec
ifeq ($(filter-out $(SPECS),x86),)
  ifeq ($(filter-out $(SPECS),32bit),)
    CC_RUSTC_OPT += --target $(call select_from_repositories,$(TARGET_DIR)/x86_32/target.json)
  endif # 32bit

  ifeq ($(filter-out $(SPECS),64bit),)
    CC_RUSTC_OPT += --target $(call select_from_repositories,$(TARGET_DIR)/x86_64/target.json)
  endif # 64bit
endif # x86

ifeq ($(filter-out $(SPECS),arm),)
  CC_RUSTC_OPT += --target $(call select_from_repositories,$(TARGET_DIR)/arm/target.json)
endif # ARM

ifeq ($(filter-out $(SPECS),riscv),)
  CC_RUSTC_OPT += --target $(call select_from_repositories,$(TARGET_DIR)/riscv/target.json)
endif # RISCV
