LIBS += virt_linux_generated

LX_SRC_DIR := $(call select_from_ports,linux)/src/linux
ifeq ($(wildcard $(LX_SRC_DIR)),)
LX_SRC_DIR := $(call select_from_repositories,src/linux)
endif

LX_GEN_DIR := $(LIB_CACHE_DIR)/virt_linux_generated

-include $(call select_from_repositories,lib/import/import-lx_emul_common.inc)

SRC_CC += lx_kit/memory_non_dma.cc
