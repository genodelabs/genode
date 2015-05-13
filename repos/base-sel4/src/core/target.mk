TARGET = core
LIBS  += core
SRC_S  = boot_modules.s

LD_TEXT_ADDR ?= 0x02000000

# XXX hack, based on base-hw/lib/mk/core.mk
ifneq ($(wildcard $(BUILD_BASE_DIR)/boot_modules.s),)
  BOOT_MODULES_VPATH = $(BUILD_BASE_DIR)
  INC_DIR += $(BOOT_MODULES_VPATH)
else
  # use dummy boot-modules by default
  BOOT_MODULES_VPATH = $(REP_DIR)/src/core/
endif
vpath boot_modules.s $(BOOT_MODULES_VPATH)
