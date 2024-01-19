LIBS += virt_linux_generated

LX_SRC_DIR := $(call select_from_ports,linux)/src/linux
ifeq ($(wildcard $(LX_SRC_DIR)),)
LX_SRC_DIR := $(call select_from_repositories,src/linux)
endif

LX_GEN_DIR := $(LIB_CACHE_DIR)/virt_linux_generated

VIRT_LINUX_INCLUDE_DIR := $(call select_from_repositories,src/include/virt_linux)

INC_DIR  += $(VIRT_LINUX_INCLUDE_DIR)

ifeq ($(filter-out $(SPECS),x86_32),)
	INC_DIR += $(VIRT_LINUX_INCLUDE_DIR)/spec/x86_32
endif
ifeq ($(filter-out $(SPECS),x86_64),)
	INC_DIR += $(VIRT_LINUX_INCLUDE_DIR)/spec/x86_64
endif
ifeq ($(filter-out $(SPECS),arm_v6),)
	INC_DIR += $(VIRT_LINUX_INCLUDE_DIR)/spec/arm_v6
endif
ifeq ($(filter-out $(SPECS),arm_v7),)
	INC_DIR += $(VIRT_LINUX_INCLUDE_DIR)/spec/arm_v7
endif
ifeq ($(filter-out $(SPECS),arm_v8),)
	INC_DIR += $(VIRT_LINUX_INCLUDE_DIR)/spec/arm_v8
endif

-include $(call select_from_repositories,lib/import/import-lx_emul_common.inc)

SRC_C  += lx_emul/shadow/drivers/char/random.c
SRC_C  += lx_emul/shadow/kernel/rcu/srcutree.c
SRC_C  += lx_emul/shadow/mm/vmalloc.c
SRC_C  += lx_emul/virt/common_dummies.c
SRC_CC += lx_emul/virt/irq.cc
SRC_CC += lx_kit/memory_non_dma.cc

ifeq ($(filter-out $(SPECS),x86),)
	SRC_C += lx_emul/virt/spec/x86/dummies_arch.c

	#
	# Align memory allocations to 16 byte (because we allow FPU use in drivers)
	#
	CC_DEF += -DARCH_DMA_MINALIGN=16 -DARCH_SLAB_MINALIGN=1
endif
ifeq ($(filter-out $(SPECS),arm_v6),)
	SRC_C += lx_emul/virt/spec/arm/dummies_arch.c
endif
ifeq ($(filter-out $(SPECS),arm_v7),)
	SRC_C += lx_emul/virt/spec/arm/dummies_arch.c
endif
ifeq ($(filter-out $(SPECS),arm_v8),)
	SRC_C += lx_emul/virt/spec/arm_v8/dummies_arch.c
endif
