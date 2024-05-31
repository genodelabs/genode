SRC_CC  += device.cc
SRC_CC  += device_component.cc
SRC_CC  += device_pd.cc
SRC_CC  += main.cc
SRC_CC  += pci.cc
SRC_CC  += root.cc
SRC_CC  += session_component.cc
SRC_CC  += shared_irq.cc
SRC_CC  += dma_allocator.cc

GENERIC_DIR := $(dir $(call select_from_repositories,src/driver/platform/target.inc))

INC_DIR += $(PRG_DIR) $(GENERIC_DIR)
LIBS    += base

vpath %.cc $(PRG_DIR)
vpath %.cc $(GENERIC_DIR)
