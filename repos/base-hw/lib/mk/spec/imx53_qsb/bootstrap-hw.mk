INC_DIR += $(BASE_DIR)/../base-hw/src/bootstrap/spec/imx53_qsb

SRC_S   += bootstrap/spec/arm/crt0.s

SRC_CC  += bootstrap/spec/arm/cortex_a8_mmu.cc
SRC_CC  += bootstrap/spec/arm/cpu.cc
SRC_CC  += bootstrap/spec/arm/imx_tzic.cc
SRC_CC  += hw/spec/arm/arm_v7_cpu.cc
SRC_CC  += hw/spec/32bit/memory_map.cc

ifneq ($(filter-out $(SPECS),trustzone),)
SRC_CC  += bootstrap/spec/imx53_qsb/platform.cc
else
SRC_CC  += bootstrap/spec/imx53_qsb/platform_trustzone.cc
endif

include $(BASE_DIR)/../base-hw/lib/mk/bootstrap-hw.inc
